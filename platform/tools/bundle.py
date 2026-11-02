"""Reproducible source-only native-platform ZIP; no binaries, caches, fonts or old archives."""

from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path

VERSION = "0.6.0"
EXCLUDED = {
    ".git",
    ".local",
    ".venv",
    "venv",
    "__pycache__",
    ".pytest_cache",
    ".ruff_cache",
    "node_modules",
    "reports",
}
SUFFIXES = {
    ".pyc",
    ".pyo",
    ".o",
    ".obj",
    ".a",
    ".lib",
    ".so",
    ".dll",
    ".exe",
    ".zip",
    ".ttf",
    ".otf",
    ".woff",
    ".woff2",
    ".ttc",
    ".pfb",
    ".pfm",
}
SOLUTION_ONLY = {"tests/wasm/candidate-view.test.mjs"}


def matches_safe_starter(root: Path) -> bool:
    """Recognize the shipped firmware bytes; other inputs have unknown provenance."""
    firmware = root / "firmware"
    fixture = root / "tests/fixtures/safe-starter"
    if not firmware.is_dir() or not fixture.is_dir():
        return False

    def code_files(directory: Path) -> dict[str, bytes] | None:
        records: dict[str, bytes] = {}
        for path in directory.rglob("*"):
            if path.is_symlink():
                return None
            if path.is_file() and path.name != "README.md":
                records[path.relative_to(directory).as_posix()] = path.read_bytes()
        return records

    source = code_files(firmware)
    expected = code_files(fixture)
    return source is not None and expected is not None and source == expected and bool(source)


def included(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    return (
        path.is_file()
        and not path.is_symlink()
        and path.suffix.lower() not in SUFFIXES
        and rel.as_posix() not in SOLUTION_ONLY
        and path.name not in {"MANIFEST.sha256", "SOURCE-INVENTORY.json", ".DS_Store"}
        and not any(
            x in EXCLUDED or x == "build" or x.startswith("build-") or x.startswith("cmake-build-")
            for x in rel.parts
        )
    )


def bundle(output: Path, root: Path) -> dict[str, object]:
    output = output.resolve()
    if output.is_relative_to(root):
        raise ValueError("write archive outside the source tree")
    files = sorted(
        (p for p in root.rglob("*") if included(p, root)),
        key=lambda p: p.relative_to(root).as_posix(),
    )
    records = [
        {
            "path": p.relative_to(root).as_posix(),
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
            "bytes": p.stat().st_size,
        }
        for p in files
    ]
    manifest = "".join(f"{r['sha256']}  {r['path']}\n" for r in records)
    known_safe_starter = matches_safe_starter(root)
    inventory = {
        "version": VERSION,
        "b8_interface": "03",
        "chassis": "04",
        "files": records,
        "font_files_included": False,
        "firmware_solution": False if known_safe_starter else None,
        "firmware_provenance": "safe_starter_bytes_match" if known_safe_starter else "unknown",
        "physical_hwil_validated": False,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".partial.zip")
    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        payload = [(p.relative_to(root).as_posix(), p.read_bytes()) for p in files]
        payload += [
            ("MANIFEST.sha256", manifest.encode()),
            ("SOURCE-INVENTORY.json", (json.dumps(inventory, indent=2) + "\n").encode()),
        ]
        for relative, data in payload:
            info = zipfile.ZipInfo("HalfALabs-B8/" + relative, date_time=(2026, 10, 7, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    with zipfile.ZipFile(temporary) as z:
        if z.testzip() is not None:
            raise ValueError("ZIP CRC verification failed")
    temporary.replace(output)
    return {
        "path": str(output),
        "bytes": output.stat().st_size,
        "files": len(payload),
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--output", type=Path, help="source ZIP destination")
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "CMakeLists.txt").is_file():
        parser.error(f"not a B8 platform root: {root}")
    output = args.output or root.parent / f"HalfALabs-B8-Platform-v{VERSION}.zip"
    print(json.dumps(bundle(output, root), indent=2))


if __name__ == "__main__":
    main()
