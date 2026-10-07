"""Local dependency-boundary check. Not a security sandbox."""

import argparse
import re
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--firmware", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "CMakeLists.txt").is_file():
        parser.error(f"not a B8 platform root: {root}")
    firmware = args.firmware or root / "firmware"
    if not firmware.is_absolute():
        firmware = root / firmware
    errors: list[str] = []
    for path in firmware.rglob("*"):
        if path.suffix not in {".cpp", ".hpp", ".h"}:
            continue
        text = path.read_text(encoding="utf-8")
        for include in re.findall(r'#\s*include\s*[<"]([^>"]+)', text):
            if (
                "sim/" in include
                or "host/" in include
                or Path(include).is_absolute()
                or ".." in include
                or include in {"thread", "chrono", "random", "filesystem"}
            ):
                errors.append(f"{path}: forbidden dependency {include}")
        if re.search(r"\bb8::sim\b", text):
            errors.append(f"{path}: simulator namespace leaked")
    for path in (root / "sdk/include").rglob("*.hpp"):
        if any(
            bad in path.read_text(encoding="utf-8") for bad in ("blender8/sim", "blender8/host")
        ):
            errors.append(f"{path.name}: public SDK leaked simulator dependency")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Firmware and SDK dependency boundaries passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
