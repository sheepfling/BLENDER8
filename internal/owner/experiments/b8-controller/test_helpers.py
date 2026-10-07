"""Check the worked B8's byte arithmetic against a wider host oracle."""

from __future__ import annotations

import json
import os
import runpy
import shutil
import subprocess
from collections.abc import Callable
from pathlib import Path
from typing import Any, cast

import pytest


def test_byte_arithmetic_and_streamed_pixels(tmp_path: Path, pytestconfig: pytest.Config) -> None:
    """Exercise carries, borrows, wrap, saturation and every generated display byte."""
    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("LLVM Clang is required for firmware development")
    root = pytestconfig.rootpath
    candidate = root / "internal/owner/experiments/b8-controller"
    source = tmp_path / "host_oracle.cpp"
    source.write_text(
        r"""
#include "byte_pair.hpp"
#include "pixels.hpp"
#include "font.hpp"
#include <cassert>
#include <algorithm>
using namespace firmware;
bytes::Pair pair(unsigned value) {
    return {static_cast<bytes::Byte>(value), static_cast<bytes::Byte>(value >> 8)};
}
unsigned host(bytes::Pair value) { return (unsigned(value.high) << 8) | value.low; }
int main() {
    const unsigned deltas[]={0,1,255,256,500,2000,30000,65535};
    for(unsigned value=0;value<=65535;++value) {
        for(unsigned delta:deltas) {
            const auto a=pair(value), b=pair(delta);
            assert(host(bytes::add(a,b))==((value+delta)&65535));
            assert(host(bytes::subtract(a,b))==((value-delta)&65535));
            assert(bytes::equal(a,b)==(value==delta));
            assert(bytes::less(a,b)==(value<delta));
            assert(host(bytes::elapsed(a,b))==std::min(value+delta,30000u));
        }
    }
    for(unsigned label=0;label<24;++label)
        for(unsigned address=0;address<64;++address)
            assert(pixel_byte(label,address)==glyphs[label][address]);
}
""",
        encoding="utf-8",
    )
    executable = tmp_path / "host_oracle"
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-DB8_NUMERIC_PROFILE=8",
            "-I",
            str(root / "platform/sdk/include"),
            "-I",
            str(candidate),
            str(source),
            "-o",
            str(executable),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run([str(executable)], check=True, timeout=10)


def test_compact_font_is_current(pytestconfig: pytest.Config) -> None:
    """Keep the compact implementation tied to the same explicit font source."""
    candidate = pytestconfig.rootpath / "internal/owner/experiments/b8-controller"
    module = runpy.run_path(str(candidate / "generate_font.py"))
    generate = cast(Callable[[Path], str], module["compact_header"])
    assert (candidate / "compact_font.hpp").read_text(encoding="utf-8") == generate(
        candidate / "font-source.json"
    )


def test_native_epoch_wrap(pytestconfig: pytest.Config) -> None:
    """Run beyond the full paired-byte tick range, then exercise STOP."""
    root = pytestconfig.rootpath
    name = "b8_emulator.exe" if os.name == "nt" else "b8_emulator"
    executable = Path(os.environ.get("B8_TEACHING_EXE", root / "build/teaching-b8-native" / name))
    if not executable.is_file():
        pytest.skip("Build the worked native B8 first")
    commands = [
        "bounce off",
        "run 2300000",
        "stop 1",
        "run 50000",
        "stop 0",
        "run 50000",
        "speed 1",
        "run 1000000",
        "snapshot",
        *(["run 10000000"] * 7),
        "stop 1",
        "run 50000",
        "quit",
    ]
    result = subprocess.run(
        [str(executable)],
        input="\n".join(commands) + "\n",
        capture_output=True,
        text=True,
        check=True,
        timeout=60,
    )
    replies = cast(list[dict[str, Any]], [json.loads(line) for line in result.stdout.splitlines()])
    assert replies[0]["device"] == "B8"
    assert all(reply["ok"] for reply in replies)
    initial = replies[commands.index("snapshot") + 1]["state"]
    assert initial["drive_enabled"]
    for index, command in enumerate(commands):
        if command == "run 10000000":
            state = replies[index + 1]["state"]
            assert state["drive_enabled"]
            assert state["mcu"]["reset_serial"] == initial["mcu"]["reset_serial"]
    final = replies[commands.index("quit")]["state"]
    assert final["time_us"] > 70_000_000
    assert not final["drive_enabled"]
    assert final["mcu"]["pwm_shadow"] == 0
