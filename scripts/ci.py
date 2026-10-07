"""Run the same Python and Markdown quality gates locally and in CI."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

PYTHON_PATHS = ("src", "scripts", "tests", "tools", "platform/tools", "platform/tests")


def run(command: Sequence[str], environment: dict[str, str], root: Path) -> int:
    """Run one check from the repository root and report its exit status."""
    print("+ " + " ".join(command), flush=True)
    try:
        return subprocess.run(command, cwd=root, env=environment, check=False).returncode
    except OSError as error:
        print(f"Could not start {command[0]}: {error}", file=sys.stderr)
        return 1


def main(argv: Sequence[str] | None = None) -> int:
    """Apply optional safe fixes, then run read-only checks and tests."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="apply formatter and safe policy fixes")
    parser.add_argument("--static", action="store_true", help="skip the Python test suite")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if (
        not (root / "pyproject.toml").is_file()
        or not (root / "internal/authoring/config/catalog.toml").is_file()
    ):
        parser.error(f"not a Blender8 repository root: {root}")
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join(
        (str(Path(sys.executable).parent), environment.get("PATH", ""))
    )
    rumdl = shutil.which("rumdl", path=environment["PATH"])
    if rumdl is None:
        print(
            'Install development dependencies: python -m pip install -e ".[dev]"', file=sys.stderr
        )
        return 1
    python = sys.executable
    if args.fix:
        fixes = (
            (python, "-m", "ruff", "check", "--fix", *PYTHON_PATHS),
            (python, "-m", "ruff", "format", *PYTHON_PATHS),
            (rumdl, "check", "--fix", "."),
            (python, "-m", "mdrepo", "fix", "."),
        )
        for command in fixes:
            if status := run(command, environment, root):
                return status
    checks = [
        (python, "-m", "compileall", "-q", *PYTHON_PATHS),
        (python, "-m", "ruff", "check", *PYTHON_PATHS),
        (python, "-m", "ruff", "format", "--check", *PYTHON_PATHS),
        (python, "-m", "pyright", "--pythonpath", python),
        (rumdl, "check", "."),
        (python, "-m", "mdrepo", "check", "."),
        (python, "-m", "pre_commit", "validate-config", ".pre-commit-config.yaml"),
    ]
    if not args.static:
        checks.append((python, "-m", "pytest"))
    for command in checks:
        if status := run(command, environment, root):
            return status
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
