from __future__ import annotations

import json
import os
from pathlib import Path

import numpy as np
import pytest
from pydantic import ValidationError
from pypdf import PdfReader, PdfWriter
from pypdf.generic import RectangleObject
from test_foundry import ROOT, specimen, tiny_pdf
from typer.testing import CliRunner

from b8docs.artifact import make_artifact
from b8docs.cli import app
from b8docs.effects.affine import plan_placement, warp
from b8docs.effects.common import rng_for, stream_seed
from b8docs.pipeline import (
    corrupt_pdf,
    digest,
    replay_pdf,
    verify_generation_sequence,
    verify_pair,
)
from b8docs.recipe import Placement, Recipe, deep_merge, load_recipe, recipe_hash


def patch(recipe: Recipe, changes: dict[str, object]) -> Recipe:
    return Recipe.model_validate(deep_merge(recipe.model_dump(), changes))


@pytest.mark.parametrize("name", ["clean", "lab_copy", "shop_copy", "archive_scan", "bad_xerox"])
def test_v2_presets_validate(name: str) -> None:
    config = load_recipe(ROOT, name)
    assert config.schema_version == 2
    assert Recipe.model_validate_json(config.model_dump_json()) == config


@pytest.mark.parametrize(
    "changes",
    [
        {"unknown": True},
        {"generation_depth": -1},
        {"generation_depth": 5},
        {"dpi": 100},
        {"dpi": 900},
        {"dpi": True},
        {"placement": {"rotation_deg": {"min": 2, "max": -2}}},
        {"placement": {"rotation_deg": {"min": 0, "max": 9}}},
        {"placement": {"rotation_deg": {"min": 0, "max": float("nan")}}},
        {"placement": {"scale": {"min": 0, "max": 1}}},
        {"streaks": {"opacity": {"min": 0.8, "max": 1.0}}},
        {"streaks": {"horizontal": {"min": 1.5, "max": 3}}},
        {"repro": {"blur_mm": {"min": -1, "max": 0}}},
        {"paper": {"rgb": [255, 256, -1]}},
        {"handling": {"stamp": "bad\nmultiline"}},
        {"protection": {"regions": [{"x0": 0.6, "y0": 0.1, "x1": 0.5, "y1": 0.9}]}},
    ],
)
def test_invalid_settings_rejected(changes: dict[str, object]) -> None:
    with pytest.raises(ValidationError):
        patch(load_recipe(ROOT, "shop_copy"), changes)


def test_merge_replaces_arrays_and_does_not_mutate() -> None:
    original = {"a": {"b": 1, "c": 2}, "pages": [1, 2]}
    result = deep_merge(original, {"a": {"b": 4}, "pages": [3]})
    assert result == {"a": {"b": 4, "c": 2}, "pages": [3]}
    assert original["pages"] == [1, 2]


def test_custom_file_and_cli_precedence(tmp_path: Path) -> None:
    custom = tmp_path / "custom.toml"
    custom.write_text('extends="lab_copy"\nname="mycopy"\ngeneration_depth=2\n')
    result = load_recipe(
        ROOT, "shop_copy", custom, {"generation_depth": 1}, ("generation_depth=3",), depth=4
    )
    assert result.name == "mycopy" and result.generation_depth == 4
    assert result.copier_id == "engineering-copier-02"


def test_settings_override_cli_typo_rejected() -> None:
    with pytest.raises(ValidationError):
        load_recipe(ROOT, "shop_copy", overrides=("placement.rotatoin_deg.min=1",))


def test_page_specific_patch_does_not_change_other_pages() -> None:
    config = load_recipe(
        ROOT, "bad_xerox", ROOT / "internal/authoring/config/examples/mux-returned-copy.toml"
    )
    assert config.for_page(1).protection.max_delta == 0.12
    assert config.for_page(2).protection.max_delta == 0.075


@pytest.mark.parametrize(
    "rules",
    [
        [{"pages": [0], "patch": {}}],
        [{"pages": [2], "patch": {"made_up": True}}],
        [{"pages": [2, 2], "patch": {}}],
        [{"pages": [2], "patch": {}}, {"pages": [2], "patch": {}}],
        [{"pages": [2], "patch": {"dpi": 200}}],
    ],
)
def test_invalid_page_overrides_rejected(rules: list[dict[str, object]]) -> None:
    with pytest.raises(ValidationError):
        patch(load_recipe(ROOT, "lab_copy"), {"page_overrides": rules})


