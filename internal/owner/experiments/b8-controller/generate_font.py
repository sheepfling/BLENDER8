"""Generate the compact B8 font from the same explicit rows as its pixel oracle."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import cast


def compact_header(source: Path) -> str:
    """Store twenty five-column glyphs and a pair of glyph indices per label."""
    rows = cast(dict[str, list[str]], json.loads(source.read_text(encoding="utf-8")))
    characters = "01234567PCKWDMTSHIFL"
    labels = list("01234567P") + [f"{i}P" for i in range(1, 8)]
    labels += ["CK", "WD", "DM", "TS", "TH", "IF", "IL", "ST"]
    columns: list[str] = []
    for char in characters:
        glyph = rows[char]
        if len(glyph) != 7 or any(len(row) != 5 or set(row) - {"0", "1"} for row in glyph):
            raise ValueError(f"invalid 5 by 7 glyph: {char}")
        data = [sum(int(glyph[y][x]) << y for y in range(7)) for x in range(5)]
        columns.append("    {" + ",".join(map(str, data)) + "}, // " + char)
    indices = [
        "    {"
        + ",".join(str(characters.index(char)) for char in label.ljust(2, "0"))
        + "}, // "
        + label
        for label in labels
    ]
    return (
        "#pragma once\n#include <cstdint>\nnamespace firmware {\n"
        "inline constexpr std::uint8_t columns[20][5]={\n"
        + "\n".join(columns)
        + "\n};\ninline constexpr std::uint8_t letters[24][2]={\n"
        + "\n".join(indices)
        + "\n};\n}\n"
    )


def main() -> None:
    """Generate into the explicitly selected experiment directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experiment", type=Path, required=True)
    args = parser.parse_args()
    root = args.experiment.resolve()
    (root / "compact_font.hpp").write_text(
        compact_header(root / "font-source.json"), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
