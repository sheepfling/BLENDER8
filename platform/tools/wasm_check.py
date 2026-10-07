"""Compare actual native and compiled-Wasm replies; missing products FAIL, never fake a pass."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import random
import shutil
import subprocess
from pathlib import Path
from typing import Any, cast

from b8client import Engine, EngineError, resolve_executable
from scenario import evaluate, field_value, load_scenario

FLOAT_FIELDS = {
    "rail_v",
    "analog_v",
    "sensor_c",
    "motor.rpm",
    "motor.duty",
    "motor.case_c",
    "motor.loss_w",
    "thermal.nearby_air_c",
    "thermal.food_c",
    "thermal.load_current_equivalent_a",
    "mcu.system_hz",
    "mcu.peripheral_hz",
}


def compare(left: object, right: object, path: str = "") -> None:
    """Exact states/discrete events; narrow tolerance only for continuous double-valued observations."""
    if isinstance(left, dict) and isinstance(right, dict):
        left_map = cast(dict[str, Any], left)
        right_map = cast(dict[str, Any], right)
        if set(left_map) != set(right_map):
            raise AssertionError(f"{path}: different fields")
        for key in left_map:
            compare(left_map[key], right_map[key], f"{path}.{key}" if path else key)
    elif isinstance(left, list) and isinstance(right, list):
        left_list = cast(list[Any], left)
        right_list = cast(list[Any], right)
        if len(left_list) != len(right_list):
            raise AssertionError(f"{path}: different list lengths")
        for i, (a, b) in enumerate(zip(left_list, right_list, strict=True)):
            compare(a, b, f"{path}[{i}]")
    elif any(path == name or path.endswith("." + name) for name in FLOAT_FIELDS):
        if left is None or right is None:
            if left is not right:
                raise AssertionError(f"{path}: availability differs")
        elif (
            type(cast(object, left)) not in (int, float)
            or type(right) not in (int, float)
            or not math.isfinite(cast(float, left))
            or not math.isfinite(cast(float, right))
            or not math.isclose(cast(float, left), cast(float, right), rel_tol=1e-9, abs_tol=1e-7)
        ):
            raise AssertionError(f"{path}: physical values differ: {left} vs {right}")
    elif type(cast(object, left)) is not type(right) or left != right:
        # JSON permits integral-valued doubles to serialize as 0 or 0.0.
        if (
            type(cast(object, left)) in (int, float)
            and type(right) in (int, float)
            and left == right
        ):
            return
        raise AssertionError(f"{path}: exact mismatch: {left!r} vs {right!r}")


def wasm_engine(
    root: Path, web: Path, variant: str, *, bench: bool = False, legacy: bool = False
) -> Engine:
    node = shutil.which("node") or shutil.which("nodejs")
    if not node:
        raise FileNotFoundError("Node is required for compiled Wasm parity tests")
    module = web / (variant + ".mjs")
    binary = module.with_suffix(".wasm")
    if not module.is_file() or not binary.is_file():
        raise FileNotFoundError(
            f"Missing actual Emscripten products: {module}. Run wasm-build --test-fixtures first."
        )
    return Engine(
        Path(node),
        arguments=(str(root / "tools/wasm_node.mjs"), str(module)),
        bench=bench,
        legacy=legacy,
        timeout=30,
    )


def paired(native: Engine, wasm: Engine, command: str) -> dict[str, Any]:
    def result(engine: Engine) -> dict[str, Any]:
        try:
            return engine.command(command)
        except EngineError as error:
            return {"ok": False, "error": str(error)}

    left, right = result(native), result(wasm)
    compare(left, right)
    return left


def conformance(root: Path, native_build: Path, web: Path, spec_path: Path) -> dict[str, Any]:
    spec = load_scenario(spec_path)
    bench, legacy = spec["mode"] == "bench", spec.get("profile") == "legacy02"
    with (
        Engine(
            resolve_executable(native_build, "b8_starter_emulator"), bench=bench, legacy=legacy
        ) as a,
        wasm_engine(root, web, "b8_starter", bench=bench, legacy=legacy) as b,
    ):
        compare(a.hello, b.hello)
        commands = 0
        for event in spec["events"]:
            reply = paired(a, b, f"schedule {event['at_us']} {event['command']}")
            if not reply["ok"]:
                raise AssertionError(reply)
            commands += 1
        state = paired(a, b, "snapshot")["state"]
        for check in sorted(spec["checks"], key=lambda c: c["at_us"]):
            while state["time_us"] < check["at_us"]:
                duration = min(10_000_000, check["at_us"] - state["time_us"])
                state = paired(a, b, f"run {duration}")["state"]
                commands += 1
            if not evaluate(field_value(state, check["field"]), check["op"], check.get("value")):
                raise AssertionError(
                    f"{spec['name']}: independently specified assertion failed: {check}"
                )
        return {
            "name": spec["name"],
            "passed": True,
            "commands": commands,
            "assertions": len(spec["checks"]),
        }


def trajectory(root: Path, native_build: Path, web: Path, seed: int) -> dict[str, Any]:
    rng = random.Random(seed)
    with (
        Engine(resolve_executable(native_build, "b8_starter_emulator"), bench=True) as a,
        wasm_engine(root, web, "b8_starter", bench=True) as b,
    ):
        compare(a.hello, b.hello)
        commands = [
            "run 60000",
            "jar_bounce 0",
            "bounce random 4815",
            "noise 0.002 4815",
            "stop 1",
            "run 1000",
            "stop 0",
            "speed 4",
            "run 6000",
            "write 0x28 1",
            "write 0x29 1",
            "write 0x61 200",
            "write 0x60 1",
            "run 50000",
        ]
        result: dict[str, Any] = {}
        for cmd in commands:
            result = paired(a, b, cmd)
            if not result["ok"]:
                raise AssertionError(result)
        if result["state"]["motor"]["rpm"] <= 0:
            raise AssertionError("physics trajectory precondition did not produce motion")
        for _ in range(128):
            cmd = rng.choice(
                [
                    f"speed {rng.randint(1, 7)}",
                    f"pulse {rng.randint(0, 1)}",
                    f"stop {rng.randint(0, 1)}",
                    f"jar {rng.randint(0, 1)}",
                    f"jam {rng.randint(0, 1)}",
                    f"load {rng.random():.6f}",
                    "sensor ground",
                    "sensor healthy",
                    "clock_failed 1",
                    "clock_failed 0",
                    "voltage 2.7",
                    "voltage auto",
                    "reset",
                    "environment 25 0 0.15",
                ]
            )
            paired(a, b, cmd)
            state = paired(a, b, f"run {rng.randint(1, 4000)}")["state"]
            if state["drive_enabled"] and not (
                state["jar_ok"]
                and state["jar_permit"]
                and state["run_permit"]
                and state["motor_supply"]
            ):
                raise AssertionError("independent drive-permission invariant failed")
        return {"seed": seed, "passed": True, "actions": 128, "precondition_motion": True}


def selected_device_trajectory(root: Path, native_build: Path, web: Path) -> dict[str, Any]:
    """Compare the actual selected device, including B16 transfers and pin-connected motor motion."""
    with (
        Engine(resolve_executable(native_build, "b8_emulator"), bench=True) as native,
        wasm_engine(root, web, "b8_student", bench=True) as wasm,
    ):
        compare(native.hello, wasm.hello)
        device = native.hello["device"]
        commands = [
            "run 60000",
            "jar_bounce 0",
            "bounce off",
            "stop 1",
            "run 1000",
            "stop 0",
            "speed 4",
            "run 6000",
        ]
        if device == "B16":
            commands += [
                "write 0xE8 90",
                "write 0xE8 165",
                "write 0x28 33",
                "write 0xEB 2",
                "write 0xEC 1",
                "write 0xED 1",
                "write 0xE9 1",
                "write 0x1000 69",
                "write 0x1001 23",
                "write 0xD2 0",
                "write 0xD3 16",
                "write 0xD4 0",
                "write 0xD5 17",
                "write 0xD6 2",
                "write 0xD7 0",
                "write 0xD1 3",
                "write 0xD0 1",
                "run 16",
            ]
        else:
            commands += ["write 0x28 1"]
        result: dict[str, Any] = {}
        for command in commands:
            result = paired(native, wasm, command)
            if not result["ok"]:
                raise AssertionError(result)
        if device == "B16" and result["state"]["dma"]["done"] != 2:
            raise AssertionError("B16 memory-copy precondition failed")
        for command in ["write 0x29 1", "write 0x61 200", "write 0x60 1", "run 50000"]:
            result = paired(native, wasm, command)
        if result["state"]["motor"]["rpm"] <= 0:
            raise AssertionError("Selected-device PWM pin did not produce motion")
        return {
            "device": device,
            "passed": True,
            "commands": len(commands) + 4,
            "motion": True,
            "dma": device == "B16",
        }


def run(root: Path, native_build: Path, build: Path, output: Path) -> int:
    web = build / "web"
    # All prerequisites are required, including the hang image. None is silently skipped.
    for stem in ("b8_starter", "b8_probe", "b8_student", "b8_hang"):
        for extension in (".mjs", ".wasm"):
            if not (web / (stem + extension)).is_file():
                raise FileNotFoundError(
                    f"Missing {stem + extension}; use wasm-build --test-fixtures"
                )
    records = [
        conformance(root, native_build, web, p)
        for p in sorted((root / "scenarios/conformance").glob("*.json"))
    ]
    trajectories = [trajectory(root, native_build, web, seed) for seed in (4815, 90210)]
    selected_device = selected_device_trajectory(root, native_build, web)
    probe_commands = [
        "run 100000",
        "speed 4",
        "run 100000",
        "pulse 1",
        "run 60000",
        "stop 1",
        "run 20000",
        "stop 0",
        "jar 0",
        "run 1000",
        "snapshot",
        "run -1",
        "load nan",
        "write 0x29 1",
        "snapshot",
    ]
    with (
        Engine(resolve_executable(native_build, "b8_probe_emulator")) as a,
        wasm_engine(root, web, "b8_probe") as b,
    ):
        compare(a.hello, b.hello)
        for cmd in probe_commands:
            paired(a, b, cmd)
    device = json.loads((build / "site/build-info.json").read_text())["device"]
    env = {**os.environ, "B8_WASM_WEB": str(web.resolve()), "B8_EXPECT_DEVICE": device}
    node = shutil.which("node") or shutil.which("nodejs")
    if not node:
        raise FileNotFoundError("Node required")
    subprocess.run(
        [
            node,
            "--test",
            str(root / "tests/wasm/compiled.test.mjs"),
            str(root / "tests/wasm/view-compiled.test.mjs"),
            str(root / "tests/wasm/upload-compiled.test.mjs"),
        ],
        env=env,
        check=True,
        timeout=90,
    )
    report = {
        "passed": True,
        "engine": "actual compiled C++ WebAssembly",
        "conformance": records,
        "trajectories": trajectories,
        "probe_commands": len(probe_commands),
        "selected_device": selected_device,
        "float_comparison": {"relative": 1e-9, "absolute": 1e-7, "discrete_fields": "exact"},
        "module_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(web.glob("*.wasm"))
        },
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--native-build", required=True, type=Path)
    parser.add_argument("--wasm-build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        raise SystemExit(
            run(
                args.root.resolve(),
                args.native_build.resolve(),
                args.wasm_build.resolve(),
                args.output,
            )
        )
    except (OSError, ValueError, AssertionError, subprocess.SubprocessError) as error:
        print(f"WASM VERIFICATION NOT PASSED: {error}")
        raise SystemExit(2) from error