@pytest.mark.parametrize("seed", range(8))
def test_affine_contains_whole_sheet_and_inverse(seed: int) -> None:
    cfg = load_recipe(ROOT, "bad_xerox").placement
    matrix, record = plan_placement((300, 390), 150, cfg, rng_for(seed))
    corners = np.array(record["corners_px"])
    assert corners[:, 0].min() >= 0 and corners[:, 0].max() <= 300
    assert corners[:, 1].min() >= 0 and corners[:, 1].max() <= 390
    assert np.allclose(np.linalg.inv(matrix) @ matrix, np.eye(3))
    out = warp(specimen(), matrix, (240, 240, 240))
    assert out.size == (300, 390)


def test_placement_has_same_physical_parameters_at_two_dpi() -> None:
    cfg = load_recipe(ROOT, "shop_copy").placement
    a, ra = plan_placement((600, 780), 150, cfg, rng_for(55))
    b, rb = plan_placement((1200, 1560), 300, cfg, rng_for(55))
    assert ra["rotation_deg"] == rb["rotation_deg"]
    assert np.allclose(ra["effective_translation_mm"], rb["effective_translation_mm"])
    assert np.allclose(a[:2, :2], b[:2, :2])
    assert np.allclose(a[:2, 2] * 2, b[:2, 2])


def test_disabled_placement_is_identity() -> None:
    matrix, record = plan_placement((300, 390), 150, Placement(enabled=False), rng_for(1))
    assert np.array_equal(matrix, np.eye(3))
    assert record["enabled"] is False


def test_zero_depth_exact_pixels() -> None:
    img = specimen()
    out, wear = make_artifact(img, load_recipe(ROOT, "clean"), 3, "mux", 0, 150)
    assert out.mode == img.mode and out.tobytes() == img.tobytes()
    assert wear.passes == []


def test_actual_copy_chain_and_depth_prefix() -> None:
    config = load_recipe(ROOT, "bad_xerox")
    _, a = make_artifact(specimen(), patch(config, {"generation_depth": 2}), 3, "mux", 0, 150)
    _, b = make_artifact(specimen(), patch(config, {"generation_depth": 3}), 3, "mux", 0, 150)
    assert len(b.passes) == 3
    assert b.passes[:2] == a.passes
    for earlier, later in zip(b.passes[:-1], b.passes[1:], strict=True):
        assert earlier["output_image_sha256"] == later["input_image_sha256"]


def test_saved_generation_pdfs_extend_the_same_copy_chain(tmp_path: Path) -> None:
    source = tmp_path / "clean.pdf"
    tiny_pdf(source, 1)
    copies = [tmp_path / f"copy.g{depth}.pdf" for depth in (1, 2, 3)]
    for depth, copy in enumerate(copies, start=1):
        corrupt_pdf(source, copy, load_recipe(ROOT, "lab_copy", depth=depth), dpi=150)
    assert verify_generation_sequence(source, copies) == {
        "passed": True,
        "pages": 1,
        "generations": [1, 2, 3],
    }
    assert len({digest(copy) for copy in copies}) == 3
    manifest_path = copies[1].with_suffix(".manifest.json")
    manifest = json.loads(manifest_path.read_text())
    manifest["pages"][0]["passes"][0]["output_image_sha256"] = "wrong"
    manifest_path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError, match="does not extend"):
        verify_generation_sequence(source, copies)


def test_fixed_copier_lines_across_pages() -> None:
    recipe = load_recipe(ROOT, "bad_xerox")
    _, a = make_artifact(specimen(), recipe, 4815, "mux", 0, 150)
    _, b = make_artifact(specimen(), recipe, 4815, "mux", 1, 150)
    assert a.passes[0]["streaks"] == b.passes[0]["streaks"]
    assert a.passes[0]["placement"] != b.passes[0]["placement"]


