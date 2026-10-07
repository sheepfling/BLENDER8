"""Build and explore the worked B16 controller on the separate teaching branch."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path


def run(arguments: Sequence[str], root: Path, extra_env: dict[str, str] | None = None) -> int:
    """Execute an argument list from the explicit checkout with its activated Python tools."""
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join(
        (str(Path(sys.executable).parent), environment.get("PATH", ""))
    )
    if extra_env:
        environment.update(extra_env)
    print("+ " + " ".join(arguments), flush=True)
    return subprocess.run(arguments, cwd=root, env=environment, check=False).returncode


def main() -> int:
    """Select the worked firmware explicitly and keep its build products separate."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command",
        choices=(
            "native",
            "test",
            "wasm",
            "verify-native",
            "verify-wasm",
            "check-wasm",
            "serve",
            "font",
        ),
    )
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--port", type=int, default=8094)
    args = parser.parse_args()
    root = args.root.resolve()
    candidate = root / "internal/owner/experiments/b16-controller"
    if not (root / "TEACHING-SOLUTION.md").is_file() or not (candidate / "firmware.cpp").is_file():
        parser.error("select the teaching solution checkout with --root")
    python = sys.executable
    native = root / "build/teaching-b16-native"
    wasm = root / "build/teaching-b16-wasm"
    build = wasm if args.command == "wasm" else native
    driver = [python, str(root / "platform/tools/b8.py"), "--root", str(root / "platform")]
    if args.command in {"native", "wasm"}:
        command = [
            *driver,
            "--build-dir",
            str(build),
            "build" if args.command == "native" else "wasm-build",
        ]
        command += [
            "--device",
            "B16",
            "--firmware",
            str(candidate),
            "--watchdog",
            "forced",
            "--memory-profile",
            "standard",
        ]
        if args.command == "wasm":
            command += ["--test-fixtures"]
    elif args.command == "test":
        command = [*driver, "--build-dir", str(native), "test"]
    elif args.command in {"verify-native", "verify-wasm", "font"}:
        command = [
            python,
            str(root / "platform/tools/experiment.py"),
            "--experiment",
            str(candidate),
        ]
        if args.command == "font":
            command += ["--generate-font"]
        else:
            backend = args.command.removeprefix("verify-")
            command += [
                "--platform",
                str(root / "platform"),
                "--output",
                str(root / f"reports/teaching-b16-{backend}.json.gz"),
            ]
            executable = native / ("b8_emulator.exe" if os.name == "nt" else "b8_emulator")
            command += (
                ["--exe", str(executable)]
                if backend == "native"
                else ["--wasm-module", str(wasm / "web/b8_student.mjs")]
            )
    elif args.command == "serve":
        command = [
            python,
            str(root / "platform/tools/wasm_server.py"),
            str(wasm / "site"),
            "--port",
            str(args.port),
            "--mode",
            "student",
        ]
    else:
        node = shutil.which("node")
        if node is None:
            parser.error("Node is required for compiled Wasm checks")
        return run(
            [node, "--test", str(root / "platform/tests/wasm/candidate-view.test.mjs")],
            root,
            {"B8_CANDIDATE_WEB": str(wasm / "web")},
        )
    return run(command, root)


if __name__ == "__main__":
    raise SystemExit(main())
