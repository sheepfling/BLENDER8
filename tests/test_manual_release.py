from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pytest
from PIL import Image, ImageDraw
from pydantic import ValidationError
from pypdf import PdfReader
from pypdf.generic import DictionaryObject
from test_foundry import specimen, tiny_pdf

from b8docs.artifact import make_artifact
from b8docs.effects.coffee import apply_coffee
from b8docs.effects.common import rng_for
from b8docs.effects.tears import apply_tears
from b8docs.manuals import COMPONENT_ORDER, OPTIONAL_ORDER, ORDER, generate_controller_tables
from b8docs.models import load_catalog
from b8docs.pipeline import corrupt_pdf, digest, verify_pair
from b8docs.recipe import Coffee, CountSpan, Span, Tears, load_recipe

ROOT = Path.cwd().resolve()


@pytest.mark.parametrize(
    "kwargs",
    [
        {"depth_mm": {"min": 2, "max": 13}},
        {"length_mm": {"min": 0, "max": 4}},
        {"edges": []},
        {"edges": ["left", "left"]},
        {"roughness": 0.9},
    ],
)
def test_bad_tear_recipe_rejected(kwargs: dict[str, object]) -> None:
    with pytest.raises(ValidationError):
        Tears.model_validate(kwargs)


@pytest.mark.parametrize(
    "kwargs",
    [
        {"radius_mm": {"min": 0, "max": 20}},
        {"center_x": {"min": 2, "max": 3}},
        {"broken_rim": 1.0},
        {"rim_width_mm": 0},
        {"droplets": {"min": 3, "max": 1}},
    ],
)
def test_bad_coffee_recipe_rejected(kwargs: dict[str, object]) -> None:
    with pytest.raises(ValidationError):
        Coffee.model_validate(kwargs)


def test_tear_leaves_protected_interior_unchanged() -> None:
    image = Image.new("RGB", (650, 840), (250, 249, 246))
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).rectangle((60, 60, 590, 780), fill=255)
    original = image.tobytes()
    cfg = Tears(enabled=True, count=CountSpan(min=3, max=3))
    out, rec = apply_tears(image, cfg, 150, rng_for(881), mask, (210, 209, 206))
    assert len(rec["applied"]) == 3
    assert rec["skipped"] == 0
    assert np.array_equal(
        np.asarray(image)[np.asarray(mask) > 0], np.asarray(out)[np.asarray(mask) > 0]
    )
    assert image.tobytes() == original
    assert out.tobytes() != original


def test_no_safe_tear_location_is_reported_not_forced() -> None:
    image = Image.new("RGB", (250, 330), "white")
    cfg = Tears(enabled=True, count=CountSpan(min=2, max=2))
    out, rec = apply_tears(
        image, cfg, 150, rng_for(61), Image.new("L", image.size, 255), (200, 200, 200)
    )
    assert not rec["applied"]
    assert rec["skipped"] == 2
    assert out.tobytes() == image.tobytes()


def test_disabled_tears_are_exact_noop() -> None:
    image = specimen()
    out, _ = apply_tears(
        image, Tears(), 150, rng_for(1), Image.new("L", image.size), (200, 200, 200)
    )
    assert out.tobytes() == image.tobytes()


def test_tear_reproduces_polygons_and_pixels() -> None:
    image = Image.new("RGB", (500, 660), "white")
    args = (image, Tears(enabled=True), 150)
    a, ra = apply_tears(*args, rng_for(42), Image.new("L", image.size), (220, 220, 220))
    b, rb = apply_tears(*args, rng_for(42), Image.new("L", image.size), (220, 220, 220))
    assert ra == rb and a.tobytes() == b.tobytes()


def test_coffee_reproducible_and_dimensions_recorded() -> None:
    image = Image.new("RGB", (600, 780), "white")
    cfg = Coffee(radius_mm=Span(min=21, max=21))
    a, ra = apply_coffee(image, cfg, 0.4, False, 150, rng_for(41))
    b, rb = apply_coffee(image, cfg, 0.4, False, 150, rng_for(41))
    assert a.tobytes() == b.tobytes() and ra == rb
    assert ra["radius_mm"] == 21
    assert a.tobytes() != image.tobytes()


@pytest.mark.parametrize("stage", ["before_copy", "after_copy"])
def test_tear_stage_is_executed_and_recorded(stage: str) -> None:
    recipe = load_recipe(
        ROOT,
        "bad_xerox",
        overrides=(
            "tears.enabled=true",
            f'tears.stage="{stage}"',
            "tears.count.min=1",
            "tears.count.max=1",
            "protection.regions=[]",
            "handling.coffee=true",
        ),
    )
    _, rec = make_artifact(specimen(), recipe, 18, "sample", 0, 150)
    assert rec.physical_damage["tears"]["stage"] == stage
    assert len(rec.physical_damage["tears"]["applied"]) == 1
    assert "coffee" in rec.physical_damage


