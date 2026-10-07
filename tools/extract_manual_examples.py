"""Compile-check the C++ examples printed in MCU-001, without a second editable copy."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def render(root: Path) -> str:
    text = (root / "latex/mcu/blender8.tex").read_text(encoding="utf-8")
    blocks = re.findall(r"\\begin\{verbatim\}(.*?)\\end\{verbatim\}", text, re.S)
    if len(blocks) != 9:
        raise ValueError("MCU-001 example count changed; review wrapper mappings.")
    snippets = [b.strip() for b in blocks]
    wrappers = [
        "void acknowledge_timer0() {\n" + snippets[0] + "\n}",
        "void configure_pad5() {\n" + snippets[1] + "\n}",
        snippets[2],
        "void configure_timer1(std::uint8_t prescale_code, std::uint16_t compare) {\n"
        + snippets[3]
        + "\n}",
        snippets[4],
        "std::uint8_t read_external(std::uint8_t local_index) {\n"
        + snippets[5]
        + "\nreturn value;\n}",
        "std::optional<std::uint16_t> collect_adc() {\n"
        + snippets[6].replace(
            "// Consume this completion once; conversion policy is external.",
            "return code; // Test wrapper consumes the printed local result.",
        )
        + "\nreturn std::nullopt;\n}",
        "void commit_clock() {\n" + snippets[7] + "\n}",
    ]
    return (
        "// Generated from MCU-001 examples by tools/extract_manual_examples.py.\n"
        "// The final API declaration block is already compiled via the real SDK.\n"
        '#include "blender8/device.hpp"\n#include <optional>\n\nnamespace nmd_examples {\n'
        + "\n\n".join(wrappers)
        + "\n} // namespace nmd_examples\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    args = parser.parse_args()
    root = args.root.resolve()
    output = root / "platform/examples/manual_snippets.inc"
    result = render(root)
    if args.check:
        if not output.is_file() or output.read_text(encoding="utf-8") != result:
            raise SystemExit("Printed MCU examples differ from generated compile checks.")
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(result, encoding="utf-8")
    print(output.relative_to(root))


if __name__ == "__main__":
    main()