def test_copier_identity_changes_only_streak_randomness() -> None:
    recipe = load_recipe(ROOT, "bad_xerox")
    _, a = make_artifact(specimen(), recipe, 77, "mux", 0, 150)
    _, b = make_artifact(specimen(), patch(recipe, {"copier_id": "different"}), 77, "mux", 0, 150)
    assert a.passes[0]["seeds"]["placement"] == b.passes[0]["seeds"]["placement"]
    assert a.passes[0]["seeds"]["streaks"] != b.passes[0]["seeds"]["streaks"]


def test_effect_toggle_does_not_shift_other_random_streams() -> None:
    recipe = load_recipe(ROOT, "bad_xerox")
    _, a = make_artifact(specimen(), recipe, 2, "mux", 0, 150)
    _, b = make_artifact(
        specimen(), patch(recipe, {"vignette": {"enabled": False}}), 2, "mux", 0, 150
    )
    assert a.passes[0]["placement"] == b.passes[0]["placement"]
    assert a.passes[0]["streaks"] == b.passes[0]["streaks"]


def test_seed_framing_avoids_ambiguous_identifiers() -> None:
    assert stream_seed(3, "a|b", "c") != stream_seed(3, "a", "b|c")


@pytest.mark.parametrize("depth", [1, 2, 3, 4])
def test_aggregate_protection_holds_after_affine_and_copies(depth: int) -> None:
    recipe = patch(load_recipe(ROOT, "bad_xerox"), {"generation_depth": depth})
    _, wear = make_artifact(specimen(), recipe, 4, "mux", 0, 150)
    assert wear.protection["protected_pixels"] > 1000
    assert wear.protection["max_delta_observed"] <= recipe.protection.max_delta + 1e-6
    for record in wear.passes:
        assert record["protected_max_delta"] <= recipe.protection.max_delta + 1e-6


def test_cumulative_scale_floor_rejects_excessive_shrink() -> None:
    config = patch(
        load_recipe(ROOT, "bad_xerox"),
        {"generation_depth": 4, "placement": {"scale": {"min": 0.9, "max": 0.9}}},
    )
    with pytest.raises(ValueError, match="Cumulative scale"):
        make_artifact(specimen(), config, 1, "mux", 0, 150)


def test_zero_depth_pdf_is_identical_and_searchable(tmp_path: Path) -> None:
    a, b = tmp_path / "clean.pdf", tmp_path / "copy.pdf"
    tiny_pdf(a)
    corrupt_pdf(a, b, load_recipe(ROOT, "clean"), dpi=150)
    assert digest(a) == digest(b)
    assert PdfReader(b).pages[0].extract_text().strip()
    assert verify_pair(a, b, b.with_suffix(".manifest.json"))["byte_identical"]


def test_page_override_beyond_source_fails_before_commit(tmp_path: Path) -> None:
    a, b = tmp_path / "clean.pdf", tmp_path / "copy.pdf"
    tiny_pdf(a, 1)
    recipe = patch(load_recipe(ROOT, "lab_copy"), {"page_overrides": [{"pages": [2], "patch": {}}]})
    with pytest.raises(ValueError, match="beyond"):
        corrupt_pdf(a, b, recipe, dpi=150)
    assert not b.exists()


def test_hardlink_to_clean_is_not_overwritten(tmp_path: Path) -> None:
    a, b = tmp_path / "clean.pdf", tmp_path / "copy.pdf"
    tiny_pdf(a, 1)
    os.link(a, b)
    with pytest.raises(ValueError, match="overwrite"):
        corrupt_pdf(a, b, load_recipe(ROOT, "lab_copy"), dpi=150)


def test_replay_identical_even_when_input_renamed(tmp_path: Path) -> None:
    a, b, c = tmp_path / "source.pdf", tmp_path / "field.pdf", tmp_path / "replay.pdf"
    tiny_pdf(a, 1)
    corrupt_pdf(a, b, load_recipe(ROOT, "lab_copy"), dpi=150)
    renamed = tmp_path / "renamed.pdf"
    renamed.write_bytes(a.read_bytes())
    result = replay_pdf(renamed, b.with_suffix(".manifest.json"), c)
    assert result["byte_identical"] and digest(b) == digest(c)


