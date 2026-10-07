"""Run an owner firmware experiment against correspondence and additional scenarios.

Candidate glyphs are an encoding oracle, not a human legibility approval. This
runner never supplies human approvals and never changes the acceptance suite.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
from typing import Any, cast

from acceptance import load_glyphs, run
from b8client import Engine
from scenario import run_scenario
from wasm_check import wasm_engine


def font_artifacts(source: Path) -> tuple[str, dict[str, list[str]]]:
    """Pack explicit 5 by 7 glyph rows into the documented PX32-16 geometry."""
    font = cast(dict[str, list[str]], json.loads(source.read_text(encoding="utf-8")))
    labels = [str(i) for i in range(8)] + ["P"] + [f"{i}P" for i in range(1, 8)]
    labels += ["CK", "WD", "DM", "TS", "TH", "IF", "IL", "ST"]
    arrays: list[str] = []
    frames: dict[str, list[str]] = {}
    for label in labels:
        pixels = [[0] * 32 for _ in range(16)]
        left = (32 - (12 * len(label) - 2)) // 2
        for index, char in enumerate(label):
            rows = font[char]
            if len(rows) != 7 or any(len(row) != 5 or set(row) - {"0", "1"} for row in rows):
                raise ValueError(f"invalid 5 by 7 glyph: {char}")
            for y, row in enumerate(rows):
                for x, bit in enumerate(row):
                    for dy in range(2):
                        for dx in range(2):
                            pixels[1 + y * 2 + dy][left + index * 12 + x * 2 + dx] = int(bit)
        data = [
            sum(pixels[(address // 32) * 8 + bit][address % 32] << bit for bit in range(8))
            for address in range(64)
        ]
        arrays.append("    {" + ",".join(str(value) for value in data) + "}, // " + label)
        frames[label] = ["".join(str(value) for row in pixels for value in row)]
    header = (
        "#pragma once\n#include <cstdint>\nnamespace firmware {\n"
        "inline constexpr std::uint8_t glyphs[24][64]={\n" + "\n".join(arrays) + "\n};\n}\n"
    )
    return header, frames


def main() -> int:
    """Require explicit experiment and executable locations; save bounded receipts."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experiment", required=True, type=Path)
    parser.add_argument("--exe", type=Path)
    parser.add_argument(
        "--output", type=Path, help="JSON receipt; .gz selects lossless compression"
    )
    parser.add_argument("--platform", type=Path, default=Path.cwd() / "platform")
    parser.add_argument("--wasm-module", type=Path, help="compiled .mjs instead of native --exe")
    parser.add_argument("--generate-font", action="store_true")
    args = parser.parse_args()
    directory = args.experiment.resolve()
    header, glyphs = font_artifacts(directory / "font-source.json")
    if args.generate_font:
        (directory / "font.hpp").write_text(header, encoding="utf-8")
        (directory / "glyphs.json").write_text(
            json.dumps(glyphs, indent=2) + "\n", encoding="utf-8"
        )
        print("Generated font.hpp and candidate glyph encoding oracle; no human approval.")
        return 0
    if (args.exe is None) == (args.wasm_module is None) or args.output is None:
        parser.error("exactly one of --exe/--wasm-module, plus --output, is required")
    if (directory / "font.hpp").read_text(encoding="utf-8") != header:
        parser.error("font.hpp is stale; run --generate-font and rebuild")
    if load_glyphs(directory / "glyphs.json") != glyphs:
        parser.error("glyphs.json is stale; run --generate-font and rebuild")
    module: Path | None = args.wasm_module.resolve() if args.wasm_module else None
    executable = module.with_suffix(".wasm") if module else args.exe.resolve()

    def make_engine(bench: bool = False, legacy: bool = False) -> Engine:
        if module:
            return wasm_engine(
                args.platform.resolve(), module.parent, module.stem, bench=bench, legacy=legacy
            )
        return Engine(executable, bench=bench, legacy=legacy, timeout=30)

    acceptance = run(executable, glyphs=glyphs, engine_factory=make_engine)
    scenarios = [
        run_scenario(executable, path, engine_factory=make_engine)
        for path in sorted((directory / "scenarios").glob("*.json"))
    ]
    for result in scenarios:
        print(f"{result['scenario']}: {'pass' if result['passed'] else 'FAIL'}", flush=True)
    failed = acceptance["counts"]["fail"] != 0 or any(not item["passed"] for item in scenarios)
    report: dict[str, Any] = {
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "execution": "compiled-wasm-node" if module else "native",
        "automated_passed": not failed,
        "accepted": False,
        "glyph_oracle": "Candidate-defined encoding; human legibility review still required",
        "acceptance": acceptance,
        "additional_scenarios": scenarios,
        "limits": "Finite simulation evidence. Human reviews and physical validation outstanding.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output.suffix == ".gz":
        with gzip.open(args.output, "wt", encoding="utf-8") as stream:
            stream.write(serialized)
    else:
        args.output.write_text(serialized, encoding="utf-8")
    print(json.dumps({"automated_passed": not failed, "counts": acceptance["counts"]}))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