def test_new_coffee_does_not_reseed_placement() -> None:
    recipe = load_recipe(ROOT, "bad_xerox", overrides=("handling.coffee=true",))
    plain = load_recipe(ROOT, "bad_xerox", overrides=("handling.coffee=false",))
    _, a = make_artifact(specimen(), recipe, 18, "sample", 0, 150)
    _, b = make_artifact(specimen(), plain, 18, "sample", 0, 150)
    assert [x["placement"] for x in a.passes] == [x["placement"] for x in b.passes]


def test_jpeg_output_repeatable_and_manifest_explicit(tmp_path: Path) -> None:
    source, a, b = [tmp_path / name for name in ("source.pdf", "a.pdf", "b.pdf")]
    tiny_pdf(source, 1)
    recipe = load_recipe(
        ROOT,
        "bad_xerox",
        overrides=('output.encoding="jpeg"', "output.jpeg_quality=96", "tears.enabled=true"),
    )
    corrupt_pdf(source, a, recipe, dpi=150, document_key="test")
    corrupt_pdf(source, b, recipe, dpi=150, document_key="test")
    assert digest(a) == digest(b)
    rec = json.loads(a.with_suffix(".manifest.json").read_text())
    assert "lossy" in rec["pixel_hash_scope"]
    assert verify_pair(source, a, a.with_suffix(".manifest.json"))["passed"]
    reference = PdfReader(a).pages[0].images[0].indirect_reference
    assert reference is not None
    image_object = reference.get_object()
    assert isinstance(image_object, DictionaryObject)
    assert "/DCTDecode" in str(image_object["/Filter"])


@pytest.mark.parametrize("document", (*ORDER, *OPTIONAL_ORDER))
def test_all_manual_recipes_are_generation_three(document: str) -> None:
    cfg = load_recipe(
        ROOT, "bad_xerox", ROOT / f"internal/authoring/config/manuals/{document}.toml"
    )
    assert cfg.generation_depth == 3
    assert cfg.protection.enabled
    assert cfg.output.jpeg_quality == 96


@pytest.mark.parametrize("document", (*COMPONENT_ORDER, *OPTIONAL_ORDER))
def test_supplier_sources_are_in_world(document: str) -> None:
    doc = next(x for x in load_catalog(ROOT).documents if x.id == document)
    text = (ROOT / doc.source).read_text().casefold()
    for forbidden in (
        "teaching",
        "fictional",
        "hypothetical",
        "instructor",
        "student",
        "simulator",
        "jordan",
    ):
        assert forbidden not in text


def test_register_tables_come_from_hash_checked_source() -> None:
    generate_controller_tables(ROOT)
    a = (ROOT / "generated/b8-registers-a.tex").read_text()
    b = (ROOT / "generated/b8-registers-b.tex").read_text()
    assert "0000h" in a and "0055h" in a
    assert "0095h" in b and r"ADC\_PRESCALE} & RW & 02h" in b
    assert "0095h" not in a


@pytest.mark.parametrize("document", (*ORDER, *OPTIONAL_ORDER))
def test_built_supplier_pdf_language_and_page_count(document: str) -> None:
    doc = next(x for x in load_catalog(ROOT).documents if x.id == document)
    path = ROOT / f"build/docs/clean/{doc.output_stem}.pdf"
    if not path.exists():
        pytest.skip("Compile packet for rendered-document checks.")
    reader = PdfReader(path)
    assert len(reader.pages) == doc.expected_pages
    text = " ".join((p.extract_text() or "") for p in reader.pages).casefold()
    text += str(reader.metadata).casefold()
    for forbidden in (
        "teaching",
        "fictional",
        "hypothetical",
        "student",
        "simulator",
        "instructor",
    ):
        assert forbidden not in text


def test_fast_mask_dilation_matches_reference() -> None:
    from b8docs.effects.tears import dilate_mask

    a = np.zeros((100, 140), dtype=np.uint8)
    a[30:62, 20:110] = 255
    image = Image.fromarray(a)
    for radius in (0, 1, 3, 12):
        # Independent brute-force square dilation, including radius zero.
        padded = np.pad(a > 0, radius, mode="constant")
        expected = np.zeros_like(a, dtype=bool)
        for dy in range(2 * radius + 1):
            for dx in range(2 * radius + 1):
                expected |= padded[dy : dy + a.shape[0], dx : dx + a.shape[1]]
        assert np.array_equal(dilate_mask(image, radius), expected)