def test_replay_rejects_changed_source(tmp_path: Path) -> None:
    a, b = tmp_path / "source.pdf", tmp_path / "field.pdf"
    tiny_pdf(a, 1)
    corrupt_pdf(a, b, load_recipe(ROOT, "lab_copy"), dpi=150)
    tiny_pdf(a, 2)
    with pytest.raises(ValueError, match="source hash"):
        replay_pdf(a, b.with_suffix(".manifest.json"), tmp_path / "replay.pdf")


def test_replay_rejects_changed_toolchain(tmp_path: Path) -> None:
    a, b = tmp_path / "source.pdf", tmp_path / "field.pdf"
    tiny_pdf(a, 1)
    corrupt_pdf(a, b, load_recipe(ROOT, "lab_copy"), dpi=150)
    mp = b.with_suffix(".manifest.json")
    data = json.loads(mp.read_text())
    data["tool_versions"]["Pillow"] = "0.0.fake"
    mp.write_text(json.dumps(data))
    with pytest.raises(ValueError, match="Toolchain"):
        replay_pdf(a, mp, tmp_path / "replay.pdf")


def test_manifest_contains_effects_and_no_wall_clock(tmp_path: Path) -> None:
    a, b = tmp_path / "source.pdf", tmp_path / "field.pdf"
    tiny_pdf(a, 1)
    cfg = load_recipe(ROOT, "bad_xerox")
    corrupt_pdf(a, b, cfg, dpi=150)
    data = json.loads(b.with_suffix(".manifest.json").read_text())
    assert data["schema_version"] == 2
    assert len(data["pages"][0]["passes"]) == 3
    assert data["recipe_sha256"] == recipe_hash(patch(cfg, {"dpi": 150}))
    assert "timestamp" not in data


def test_mixed_rotated_and_cropped_page_geometry(tmp_path: Path) -> None:
    a, b = tmp_path / "source.pdf", tmp_path / "field.pdf"
    base = tmp_path / "base.pdf"
    tiny_pdf(base, 2)
    writer = PdfWriter()
    reader = PdfReader(base)
    first = reader.pages[0]
    first.rotate(90)
    writer.add_page(first)
    second = reader.pages[1]
    second.cropbox = RectangleObject((5.0, 5.0, 139.0, 187.0))
    writer.add_page(second)
    with a.open("wb") as output:
        writer.write(output)
    corrupt_pdf(a, b, load_recipe(ROOT, "lab_copy"), dpi=150)
    assert verify_pair(a, b, b.with_suffix(".manifest.json"))["pages"] == 2


def test_cli_plan_outputs_valid_json() -> None:
    result = CliRunner().invoke(
        app,
        [
            "plan",
            "--root",
            str(ROOT),
            "--preset",
            "bad_xerox",
            "--set",
            "placement.rotation_deg={min=-1.2,max=1.2}",
            "--json",
        ],
    )
    assert result.exit_code == 0, result.output
    config = json.loads(result.output)["recipe"]
    assert config["placement"]["rotation_deg"]["max"] == 1.2


def test_cli_error_is_json_and_does_not_render() -> None:
    result = CliRunner().invoke(app, ["plan", "--root", str(ROOT), "--depth", "9", "--json"])
    assert result.exit_code == 1
    assert json.loads(result.output)["ok"] is False


def test_cli_sweep_produces_depth_zero_and_one(tmp_path: Path) -> None:
    a = tmp_path / "source.pdf"
    tiny_pdf(a, 1)
    outdir = tmp_path / "out"
    result = CliRunner().invoke(
        app,
        [
            "sweep",
            str(a),
            str(outdir),
            "--root",
            str(ROOT),
            "--depths",
            "0,1",
            "--dpi",
            "150",
            "--json",
        ],
    )
    assert result.exit_code == 0, result.output
    assert len(json.loads(result.output)["outputs"]) == 2
    assert digest(outdir / "source.g0.pdf") == digest(a)


def test_schema_export_matches_model(tmp_path: Path) -> None:
    output = tmp_path / "recipe.schema.json"
    result = CliRunner().invoke(app, ["schema", "--output", str(output)])
    assert result.exit_code == 0, result.output
    assert json.loads(output.read_text())["additionalProperties"] is False
