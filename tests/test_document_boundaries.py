from __future__ import annotations

import json
import re
import shutil
from pathlib import Path
from typing import cast

import pytest
from pypdf import PdfReader
from typer.testing import CliRunner

from b8docs.boundaries import audit_boundaries
from b8docs.cli import app
from b8docs.manuals import (
    HANDOUTS,
    OPTIONAL_ORDER,
    ORDER,
    generate_controller_tables,
    generation_source_path,
)
from b8docs.models import load_catalog

ROOT = Path.cwd().resolve()


def test_exactly_seven_recipient_documents_with_unique_source_membership() -> None:
    assert len(HANDOUTS) == 7
    assert len({h.filename for h in HANDOUTS}) == 7
    sources = [s for h in HANDOUTS for s in h.sources]
    assert len(sources) == len(set(sources)) == 8
    assert sorted(sources) == sorted(ORDER)
    assert HANDOUTS[0].sources == ("requirements",)
    assert HANDOUTS[4].sources == ("motor", "temperature")
    assert HANDOUTS[5].sources == ("buttons",)
    assert not set(OPTIONAL_ORDER) & set(sources)


def test_old_board_is_not_a_seventh_document() -> None:
    assert "board" not in {d.id for d in load_catalog(ROOT).documents}
    assert "board" not in ORDER
    assert not (ROOT / "latex/board/board-mux.tex").exists()


def test_published_source_ownership_audit_passes() -> None:
    assert audit_boundaries(ROOT)["passed"] is True


def test_audit_catches_sensor_assigned_to_board_channel(tmp_path: Path) -> None:
    for folder in (
        "latex",
        "internal/authoring/config",
        "internal/authoring/spec",
        "internal/authoring/correspondence",
    ):
        (tmp_path / folder).parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(ROOT / folder, tmp_path / folder)
    p = tmp_path / "latex/temperature/avt10.tex"
    p.write_text(p.read_text() + "\nConnect to B8 AN0.\n")
    result = audit_boundaries(tmp_path)
    assert result["passed"] is False
    violations = cast(list[dict[str, str]], result["violations"])
    assert any(v["source"] == "temperature" for v in violations)


def test_audit_cli_reports_seven() -> None:
    result = CliRunner().invoke(app, ["audit", "--root", str(ROOT), "--json"])
    assert result.exit_code == 0
    assert json.loads(result.output)["recipient_documents"] == 7


def test_all_previous_clauses_have_explicit_private_dispositions() -> None:
    from b8docs.correspondence import load_archive

    archive = load_archive(ROOT)
    rows = json.loads((ROOT / "internal/owner/requirements-crosswalk.json").read_text())["rows"]
    assert {r["prior_id"] for r in rows} == {f"R-{i:03}" for i in range(1, 41)}
    assert len(rows) == 40
    message_ids = {m.id for t in archive.threads for m in t.messages} | {archive.forward.id}
    assert all(set(row["messages"]) <= message_ids for row in rows)
    public_text = (ROOT / "internal/authoring/correspondence/archive.json").read_text()
    assert not re.search(r"R-\d{3}", public_text)
    assert "Firmware team" in public_text and "Half-A/Labs" in public_text
    assert "New Employee" not in public_text
    assert "Jordon" not in public_text and "Jordan" not in public_text


@pytest.mark.parametrize(
    "term",
    [
        "3,000",
        "18,000",
        "750 ms",
        "400 ms",
        "85 degrees",
        "65 degrees",
        "250 ms",
        "100 ms",
        "1.000 MHz",
        "3,906.25 Hz",
    ],
)
def test_confirmed_product_limits_are_in_the_correspondence(term: str) -> None:
    assert term in (ROOT / "internal/authoring/correspondence/archive.json").read_text()


def test_motor_explicitly_does_not_supply_the_product_lockout() -> None:
    text = (ROOT / "latex/motor/md20.tex").read_text()
    assert "No obstruction-history latch is present" in text
    assert "Acceleration resumes; the module has no memory-based lockout" in text
    assert "PWM\\_CTRL" not in text and "restart must" not in text


def test_sensor_has_voltage_and_thermal_interface_not_controller_policy() -> None:
    text = (ROOT / "latex/temperature/avt10.tex").read_text()
    assert "0.500" in text and "0.010" in text and "0.250" in text
    for term in ("B8", "AN0", "65 &", "85 &", "DATA\\_LO"):
        assert term not in text


def test_buttons_have_bounce_but_no_minimum_firmware_actuation_rule() -> None:
    text = (ROOT / "latex/buttons/ba-8.tex").read_text()
    assert "first 5 ms" in text
    assert "no built-in minimum-hold timer" in text
    assert "50 ms" not in text and "controller must" not in text


def test_controlled_source_inputs_are_unchanged() -> None:
    generate_controller_tables(ROOT)


def test_clean_binder_has_seven_top_level_sections() -> None:
    path = ROOT / "build/docs/Blender8-Supplier-Manuals-Clean.pdf"
    if not path.exists():
        pytest.skip("Build the packet first.")
    reader = PdfReader(path)
    assert len([x for x in reader.outline if not isinstance(x, list)]) == 7
    catalog = {doc.id: doc for doc in load_catalog(ROOT).documents}
    assert len(reader.pages) == sum(catalog[key].expected_pages for key in ORDER)


def test_recipient_directory_contains_only_the_seven_handout_pdfs() -> None:
    path = ROOT / "dist/recipient"
    if not any(path.glob("*.pdf")):
        pytest.skip("Build the field packet first.")
    assert sorted(p.name for p in path.iterdir()) == sorted(h.filename for h in HANDOUTS)
    for h in HANDOUTS:
        pdf = PdfReader(path / h.filename)
        assert not any((page.extract_text() or "").strip() for page in pdf.pages)


def test_pristine_and_early_copies_stay_in_build() -> None:
    if not (ROOT / "build/docs/packet-build.json").exists():
        pytest.skip("Build the packet first.")
    catalog = {doc.id: doc for doc in load_catalog(ROOT).documents}
    for key in (*ORDER, *OPTIONAL_ORDER):
        stem = catalog[key].output_stem
        assert (ROOT / f"build/docs/clean/{stem}.pdf").is_file()
        for depth in (1, 2, 3):
            assert generation_source_path(ROOT, stem, depth).is_file()
    for name in ("clean", "g1", "g2", "recipient-clean", "recipient-g1", "recipient-g2"):
        assert not (ROOT / "dist" / name).exists()
    assert (ROOT / "dist/Blender8-Supplier-Manuals-G3.pdf").is_file()


def test_motor_and_sensor_retain_their_own_supplier_authorship() -> None:
    path = ROOT / "build/docs/recipient-clean/05-Motor-and-Temperature.pdf"
    if not path.exists():
        pytest.skip("Build packet first.")
    reader = PdfReader(path)
    assert len(reader.pages) == 7
    assert "VORTEK" in reader.pages[0].extract_text()
    assert "ThermaSense" in reader.pages[4].extract_text()
    assert len([x for x in reader.outline if not isinstance(x, list)]) == 2
