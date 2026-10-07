"""Detailed manual, exact register coverage, and varied physical document histories."""

from __future__ import annotations

import importlib.util
import json
import re
from pathlib import Path
from typing import Any, cast

from pypdf import PdfReader

from b8docs.recipe import load_recipe

ROOT = Path.cwd().resolve()


def test_all_62_registers_have_detailed_manual_entries() -> None:
    registers = json.loads(
        (ROOT / "internal/authoring/source_inputs/B8-registers-rev03.json").read_text()
    )
    text = (ROOT / "latex/mcu/blender8.tex").read_text().replace(r"\_", "_")
    # Controlled snapshot is the existing model register list, not newly invented addresses.
    rows = cast(
        list[dict[str, Any]], registers["registers"] if isinstance(registers, dict) else registers
    )
    assert len(rows) == 62
    for row in rows:
        assert row["name"] in text
        address = int(row["address"], 16) if isinstance(row["address"], str) else row["address"]
        assert f"{address:04X}h".lower() in text.lower()


def test_mcu_has_32_bookmarked_sections() -> None:
    text = (ROOT / "latex/mcu/blender8.tex").read_text()
    assert len(re.findall(r"\\pdfbookmark\[0\]", text)) == 32
    index = json.loads((ROOT / "internal/authoring/spec/b8-manual-index.json").read_text())
    assert index is not None
    pdf = ROOT / "build/docs/clean/NMD-B8-Controller.pdf"
    assert len(PdfReader(pdf).pages) == 32


def test_generated_printed_examples_are_current() -> None:
    spec = importlib.util.spec_from_file_location(
        "extract_examples", ROOT / "tools/extract_manual_examples.py"
    )
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.render(ROOT) == (ROOT / "platform/examples/manual_snippets.inc").read_text()


def test_no_shared_copier_across_all_suppliers() -> None:
    recipes = [
        load_recipe(ROOT, "bad_xerox", ROOT / f"internal/authoring/config/manuals/{key}.toml")
        for key in ("mcu", "mux", "lcd", "motor", "temperature", "buttons", "oscillator")
    ]
    assert len({r.copier_id for r in recipes}) == 7
    assert all(r.generation_depth == 3 for r in recipes)


def test_coffee_is_exception_not_every_page() -> None:
    counts = {
        "mcu": 32,
        "mux": 3,
        "lcd": 5,
        "motor": 4,
        "temperature": 3,
        "buttons": 3,
        "oscillator": 2,
        "requirements": 10,
    }
    coffee: list[tuple[str, int]] = []
    for key, pages in counts.items():
        recipe = load_recipe(
            ROOT, "bad_xerox", ROOT / f"internal/authoring/config/manuals/{key}.toml"
        )
        for page in range(1, pages + 1):
            if recipe.for_page(page).handling.coffee:
                coffee.append((key, page))
    assert coffee == [("motor", 1), ("buttons", 2), ("requirements", 3), ("requirements", 7)]


def test_coffee_placements_are_not_identical() -> None:
    locations: list[str] = []
    for key, page in [("motor", 1), ("buttons", 2), ("requirements", 3), ("requirements", 7)]:
        recipe = load_recipe(
            ROOT, "bad_xerox", ROOT / f"internal/authoring/config/manuals/{key}.toml"
        )
        settings = recipe.for_page(page).handling.coffee_profile
        locations.append(settings.model_dump_json())
    assert len(set(locations)) == 4


def test_owner_readiness_cannot_imply_jar_or_hwil_is_done() -> None:
    text = " ".join((ROOT / "docs/START-HERE.md").read_text().split())
    assert "chassis 04" in text
    assert "unfinished safe starter" in text
    assert "Physical HWIL" in text and "not implemented" in text
    assert "not" in (ROOT / "docs/INTERFACES-AND-SWIL-HWIL.md").read_text()


def test_source_bundle_excludes_fonts_and_duplicate_binders() -> None:
    spec = importlib.util.spec_from_file_location("project_tool", ROOT / "tools/project.py")
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for relative in (
        "fonts/vendor.ttf",
        "fonts/vendor.otf",
        "build/file.cpp",
        "dist/recipient/02-B8-Microcontroller.pdf",
        "dist/Blender8-Supplier-Manuals-G3.pdf",
        "internal/owner/private/TRUE-REQUIREMENTS.md",
        "internal/owner/requirements-crosswalk.json",
        "internal/owner/experiments/b16-controller/firmware.cpp",
        "internal/owner/experiments/b16-controller/glyphs.json",
        "internal/owner/experiments/b16-controller/REQUIREMENTS-AUDIT.md",
        "internal/history/docs/P4/VALIDATION.md",
    ):
        assert not module.include(ROOT / relative, ROOT)
    for relative in (
        "latex/mcu/blender8.tex",
        "src/b8docs/artifact.py",
        "platform/sim/include/blender8/sim/board.hpp",
        "dist/field/NMD-B8-Controller.field.pdf",
        "dist/field/NMD-B16-Follow-On.field.pdf",
    ):
        assert module.include(ROOT / relative, ROOT)
