"""Release-time regression: refresh manifests ONLY when prior PDF bytes reproduce.

The explicit allowance for engine changes is for this release test, not normal replay.
A differing PDF is a hard failure and is never promoted to the release directory.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any, cast

from b8docs.pipeline import replay_pdf


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    parser.add_argument("pdfs", nargs="+", help="field PDF paths relative to dist")
    args = parser.parse_args()
    root = args.root.resolve()
    report_path = root / "dist/full-size-replay-results.json"
    records: list[dict[str, Any]] = (
        cast(list[dict[str, Any]], json.loads(report_path.read_text()))
        if report_path.exists()
        else []
    )
    for relative in args.pdfs:
        target = root / "dist" / relative
        manifest_path = target.with_suffix(".manifest.json")
        manifest = json.loads(manifest_path.read_text())
        source = root / "build/docs/clean" / Path(manifest["source_filename"]).name
        with TemporaryDirectory(prefix="b8-release-check-") as folder:
            rebuilt = Path(folder) / target.name
            result = replay_pdf(source, manifest_path, rebuilt, allow_toolchain_change=True)
            if not result["byte_identical"]:
                raise RuntimeError(f"Pixel/PDF regression for {relative}; original kept.")
            target.write_bytes(rebuilt.read_bytes())
            manifest_path.write_bytes(rebuilt.with_suffix(".manifest.json").read_bytes())
        entry = {
            "file": relative,
            "byte_identical": True,
            "expected_sha256": manifest["output_sha256"],
            "toolchain_changes_during_release_test": result["toolchain_changes"],
        }
        records = [item for item in records if item["file"] != relative] + [entry]
        report_path.write_text(json.dumps(records, indent=2, sort_keys=True) + "\n")
        print(f"PASS: {relative}", flush=True)


if __name__ == "__main__":
    main()
