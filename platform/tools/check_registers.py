"""Verify canonical register manifest matches the public enum exactly."""

import argparse
import json
import re
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    root = parser.parse_args().root.resolve()
    manifest = json.loads((root / "spec/registers.json").read_text())
    expected = {item["name"]: item["address"] for item in manifest["registers"]}
    text = (root / "sdk/include/blender8/registers.hpp").read_text()
    actual = {
        name: int(value, 16) for name, value in re.findall(r"(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", text)
    }
    if expected != actual:
        raise RuntimeError("Register manifest and public header disagree")
    print(f"{len(expected)} register addresses match the canonical manifest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
