from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pytest
from PIL import Image, ImageDraw
from pydantic import ValidationError
from pypdf import PdfReader
from reportlab.pdfgen.canvas import Canvas
from typer.testing import CliRunner

from b8docs.artifact import derive_seed, make_artifact, protect, protected_mask
from b8docs.cli import app
from b8docs.effects.affine import plan_placement, warp
from b8docs.effects.common import rng_for
from b8docs.models import MuxSpec, Region, load_catalog, load_mux, load_recipe
from b8docs.pipeline import corrupt_pdf, digest, generate_values, verify_pair, within
from b8docs.recipe import Placement, Span

ROOT = Path.cwd().resolve()
RECIPES = ("lab_copy", "shop_copy", "archive_scan", "bad_xerox")


def specimen() -> Image.Image:
    image = Image.new("RGB", (300, 390), "white")
    draw = ImageDraw.Draw(image)
    draw.text((40, 55), "MX8-1   S0 + 2 S1 + 4 S2", fill="black")
    draw.text((40, 75), "2 us / 0.2.0 / NOT A REAL PART", fill="black")
    draw.rectangle((40, 110, 260, 270), outline="black", width=2)
    return image


def tiny_pdf(path: Path, count: int = 2) -> None:
    canvas = Canvas(str(path), pagesize=(144, 192), invariant=1)
    for i in range(count):
        canvas.setFont("Helvetica", 10)
        canvas.drawString(14, 160, f"MX8-1 page {i + 1}")
        canvas.drawString(14, 135, "Settling: 2 us")
        canvas.showPage()
    canvas.save()


def test_baseline_matches_source() -> None:
    spec = load_mux(ROOT)
    assert spec.behavior_revision == "0.2.0"
    assert (spec.input_count, spec.select_count, spec.settling_us) == (8, 3, 2)
    assert spec.source_sha256 == digest(ROOT / spec.source_file)


@pytest.mark.parametrize("i", range(8))
def test_generated_truth_table(i: int) -> None:
    generate_values(ROOT)
    contents = (ROOT / "generated/mx8-1-truth.tex").read_text()
    assert f"{(i >> 2) & 1} & {(i >> 1) & 1} & {i & 1} & $X_{{{i}}}$" in contents


@pytest.mark.parametrize(
    "field,value",
    [
        ("input_count", 7),
        ("select_count", 4),
        ("settling_us", 3),
        ("part", "74HC151"),
        ("behavior_revision", "0.3.0"),
    ],
)
def test_behavior_drift_requires_deliberate_schema_revision(field: str, value: object) -> None:
    data = load_mux(ROOT).model_dump()
    data[field] = value
    with pytest.raises(ValidationError):
        MuxSpec.model_validate(data)


@pytest.mark.parametrize("recipe_name", RECIPES)
def test_repeatable_pixels_and_wear(recipe_name: str) -> None:
    recipe = load_recipe(ROOT, recipe_name)
    a, ma = make_artifact(specimen(), recipe, 4815, "mux", 0, 150)
    b, mb = make_artifact(specimen(), recipe, 4815, "mux", 0, 150)
    assert a.tobytes() == b.tobytes()
    assert ma == mb
    assert a.size == (300, 390)


def test_new_seed_changes_pixels() -> None:
    recipe = load_recipe(ROOT, "shop_copy")
    a, _ = make_artifact(specimen(), recipe, 1, "mux", 0, 150)
    b, _ = make_artifact(specimen(), recipe, 2, "mux", 0, 150)
    assert a.tobytes() != b.tobytes()


def test_different_pages_have_different_seeds() -> None:
    assert derive_seed(4815, "mux", 0) != derive_seed(4815, "mux", 1)
    assert derive_seed(4815, "mux", 0) != derive_seed(4815, "board", 0)


def test_source_image_untouched() -> None:
    image = specimen()
    original = image.tobytes()
    make_artifact(image, load_recipe(ROOT, "shop_copy"), 4815, "mux", 0, 150)
    assert image.tobytes() == original


@pytest.mark.parametrize("recipe_name", RECIPES)
def test_strong_marks_are_capped_in_protected_regions(recipe_name: str) -> None:
    recipe = load_recipe(ROOT, recipe_name)
    mask = protected_mask((300, 390), recipe)
    baseline = Image.new("RGB", (300, 390), "white")
    damaged = Image.new("RGB", (300, 390), "black")
    result, observed = protect(
        damaged, baseline, Image.fromarray((mask * 255).astype(np.uint8)), recipe
    )
    assert observed <= recipe.protection.max_delta + 1e-6
    assert np.asarray(result)[0, 0, 0] == 0


