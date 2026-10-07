"""Create the repository virtual environment and install its Python development tools."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import venv
from pathlib import Path


def main() -> None:
    """Install from the selected checkout without inferring a root from this script's path."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    args = parser.parse_args()
    root = args.root.resolve()
    if (
        not (root / "pyproject.toml").is_file()
        or not (root / "internal/authoring/config/catalog.toml").is_file()
    ):
        parser.error(f"not a Blender8 repository root: {root}")
    if sys.version_info < (3, 12):  # noqa: UP036 - bootstrap runs before package checks
        parser.error("Python 3.12 or newer is required")

    environment = root / ".venv"
    python = environment / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if not python.is_file():
        venv.EnvBuilder(with_pip=True).create(environment)
    try:
        subprocess.run([str(python), "-m", "pip", "install", "-e", ".[dev]"], cwd=root, check=True)
    except subprocess.CalledProcessError as error:
        parser.exit(1, f"Python tooling installation failed (pip exit {error.returncode}).\n")
    print(f"Python tooling installed: {python}")
    command = [str(python), "tools/project.py", "doctor", "--root", str(root)]
    print("Next: " + (subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)))


if __name__ == "__main__":
    main()
