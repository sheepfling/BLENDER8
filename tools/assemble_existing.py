"""Reassemble handouts/binders from verified existing source PDFs, without recompiling."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path
from typing import Any

from pypdf import PdfReader

from b8docs.boundaries import audit_boundaries
from b8docs.manuals import (
    HANDOUTS,
    OPTIONAL_ORDER,
    ORDER,
    generation_source_path,
    merge_supplier_packet,
)
from b8docs.models import load_catalog
from b8docs.pipeline import digest, verify_generation_sequence


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    root = parser.parse_args().root.resolve()
    catalog = {d.id: d for d in load_catalog(root).documents}
    source_records: list[dict[str, Any]] = []
    for key in (*ORDER, *OPTIONAL_ORDER):
        stem = catalog[key].output_stem
        clean = root / f"build/docs/clean/{stem}.pdf"
        copies = [generation_source_path(root, stem, depth) for depth in (1, 2, 3)]
        if len(PdfReader(clean).pages) != catalog[key].expected_pages:
            raise ValueError(f"Stale page count: {key}")
        source_records.append(
            {
                "source_unit": key,
                "pages": catalog[key].expected_pages,
                "clean": str(clean.relative_to(root)),
                "field": str(copies[2].relative_to(root)),
                "generations": {
                    f"G{depth}": str(copy.relative_to(root))
                    for depth, copy in enumerate(copies, start=1)
                },
                "source_sha256": digest(clean),
                "verification": verify_generation_sequence(clean, copies),
            }
        )
    documents: list[dict[str, Any]] = []
    generation_packets: dict[str, str] = {}
    for depth, destination, output in (
        (
            0,
            root / "build/docs/recipient-clean",
            root / "build/docs/Blender8-Supplier-Manuals-Clean.pdf",
        ),
        (1, root / "build/docs/recipient-g1", root / "build/docs/Blender8-Supplier-Manuals-G1.pdf"),
        (2, root / "build/docs/recipient-g2", root / "build/docs/Blender8-Supplier-Manuals-G2.pdf"),
        (3, root / "dist/recipient", root / "dist/Blender8-Supplier-Manuals-G3.pdf"),
    ):
        if destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True)
        packet: list[tuple[str, Path]] = []
        for handout in HANDOUTS:
            inputs: list[tuple[str, Path]] = []
            for key in handout.sources:
                stem = catalog[key].output_stem
                source = (
                    root / f"build/docs/clean/{stem}.pdf"
                    if depth == 0
                    else generation_source_path(root, stem, depth)
                )
                inputs.append((catalog[key].title, source))
            handout_path = destination / handout.filename
            if len(inputs) == 1:
                shutil.copyfile(inputs[0][1], handout_path)
            else:
                merge_supplier_packet(
                    inputs,
                    handout_path,
                    "MD20 / Motor and case-temperature sensor papers",
                    "Vortek Motion; ThermaSense Components",
                )
            packet.append((handout.title, handout_path))
            if depth == 0:
                documents.append(
                    {
                        "document": handout.key,
                        "filename": handout.filename,
                        "source_units": list(handout.sources),
                        "pages": len(PdfReader(handout_path).pages),
                        "clean_sha256": digest(handout_path),
                    }
                )
            elif depth == 3:
                documents[len(packet) - 1]["field_sha256"] = digest(handout_path)
            else:
                documents[len(packet) - 1][f"g{depth}_sha256"] = digest(handout_path)
        merge_supplier_packet(packet, output)
        if depth:
            generation_packets[f"G{depth}"] = str(output.relative_to(root))
    result: dict[str, Any] = {
        "publication_issue": "P6",
        "component_interface_revision": "03 (B8); 04 (B16 addendum only); others as marked",
        "recipient_document_count": len(HANDOUTS),
        "optional_source_units": list(OPTIONAL_ORDER),
        "loose_component_count": 7,
        "source_units": source_records,
        "documents": documents,
        "pages": sum(d["pages"] for d in documents),
        "boundary_audit": audit_boundaries(root),
        "clean_packet": "build/docs/Blender8-Supplier-Manuals-Clean.pdf",
        "field_packet": "dist/Blender8-Supplier-Manuals-G3.pdf",
        "field_packet_sha256": digest(root / "dist/Blender8-Supplier-Manuals-G3.pdf"),
        "generation_packets": generation_packets,
    }
    for name in ("clean", "g1", "g2", "recipient-clean", "recipient-g1", "recipient-g2"):
        legacy_dir = root / "dist" / name
        if legacy_dir.exists():
            shutil.rmtree(legacy_dir)
    for name in ("Clean", "G1", "G2"):
        (root / f"dist/Blender8-Supplier-Manuals-{name}.pdf").unlink(missing_ok=True)
    for name in ("packet-build.json", "Supplier-Manuals-Preview.png"):
        (root / "dist" / name).unlink(missing_ok=True)
    (root / "build/docs/packet-build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"pages": result["pages"], "documents": len(documents)}))


if __name__ == "__main__":
    main()
