"""Firmware memory section accounting and Wasm metadata regression checks."""

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
from pathlib import Path
from types import ModuleType

ROOT = Path.cwd()


def memory_tool() -> ModuleType:
    """Load the platform tool from the explicit test working directory."""
    spec = importlib.util.spec_from_file_location(
        "memory_tool", ROOT / "platform/tools/check_memory.py"
    )
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_stamp_is_replaceable(tmp_path: Path) -> None:
    """Repeated packaging replaces rather than accumulating metadata."""
    tool = memory_tool()
    path = tmp_path / "image.wasm"
    path.write_bytes(b"\x00asm\x01\x00\x00\x00")
    tool.stamp(path, b'{"schema":1}')
    first = path.read_bytes()
    tool.stamp(path, b'{"schema":1}')
    assert path.read_bytes() == first
    assert first.count(b"b8.firmware-memory") == 1


def test_browser_memory_policy(tmp_path: Path) -> None:
    """Strict rejects overages and missing metadata; free preserves numeric scope."""
    tool = memory_tool()
    path = tmp_path / "image.wasm"
    path.write_bytes(b"\x00asm\x01\x00\x00\x00")
    tool.stamp(
        path,
        json.dumps(
            {
                "schema": 1,
                "device": "B8",
                "usage": {"ram": 3000, "program": 100},
                "limits": {"ram": 2048, "program": 16384},
            }
        ).encode(),
    )
    script = """
import fs from 'node:fs';
import {checkMemory} from './platform/webview/image-loader.mjs';
const image=await WebAssembly.compile(fs.readFileSync(process.argv[1]));
const profiles=JSON.parse(fs.readFileSync('platform/memory-profiles.json'));
if(!checkMemory(image,'warn','standard',profiles).warnings.some(w=>w.includes('exceeds')))process.exit(1);
try{checkMemory(image,'strict','standard',profiles);process.exit(2);}catch{}
checkMemory(image,'strict','free',profiles);
const empty=await WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0]));
try{checkMemory(empty,'strict');process.exit(3);}catch{}
"""
    subprocess.run(["node", "--input-type=module", "-e", script, str(path)], check=True)


def test_capacity_failure_and_free(tmp_path: Path) -> None:
    """A real oversized object fails the gate, while free reports its usage."""
    source = tmp_path / "large.cpp"
    source.write_text("unsigned char storage[5000];\n", encoding="utf-8")
    obj = tmp_path / "large.o"
    subprocess.run(["clang++", "-c", str(source), "-o", str(obj)], check=True)
    command = [
        sys.executable,
        "platform/tools/check_memory.py",
        "--config",
        "platform/memory-profiles.json",
        "--device",
        "B8",
        "--size-tool",
        "size",
        "--output",
        str(tmp_path / "report.json"),
        "--objects",
        str(obj),
    ]
    assert subprocess.run([*command, "--tier", "standard"], check=False).returncode != 0
    assert subprocess.run([*command, "--tier", "free"], check=False).returncode == 0
