"""Seven recipient documents; component papers remain independently authored."""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

COMPONENT_ORDER = ("mcu", "mux", "lcd", "motor", "temperature", "buttons", "oscillator")
ORDER = ("requirements", *COMPONENT_ORDER)
OPTIONAL_ORDER = ("mcu_b16",)
GENERATION_SOURCE_FOLDERS = {1: "g1", 2: "g2", 3: "field"}
GENERATION_RECIPIENT_FOLDERS = {1: "recipient-g1", 2: "recipient-g2", 3: "recipient"}


@dataclass(frozen=True, slots=True)
class Handout:
    key: str
    title: str
    filename: str
    sources: tuple[str, ...]


HANDOUTS = (
    Handout(
        "requirements",
        "01 / Half-A/Labs customer correspondence",
        "01-Customer-Correspondence.pdf",
        ("requirements",),
    ),
    Handout(
        "mcu",
        "02 / Northstar B8 microcontroller user manual",
        "02-B8-Microcontroller.pdf",
        ("mcu",),
    ),
    Handout("mux", "03 / TriAxis MX8-1 multiplexer", "03-MX8-1-Multiplexer.pdf", ("mux",)),
    Handout("lcd", "04 / PixelRiver PX32-16 LCD", "04-PX32-16-LCD.pdf", ("lcd",)),
    Handout(
        "motor_sensing",
        "05 / MD20 motor and AVT10 sensor papers",
        "05-Motor-and-Temperature.pdf",
        ("motor", "temperature"),
    ),
    Handout("buttons", "06 / Kestrel BA-8 button assembly", "06-BA-8-Buttons.pdf", ("buttons",)),
    Handout(
        "oscillator",
        "07 / Meridion XO8 crystal oscillator",
        "07-XO8-Oscillator.pdf",
        ("oscillator",),
    ),
)


def generation_source_path(root: Path, stem: str, depth: int) -> Path:
    """Return the stable source-unit PDF path for a copy generation."""
    if depth not in GENERATION_SOURCE_FOLDERS:
        raise ValueError("Copy generation must be G1, G2 or G3.")
    suffix = "field" if depth == 3 else f"g{depth}"
    parent = root / ("dist" if depth == 3 else "build/docs")
    return parent / GENERATION_SOURCE_FOLDERS[depth] / f"{stem}.{suffix}.pdf"


def generate_controller_tables(root: Path) -> None:
    sources = json.loads((root / "internal/authoring/spec/manual-source-hashes.json").read_text())
    for name, expected in sources.items():
        path = root / "internal/authoring/source_inputs" / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Technical source changed: {name}; review before publishing.")
    groups: dict[str, list[str]] = {"a": [], "b": []}
    for line in (root / "internal/authoring/source_inputs/MCU_B8.md").read_text().splitlines():
        if not line.startswith("| `0x"):
            continue
        address, name, access, reset, _ = [part.strip() for part in line.strip("|").split("|")]
        a = address.strip("`").removeprefix("0x")
        if int(a, 16) in (2, 3, 4) or int(a, 16) >= 0xA0:
            continue
        n = name.strip("`").replace("_", r"\_")
        rv = reset.strip("`").removeprefix("0x")
        group = "a" if int(a, 16) < 0x60 else "b"
        groups[group].append(a + r"h & \texttt{" + n + "} & " + access + " & " + rv + r"h \\")
    if not groups["a"] or not groups["b"]:
        raise ValueError("No usable controller register rows in the controlled source.")
    (root / "generated").mkdir(exist_ok=True)
    for key, lines in groups.items():
        (root / f"generated/b8-registers-{key}.tex").write_text(
            "\\newcommand{\\BEightRows" + key.upper() + "}{%\n" + "\n".join(lines) + "%\n}\n"
        )


def merge_supplier_packet(
    inputs: list[tuple[str, Path]],
    output: Path,
    title: str = "BLENDER-8 / Engineering Correspondence and Reference File / Issue P6",
    author: str = "Rick / Half-A/Labs",
) -> None:
    from pypdf import PdfWriter

    if not inputs:
        raise ValueError("Cannot assemble an empty document.")
    writer = PdfWriter()
    try:
        for section, path in inputs:
            writer.append(str(path), outline_item=section)
        writer.add_metadata(
            {
                "/Title": title,
                "/Author": author,
                "/Subject": "Engineering correspondence and retained component papers",
            }
        )
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("wb") as stream:
            writer.write(stream)
    finally:
        writer.close()


