"""Check isolated firmware object budgets, excluding emulator and plant code."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def measure(size_tool: str, objects: list[Path]) -> tuple[int, int]:
    """Sum Berkeley text/data/bss rows from LLVM or GNU size."""
    program = ram = 0
    for path in objects:
        output = subprocess.check_output([size_tool, str(path)], text=True, timeout=30)
        rows = [line.split() for line in output.splitlines()]
        values = [row for row in rows if len(row) >= 3 and all(x.isdigit() for x in row[:3])]
        if len(values) != 1:
            raise ValueError(f"Unsupported size output for {path}; use llvm-size")
        text, data, bss = map(int, values[0][:3])
        program += text + data
        ram += data + bss
    return program, ram


def leb(value: int) -> bytes:
    """Encode a nonnegative WebAssembly section length."""
    result = bytearray()
    while value > 127:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def stamp(path: Path, report: bytes) -> None:
    """Replace our custom section without changing executable Wasm sections."""
    data = path.read_bytes()
    name = b"b8.firmware-memory"
    kept = bytearray(data[:8])
    offset = 8
    while offset < len(data):
        start = offset
        kind = data[offset]
        offset += 1
        length = shift = 0
        while True:
            part = data[offset]
            offset += 1
            length |= (part & 127) << shift
            shift += 7
            if part < 128:
                break
        end = offset + length
        if kind != 0 or data[offset:end][: len(name) + 1] != leb(len(name)) + name:
            kept.extend(data[start:end])
        offset = end
    payload = leb(len(name)) + name + report
    path.write_bytes(bytes(kept) + b"\x00" + leb(len(payload)) + payload)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--device", choices=("B8", "B16"), required=True)
    parser.add_argument("--tier", choices=("standard", "plus", "free"), required=True)
    parser.add_argument("--size-tool", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--objects", nargs="+", type=Path, required=True)
    args = parser.parse_args()
    limits = json.loads(args.config.read_text(encoding="utf-8"))["devices"][args.device][args.tier]
    program, static_ram = measure(args.size_tool, args.objects)
    # B16's modeled DMA SRAM is part of its RAM capacity, even if firmware never touches it.
    peripheral_ram = 1024 if args.device == "B16" else 0
    usage = {"program": program, "ram": static_ram + peripheral_ram}
    exceeded = [key for key in usage if limits[key] is not None and usage[key] > limits[key]]
    report = {
        "schema": 1,
        "device": args.device,
        "tier": args.tier,
        "limits": limits,
        "usage": usage,
        "static_ram": static_ram,
        "peripheral_ram": peripheral_ram,
        "measurement": "isolated-object-sections",
        "unaccounted": ["runtime stack", "heap", "external linked helpers"],
        "exceeded": exceeded,
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    if exceeded:
        raise SystemExit("Firmware memory budget exceeded: " + ", ".join(exceeded))


if __name__ == "__main__":
    main()
