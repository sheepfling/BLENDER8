"""Build, test and package this source checkout; no external project paths are required."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from collections.abc import Sequence
from pathlib import Path

VERSION = "0.7.0"
EXCLUDED_DIRS = {
    ".git",
    ".pytest_cache",
    ".ruff_cache",
    "__pycache__",
    ".venv",
    "venv",
    "build",
    "node_modules",
    ".mypy_cache",
    ".pyright",
    ".cache",
    ".local",
    "INTAKE",
    "reports",
    "reference-platform",
}
EXCLUDED_SUFFIXES = {
    ".pyc",
    ".pyo",
    ".ttf",
    ".otf",
    ".ttc",
    ".afm",
    ".tfm",
    ".dfont",
    ".t1",
    ".woff",
    ".woff2",
    ".pfb",
    ".pfm",
    ".fdb_latexmk",
    ".fls",
    ".aux",
    ".toc",
    ".out",
    ".synctex.gz",
    ".o",
    ".obj",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args: Sequence[str], root: Path, extra_env: dict[str, str] | None = None) -> None:
    print("+ " + " ".join(args), flush=True)
    env = os.environ.copy()
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    if extra_env:
        env.update(extra_env)
    subprocess.run(list(args), cwd=root, env=env, check=True)


def include(path: Path, root: Path) -> bool:
    relative = path.relative_to(root)
    if relative.parts[:2] in {("internal", "owner"), ("internal", "history")}:
        return False
    if any(
        part in EXCLUDED_DIRS or part.endswith(".egg-info") or part.startswith(".b8-foundry-")
        for part in relative.parts
    ):
        return False
    if path.suffix.lower() in EXCLUDED_SUFFIXES or path.is_symlink():
        return False
    if relative.parts[0] == "dist":
        # Keep independent sources once; generated binders and recipient folders duplicate them.
        if len(relative.parts) < 2:
            return False
        if relative.parts[1] == "field":
            allowed = {
                "NMD-B8-Controller",
                "NMD-B16-Follow-On",
                "MX8-1",
                "PRD-PX32-16",
                "VM-MD20",
                "TSC-AVT10",
                "KAS-BA-8",
                "MFP-XO8-33",
                "HAL-Customer-Correspondence",
            }
            return any(
                path.name in {f"{stem}.field.pdf", f"{stem}.field.manifest.json"}
                for stem in allowed
            )
        return False
    return (
        path.name not in {"CONTENTS.sha256", "ARCHIVE-INVENTORY.json", ".DS_Store"}
        and path.suffix != ".zip"
    )


def bundle(destination: Path, root: Path) -> dict[str, object]:
    from pypdf import PdfReader

    from b8docs.manuals import OPTIONAL_ORDER, ORDER
    from b8docs.models import load_catalog
    from b8docs.pipeline import verify_pair

    catalog = {d.id: d for d in load_catalog(root).documents}
    for key in (*ORDER, *OPTIONAL_ORDER):
        stem = catalog[key].output_stem
        clean = root / f"build/docs/clean/{stem}.pdf"
        field = root / f"dist/field/{stem}.field.pdf"
        if len(PdfReader(clean).pages) != catalog[key].expected_pages:
            raise ValueError(f"Clean page count is stale: {key}")
        verify_pair(clean, field, field.with_suffix(".manifest.json"))
    source_files = sorted(
        (
            p
            for p in (root / "platform").rglob("*")
            if p.is_file() and p.suffix in {".hpp", ".cpp", ".inc"}
        ),
        key=lambda p: str(p.relative_to(root)),
    )
    inventory = {
        "platform_behavior": "0.6.0 / B8 interface03 / chassis04",
        "physical_hwil_adapter": "not implemented",
        "device_models": {"B8": "interface03", "B16": "interface04 behavioral model"},
        "jar_interlock_chassis04": "implemented in logical model",
        "files": [
            {"path": p.relative_to(root).as_posix(), "sha256": sha256(p)} for p in source_files
        ],
    }
    inventory_path = "interfaces/source-inventory.json"
    inventory_bytes = (json.dumps(inventory, indent=2) + "\n").encode()
    paths = sorted(
        (p for p in root.rglob("*") if p.is_file() and include(p, root)),
        key=lambda p: str(p.relative_to(root)),
    )
    records = [
        {"path": p.relative_to(root).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
        for p in paths
    ]
    records.append(
        {
            "path": inventory_path,
            "bytes": len(inventory_bytes),
            "sha256": hashlib.sha256(inventory_bytes).hexdigest(),
        }
    )
    records.sort(key=lambda record: str(record["path"]))
    prefix = "HalfALabs-Blender8/"
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".partial.zip")
    if destination.is_relative_to(root) or temporary.is_relative_to(root):
        raise ValueError("Write the complete ZIP outside this source directory.")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for record in records:
            info = zipfile.ZipInfo(prefix + str(record["path"]), date_time=(2026, 10, 7, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            data = (
                inventory_bytes
                if record["path"] == inventory_path
                else (root / str(record["path"])).read_bytes()
            )
            z.writestr(info, data)
        for name, content in (
            ("CONTENTS.sha256", "".join(f"{r['sha256']}  {r['path']}\n" for r in records)),
            (
                "ARCHIVE-INVENTORY.json",
                json.dumps(
                    {
                        "package_version": VERSION,
                        "files": records,
                        "file_count": len(records),
                        "omitted_generated_duplicates": [
                            "build/docs",
                            "dist/recipient",
                            "combined binders; regenerate with tools/project.py docs",
                        ],
                        "font_files_included": False,
                    },
                    indent=2,
                )
                + "\n",
            ),
        ):
            info = zipfile.ZipInfo(prefix + name, date_time=(2026, 10, 7, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, content.encode())
    with zipfile.ZipFile(temporary) as z:
        bad = z.testzip()
        if bad:
            raise ValueError(f"ZIP CRC verification failed: {bad}")
    temporary.replace(destination)
    return {
        "archive": str(destination),
        "bytes": destination.stat().st_size,
        "sha256": sha256(destination),
        "payload_files": len(records) + 2,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command",
        choices=(
            "doctor",
            "docs",
            "native",
            "test",
            "run",
            "wasm",
            "wasm-test",
            "serve-wasm",
            "bundle",
        ),
    )
    parser.add_argument(
        "--bench", action="store_true", help="run without firmware and permit raw bus writes"
    )
    parser.add_argument("--clean-only", action="store_true")
    parser.add_argument("--seed", type=int, default=4815)
    parser.add_argument("--compiler", default=None, help="C++ compiler, e.g. clang++")
    parser.add_argument(
        "--device",
        "--numeric-profile",
        dest="numeric_profile",
        choices=("B8", "B16"),
        default="B8",
        help="MCU and matching firmware arithmetic policy",
    )
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    parser.add_argument("--output", type=Path, help="complete ZIP destination")
    parser.add_argument("--port", type=int, default=8089, help="Loopback port for serve-wasm")
    parser.add_argument(
        "--memory-profile", choices=("standard", "plus", "free"), default="standard"
    )
    args = parser.parse_args()
    root = args.root.resolve()
    if (
        not (root / "pyproject.toml").is_file()
        or not (root / "internal/authoring/config/catalog.toml").is_file()
    ):
        parser.error(f"not a Blender8 repository root: {root}")
    if args.command == "doctor":
        run([sys.executable, "-m", "b8docs", "doctor", "--json", "--root", str(root)], root)
        path = str(Path(sys.executable).parent) + os.pathsep + os.environ.get("PATH", "")
        print(
            json.dumps(
                {
                    "python": sys.executable,
                    "cmake": shutil.which("cmake", path=path),
                    "cxx": shutil.which("c++", path=path),
                    "numeric_policy_clang": shutil.which("clang++", path=path),
                    "emcmake": shutil.which("emcmake", path=path),
                },
                indent=2,
            )
        )
    elif args.command == "docs":
        run([sys.executable, "tools/extract_manual_examples.py", "--root", str(root)], root)
        run(
            [
                sys.executable,
                "-m",
                "b8docs",
                "packet",
                "--seed",
                str(args.seed),
                "--root",
                str(root),
            ]
            + (["--clean-only"] if args.clean_only else []),
            root,
        )
    elif args.command in {"native", "test"}:
        build = root / ("build/native" if args.numeric_profile == "B8" else "build/native-b16")
        command = [
            "cmake",
            "-S",
            str(root / "platform"),
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPython3_EXECUTABLE={sys.executable}",
            f"-DB8_NUMERIC_PROFILE={args.numeric_profile}",
            f"-DB8_MEMORY_PROFILE={args.memory_profile}",
        ]
        if args.compiler:
            command.append(f"-DCMAKE_CXX_COMPILER={args.compiler}")
        run(command, root)
        run(["cmake", "--build", str(build), "--config", "Release", "--parallel", "4"], root)
        if args.command == "native":
            return
        run([sys.executable, "tools/assemble_existing.py", "--root", str(root)], root)
        run(
            [sys.executable, "tools/extract_manual_examples.py", "--check", "--root", str(root)],
            root,
        )
        run([sys.executable, "-m", "pytest", "-q"], root)
        run(["ctest", "--test-dir", str(build), "-C", "Release", "--output-on-failure"], root)
    elif args.command == "run":
        build = root / ("build/native" if args.numeric_profile == "B8" else "build/native-b16")
        candidates = [
            build / "b8_emulator",
            build / "b8_emulator.exe",
            build / "Release/b8_emulator.exe",
        ]
        executable = next((path for path in candidates if path.is_file()), None)
        if executable is None:
            raise FileNotFoundError("Run native --device " + args.numeric_profile + " first")
        run([str(executable), *(["--bench"] if args.bench else [])], root)
    elif args.command == "wasm":
        run(
            [
                sys.executable,
                "platform/tools/b8.py",
                "--root",
                str(root / "platform"),
                "--build-dir",
                "../build/wasm" if args.numeric_profile == "B8" else "../build/wasm-b16",
                "wasm-build",
                "--test-fixtures",
                "--memory-profile",
                args.memory_profile,
                "--numeric-profile",
                args.numeric_profile,
            ],
            root,
        )
        site = root / ("build/wasm" if args.numeric_profile == "B8" else "build/wasm-b16")
        print(f"WebAssembly page: {site / 'site/index.html'}")
    elif args.command == "wasm-test":
        run(
            [
                sys.executable,
                "platform/tools/b8.py",
                "--root",
                str(root / "platform"),
                "--build-dir",
                "../build/wasm" if args.numeric_profile == "B8" else "../build/wasm-b16",
                "wasm-test",
                "--native-build",
                "../build/native" if args.numeric_profile == "B8" else "../build/native-b16",
                "--output",
                "../reports/wasm-parity.json"
                if args.numeric_profile == "B8"
                else "../reports/wasm-b16-parity.json",
            ],
            root,
        )
    elif args.command == "serve-wasm":
        run(
            [
                sys.executable,
                "platform/tools/b8.py",
                "--root",
                str(root / "platform"),
                "--build-dir",
                "../build/wasm" if args.numeric_profile == "B8" else "../build/wasm-b16",
                "wasm-serve",
                "--port",
                str(args.port),
            ],
            root,
        )
    else:
        output = args.output or root.parent / f"HalfALabs-Blender8-Complete-v{VERSION}.zip"
        print(json.dumps(bundle(output, root), indent=2))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit(130) from None
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1) from error