def build_packet(
    root: Path, recipe_dir: Path | None = None, seed: int = 4815, clean_only: bool = False
) -> dict[str, object]:
    """Build eight source units, deliver exactly seven recipient PDFs and a convenience binder."""
    import shutil

    from pypdf import PdfReader

    from .boundaries import audit_boundaries
    from .models import load_catalog
    from .pipeline import compile_document, corrupt_pdf, digest, verify_generation_sequence
    from .recipe import load_recipe

    if seed < 0:
        raise ValueError("Seed must be nonnegative.")
    boundary_report = audit_boundaries(root)
    if not boundary_report["passed"]:
        raise ValueError(f"Document boundary audit failed: {boundary_report['violations']}")
    catalog = {doc.id: doc for doc in load_catalog(root).documents}
    recipe_dir = recipe_dir or root / "internal/authoring/config/manuals"
    clean_sources: dict[str, Path] = {}
    field_sources: dict[str, Path] = {}
    earlier_sources: dict[int, dict[str, Path]] = {1: {}, 2: {}}
    records: list[dict[str, object]] = []
    for key in (*ORDER, *OPTIONAL_ORDER):
        doc = catalog[key]
        clean = compile_document(root, doc)
        clean_sources[key] = clean
        record: dict[str, object] = {
            "source_unit": key,
            "clean": str(clean),
            "source_sha256": digest(clean),
            "pages": doc.expected_pages,
        }
        if not clean_only:
            copies: list[Path] = []
            for depth in (1, 2, 3):
                recipe = load_recipe(
                    root, doc.default_preset, recipe_dir / f"{key}.toml", depth=depth
                )
                copy = generation_source_path(root, doc.output_stem, depth)
                corrupt_pdf(clean, copy, recipe, seed, document_key=key)
                copies.append(copy)
                if depth < 3:
                    earlier_sources[depth][key] = copy
            field = copies[2]
            record.update(
                field=str(field),
                generations={f"G{depth}": str(copy) for depth, copy in enumerate(copies, start=1)},
                verification=verify_generation_sequence(clean, copies),
            )
            field_sources[key] = field
        records.append(record)

    # These directories are managed outputs, never a dump of the source-paper directory.
    clean_dir, field_dir = root / "build/docs/recipient-clean", root / "dist/recipient"
    for folder in (clean_dir, field_dir):
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(parents=True)
    clean_files: list[tuple[str, Path]] = []
    field_files: list[tuple[str, Path]] = []
    handout_records: list[dict[str, object]] = []
    for handout in HANDOUTS:
        clean = clean_dir / handout.filename
        field = field_dir / handout.filename
        if len(handout.sources) == 1:
            shutil.copyfile(clean_sources[handout.sources[0]], clean)
            if not clean_only:
                shutil.copyfile(field_sources[handout.sources[0]], field)
        else:
            title = "MD20 / Motor and case-temperature sensor papers / Issue P4"
            author = "Vortek Motion; ThermaSense Components"
            merge_supplier_packet(
                [(catalog[k].title, clean_sources[k]) for k in handout.sources],
                clean,
                title,
                author,
            )
            if not clean_only:
                merge_supplier_packet(
                    [(catalog[k].title, field_sources[k]) for k in handout.sources],
                    field,
                    title,
                    author,
                )
        pages = len(PdfReader(clean).pages)
        record = {
            "document": handout.key,
            "filename": handout.filename,
            "source_units": list(handout.sources),
            "pages": pages,
            "clean_sha256": digest(clean),
        }
        clean_files.append((handout.title, clean))
        if not clean_only:
            if len(PdfReader(field).pages) != pages:
                raise ValueError(f"Bundled page mismatch: {handout.key}")
            record["field_sha256"] = digest(field)
            field_files.append((handout.title, field))
        handout_records.append(record)
    clean_packet = root / "build/docs/Blender8-Supplier-Manuals-Clean.pdf"
    merge_supplier_packet(clean_files, clean_packet)
    result: dict[str, object] = {
        "publication_issue": "P6",
        "component_interface_revision": "03 (B8); 04 (B16 addendum only); others as marked",
        "recipient_document_count": len(HANDOUTS),
        "optional_source_units": list(OPTIONAL_ORDER),
        "source_units": records,
        "documents": handout_records,
        "clean_packet": str(clean_packet),
        "pages": len(PdfReader(clean_packet).pages),
        "boundary_audit": boundary_report,
    }
    if field_files:
        field_packet = root / "dist/Blender8-Supplier-Manuals-G3.pdf"
        merge_supplier_packet(field_files, field_packet)
        result.update(field_packet=str(field_packet), field_packet_sha256=digest(field_packet))
        generation_packets: dict[str, str] = {"G3": str(field_packet)}
        for depth in (1, 2):
            folder = root / "build/docs" / GENERATION_RECIPIENT_FOLDERS[depth]
            if folder.exists():
                shutil.rmtree(folder)
            folder.mkdir(parents=True)
            handouts: list[tuple[str, Path]] = []
            for index, handout in enumerate(HANDOUTS):
                destination = folder / handout.filename
                inputs = [
                    (catalog[key].title, earlier_sources[depth][key]) for key in handout.sources
                ]
                if len(inputs) == 1:
                    shutil.copyfile(inputs[0][1], destination)
                else:
                    merge_supplier_packet(inputs, destination)
                if len(PdfReader(destination).pages) != handout_records[index]["pages"]:
                    raise ValueError(f"G{depth} handout page mismatch: {handout.key}")
                handout_records[index][f"g{depth}_sha256"] = digest(destination)
                handouts.append((handout.title, destination))
            packet = root / f"build/docs/Blender8-Supplier-Manuals-G{depth}.pdf"
            merge_supplier_packet(handouts, packet)
            generation_packets[f"G{depth}"] = str(packet)
        result["generation_packets"] = generation_packets
    # Remove output locations used before intermediate generations moved under build/docs.
    for name in ("clean", "g1", "g2", "recipient-clean", "recipient-g1", "recipient-g2"):
        legacy_dir = root / "dist" / name
        if legacy_dir.exists():
            shutil.rmtree(legacy_dir)
    for name in ("Clean", "G1", "G2"):
        (root / f"dist/Blender8-Supplier-Manuals-{name}.pdf").unlink(missing_ok=True)
    for name in ("packet-build.json", "Supplier-Manuals-Preview.png"):
        (root / "dist" / name).unlink(missing_ok=True)
    (root / "build/docs/packet-build.json").write_text(json.dumps(result, indent=2) + "\n")
    return result
