"""One entry point for local builds, emulator, scenarios, acceptance and stress. No hosted CI."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import cast

from b8client import resolve_executable


def from_root(path: Path, root: Path) -> Path:
    """Interpret relative CLI paths from the selected platform root."""
    return path.resolve() if path.is_absolute() else (root / path).resolve()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument(
        "--build-dir", type=Path, help="build directory relative to the platform root"
    )
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("doctor", help="check the local firmware development toolchain")
    build = sub.add_parser("build")
    build.add_argument("--firmware", type=Path)
    build.add_argument("--compiler")
    build.add_argument("--sanitize", action="store_true")
    build.add_argument(
        "--device", "--numeric-profile", dest="numeric_profile", choices=("B8", "B16"), default="B8"
    )
    sub.add_parser("test")
    serve = sub.add_parser("serve")
    serve.add_argument("--probe", action="store_true")
    serve.add_argument("--bench", action="store_true")
    serve.add_argument("--port", type=int, default=8088)
    serve.add_argument("--open", action="store_true")
    animated = sub.add_parser("animated", help="run the C++ animated preview natively")
    animated_modes = animated.add_mutually_exclusive_group()
    animated_modes.add_argument("--probe", action="store_true")
    animated_modes.add_argument("--bench", action="store_true")
    animated.add_argument("--port", type=int, default=8088)
    animated.add_argument("--open", action="store_true")
    scenario = sub.add_parser("scenario")
    scenario.add_argument("file", type=Path)
    scenario.add_argument("--output", type=Path)
    stress = sub.add_parser("stress")
    stress.add_argument("--seed", type=int, default=4815)
    stress.add_argument("--episodes", type=int, default=128)
    stress.add_argument("--actions", type=int, default=256)
    accept = sub.add_parser("accept")
    accept.add_argument("--output", type=Path, default=Path("reports/acceptance.json"))
    accept.add_argument("--glyphs", type=Path)
    accept.add_argument("--reviews", type=Path)
    accept.add_argument("--case", action="append", dest="selected")
    wasm_build = sub.add_parser(
        "wasm-build", help="compile the C++ machine and scene with Emscripten"
    )
    wasm_build.add_argument("--firmware", type=Path)
    for command in (build, wasm_build):
        command.add_argument(
            "--memory-profile", choices=("standard", "plus", "free"), default="standard"
        )
    wasm_build.add_argument("--test-fixtures", action="store_true")
    wasm_build.add_argument(
        "--device", "--numeric-profile", dest="numeric_profile", choices=("B8", "B16"), default="B8"
    )
    wasm_serve = sub.add_parser("wasm-serve", help="serve only the compiled static site")
    wasm_modes = wasm_serve.add_mutually_exclusive_group()
    wasm_modes.add_argument("--probe", action="store_true")
    wasm_modes.add_argument("--bench", action="store_true")
    wasm_serve.add_argument("--port", type=int, default=8088)
    wasm_serve.add_argument("--open", action="store_true")
    wasm_serve.add_argument("--site", type=Path)
    wasm_test = sub.add_parser("wasm-test", help="run compiled Wasm and native parity checks")
    wasm_test.add_argument("--native-build", type=Path, default=Path("build"))
    wasm_test.add_argument("--output", type=Path, default=Path("reports/wasm-parity.json"))
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "CMakeLists.txt").is_file():
        parser.error(f"not a B8 platform root: {root}")
    os.environ["PATH"] = os.pathsep.join(
        (str(Path(sys.executable).parent), os.environ.get("PATH", ""))
    )
    if args.action == "doctor":
        compiler = next(
            (path for name in ("c++", "clang++", "g++", "cl") if (path := shutil.which(name))),
            None,
        )
        cmake = shutil.which("cmake")
        print(
            json.dumps(
                {
                    "platform_root": str(root),
                    "python": sys.executable,
                    "cmake": cmake,
                    "cxx": compiler,
                    "numeric_policy_clang": shutil.which("clang++"),
                    "emcmake_optional": shutil.which("emcmake"),
                    "node_optional": shutil.which("node"),
                    "firmware_source": str(root / "firmware"),
                    "native_ready": bool(cmake and compiler),
                },
                indent=2,
            )
        )
        return 0 if cmake and compiler else 1
    default_build = Path("build-wasm" if args.action.startswith("wasm-") else "build")
    if getattr(args, "numeric_profile", "B8") == "B16":
        default_build = Path(str(default_build) + "-b16")
    build_dir = from_root(args.build_dir or default_build, root)
    if args.action in {"build", "wasm-build"}:
        cache = build_dir / "CMakeCache.txt"
        if cache.is_file():
            cached = cache.read_text(encoding="utf-8")
            is_wasm = any(
                "CMAKE_TOOLCHAIN_FILE:" in line and "emscripten" in line.lower()
                for line in cached.splitlines()
            )
            if is_wasm != (args.action == "wasm-build"):
                parser.error("native and Emscripten builds need separate CMake directories")
    if args.action == "wasm-build":
        emcmake = shutil.which("emcmake")
        if emcmake is None:
            raise RuntimeError("Emscripten emcmake is required; see docs/WASM.md")
        environment = os.environ.copy()
        environment["EM_CACHE"] = str(root / ".local/emscripten-cache")
        cmd = [
            emcmake,
            "cmake",
            "-S",
            str(root),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPython3_EXECUTABLE={sys.executable}",
            "-DB8_SANITIZE=OFF",
            "-DBUILD_TESTING=ON",
            f"-DB8_NUMERIC_PROFILE={args.numeric_profile}",
            f"-DB8_MEMORY_PROFILE={args.memory_profile}",
            f"-DB8_FIRMWARE_DIR={from_root(args.firmware or Path('firmware'), root)}",
            f"-DB8_WASM_TEST_FIXTURES={'ON' if args.test_fixtures else 'OFF'}",
        ]
        subprocess.run(cmd, env=environment, check=True)
        subprocess.run(
            ["cmake", "--build", str(build_dir), "--config", "Release", "--parallel", "4"],
            env=environment,
            check=True,
        )
        return 0
    if args.action == "wasm-serve":
        from wasm_server import serve as start_static

        site = from_root(args.site, root) if args.site else build_dir / "site"
        mode = "probe" if args.probe else "bench" if args.bench else "student"
        start_static(site, args.port, mode=mode, open_browser=args.open)
        return 0
    if args.action == "wasm-test":
        if not (build_dir / "CTestTestfile.cmake").is_file():
            raise FileNotFoundError("No Wasm test build; run wasm-build --test-fixtures")
        status = subprocess.call(
            ["ctest", "--test-dir", str(build_dir), "-C", "Release", "--output-on-failure"]
        )
        if status:
            return status
        return subprocess.call(
            [
                sys.executable,
                str(root / "tools/wasm_check.py"),
                "--root",
                str(root),
                "--wasm-build",
                str(build_dir),
                "--native-build",
                str(from_root(args.native_build, root)),
                "--output",
                str(from_root(args.output, root)),
            ]
        )
    if args.action == "build":
        cmd = [
            "cmake",
            "-S",
            str(root),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPython3_EXECUTABLE={sys.executable}",
            f"-DB8_NUMERIC_PROFILE={args.numeric_profile}",
            f"-DB8_MEMORY_PROFILE={args.memory_profile}",
        ]
        if args.firmware:
            cmd.append(f"-DB8_FIRMWARE_DIR={from_root(args.firmware, root)}")
        if args.compiler:
            cmd.append(f"-DCMAKE_CXX_COMPILER={args.compiler}")
        cmd.append(f"-DB8_SANITIZE={'ON' if args.sanitize else 'OFF'}")
        subprocess.run(cmd, check=True)
        subprocess.run(
            ["cmake", "--build", str(build_dir), "--config", "Release", "--parallel", "4"],
            check=True,
        )
        return 0
    if args.action == "test":
        return subprocess.call(
            [
                "ctest",
                "--test-dir",
                str(build_dir),
                "-C",
                "Release",
                "--output-on-failure",
                "--parallel",
                "4",
            ]
        )
    if args.action == "animated":
        from animated_server import serve as start_animated

        mode = "probe" if args.probe else "bench" if args.bench else "student"
        start_animated(build_dir, root, args.port, mode=mode, open_browser=args.open)
        return 0
    if args.action == "stress":
        return subprocess.call(
            [
                str(resolve_executable(build_dir, "b8_stress")),
                "--seed",
                str(args.seed),
                "--episodes",
                str(args.episodes),
                "--actions",
                str(args.actions),
            ]
        )
    exe = resolve_executable(
        build_dir, "b8_probe_emulator" if getattr(args, "probe", False) else "b8_emulator"
    )
    if args.action == "serve":
        from emulator import serve as start

        start(exe, root, args.port, bench=args.bench, open_browser=args.open)
        return 0
    if args.action == "scenario":
        cmd = [
            sys.executable,
            str(root / "tools/scenario.py"),
            str(from_root(args.file, root)),
            "--exe",
            str(exe),
        ]
        if args.output:
            cmd += ["--output", str(from_root(args.output, root))]
        return subprocess.call(cmd)
    cmd = [
        sys.executable,
        str(root / "tools/acceptance.py"),
        "--exe",
        str(exe),
        "--output",
        str(from_root(args.output, root)),
    ]
    if args.glyphs:
        cmd += ["--glyphs", str(from_root(args.glyphs, root))]
    if args.reviews:
        cmd += ["--reviews", str(from_root(args.reviews, root))]
    for case in cast(list[str] | None, args.selected) or []:
        cmd += ["--case", case]
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
