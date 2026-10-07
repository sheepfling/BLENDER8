"""Assemble an allowlisted, self-contained static Wasm workbench after Emscripten linking."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from check_memory import stamp

UI_FILES = (
    "core-client.mjs",
    "worker-host.mjs",
    "b8-worker.mjs",
    "transport.mjs",
    "workbench.mjs",
    "wasm-main.mjs",
)
MODULES = ("b8_student", "b8_probe", "b8_starter")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def assemble(
    source: Path, build: Path, compiler: str, output: Path | None = None, device: str = "B8"
) -> dict[str, object]:
    source, build = source.resolve(), build.resolve()
    output = (output or build / "site").resolve()
    if (
        output == source
        or source.is_relative_to(output)
        or output == build
        or output == build / "web"
    ):
        raise ValueError("site output must not replace source or compiled output directories")
    if output.exists() and any(output.iterdir()) and not (output / "build-info.json").is_file():
        raise ValueError("refusing to replace an unrecognized existing directory")
    modules = [build / "web" / (name + ext) for name in MODULES for ext in (".mjs", ".wasm")]
    for path in modules:
        if not path.is_file():
            raise FileNotFoundError(
                f"Missing compiled asset: {path}. Complete the Emscripten build first."
            )
        if path.suffix == ".wasm" and path.read_bytes()[:8] != b"\x00asm\x01\x00\x00\x00":
            raise ValueError(f"Not a WebAssembly 1 binary: {path}")
    report = build / "firmware-memory.json"
    if report.is_file():
        stamp(build / "web/b8_student.wasm", report.read_bytes())
    compiler_version = subprocess.check_output(
        [compiler, "--version"], text=True, timeout=10
    ).splitlines()[0]
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="b8-site-", dir=output.parent) as temporary:
        stage = Path(temporary) / "site"
        stage.mkdir()
        shutil.copyfile(source / "memory-profiles.json", stage / "memory-profiles.json")
        for name in UI_FILES:
            shutil.copyfile(source / "ui" / name, stage / name)
        shutil.copyfile(source / "ui/wasm.html", stage / "diagnostics.html")
        shutil.copyfile(source / "webview/animated.html", stage / "index.html")
        for name in ("scene-shell.mjs", "scene-worker.mjs", "view-client.mjs", "image-loader.mjs"):
            shutil.copyfile(source / "webview" / name, stage / name)
        for path in modules:
            shutil.copyfile(path, stage / path.name)
        records = [
            {"path": path.name, "sha256": digest(path), "bytes": path.stat().st_size}
            for path in sorted(stage.iterdir())
        ]
        data: dict[str, object] = {
            "schema": 1,
            "platform": "0.6.0",
            "b8_interface": "03" if device == "B8" else "04",
            "device": device,
            "chassis": "04",
            "compiler": compiler_version,
            "backend": "wasm",
            "bridge_abi": 1,
            "scene_abi": 1,
            "renderer": "cpp-rgba-offscreen",
            "firmware": {
                "student": "Selected at compile time; the distributed default is unfinished",
                "probe": "Pixel bring-up, motor off; not product firmware",
            },
            "files": records,
        }
        (stage / "build-info.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        if output.exists():
            shutil.rmtree(output)
        shutil.move(str(stage), output)
    return {"site": str(output), "manifest": data}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--device", choices=("B8", "B16"), default="B8")
    args = parser.parse_args()
    print(
        json.dumps(
            assemble(args.source, args.build, args.compiler, args.output, args.device), indent=2
        )
    )


if __name__ == "__main__":
    main()