@pytest.mark.parametrize(
    "box", [(0.2, 0.3, 0.1, 0.9), (0.1, 0.5, 0.2, 0.5), (-0.1, 0.1, 0.9, 0.9), (0.1, 0.1, 1.1, 0.9)]
)
def test_bad_protected_rectangle_rejected(box: tuple[float, float, float, float]) -> None:
    with pytest.raises(ValidationError):
        Region(x0=box[0], y0=box[1], x1=box[2], y1=box[3])


@pytest.mark.parametrize("dpi", [0, 149, 451])
def test_bad_dpi_rejected(dpi: int) -> None:
    with pytest.raises(ValueError):
        make_artifact(specimen(), load_recipe(ROOT, "shop_copy"), 4815, "mux", 0, dpi)


def test_whole_sheet_fit_preserves_all_corners() -> None:
    image = Image.new("RGB", (300, 390), "white")
    draw = ImageDraw.Draw(image)
    for x, y, c in [
        (0, 0, (255, 0, 0)),
        (280, 0, (0, 255, 0)),
        (0, 370, (0, 0, 255)),
        (280, 370, (0, 0, 0)),
    ]:
        draw.rectangle((x, y, x + 19, y + 19), fill=c)
    matrix, _ = plan_placement(
        image.size, 150, Placement(rotation_deg=Span(min=0.88, max=0.88)), rng_for(3)
    )
    result = np.asarray(warp(image, matrix, (230, 230, 230)))
    for color in [(255, 0, 0), (0, 255, 0), (0, 0, 255), (0, 0, 0)]:
        assert np.sum(np.all(result == color, axis=2)) > 20


@pytest.mark.parametrize("recipe_name", RECIPES)
def test_pdf_output_is_image_only_and_reproducible(tmp_path: Path, recipe_name: str) -> None:
    source = tmp_path / "clean.pdf"
    a, b = tmp_path / "a.pdf", tmp_path / "b.pdf"
    tiny_pdf(source)
    before = digest(source)
    recipe = load_recipe(ROOT, recipe_name)
    corrupt_pdf(source, a, recipe, 4815, 150, "test")
    corrupt_pdf(source, b, recipe, 4815, 150, "test")
    assert digest(source) == before
    assert digest(a) == digest(b)
    result = verify_pair(source, a, a.with_suffix(".manifest.json"))
    assert result["pages"] == 2 and result["field_text_characters"] == 0
    assert all((p.extract_text() or "") == "" for p in PdfReader(a).pages)
    metadata = PdfReader(a).metadata
    assert metadata is not None and metadata.subject is not None
    assert "service-file reproduction" in metadata.subject


def test_refuses_canonical_overwrite(tmp_path: Path) -> None:
    source = tmp_path / "clean.pdf"
    tiny_pdf(source)
    with pytest.raises(ValueError, match="overwrite"):
        corrupt_pdf(source, source, load_recipe(ROOT, "lab_copy"))


def test_manifest_detects_change(tmp_path: Path) -> None:
    source, field = tmp_path / "clean.pdf", tmp_path / "field.pdf"
    tiny_pdf(source)
    corrupt_pdf(source, field, load_recipe(ROOT, "lab_copy"), dpi=150)
    manifest = field.with_suffix(".manifest.json")
    data = json.loads(manifest.read_text())
    data["source_sha256"] = "0" * 64
    manifest.write_text(json.dumps(data))
    with pytest.raises(ValueError, match="hashes"):
        verify_pair(source, field, manifest)


def test_path_escape_rejected() -> None:
    with pytest.raises(ValueError):
        within(ROOT, "../outside.tex")


def test_unknown_recipe_rejected() -> None:
    with pytest.raises(ValueError):
        load_recipe(ROOT, "unsafe_random")


def test_catalog_assets_exist() -> None:
    for doc in load_catalog(ROOT).documents:
        assert (ROOT / doc.source).is_file()
        assert (ROOT / f"latex/vendors/vendor-{doc.vendor}.sty").is_file()


def test_cli_rejects_unknown_document() -> None:
    result = CliRunner().invoke(app, ["build", "--root", str(ROOT), "--doc", "unrecognized"])
    assert result.exit_code == 1
    assert "Unknown document" in result.output


def test_negative_seed_rejected() -> None:
    with pytest.raises(ValueError):
        derive_seed(-1, "mux", 0)
