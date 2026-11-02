"""Learner build commands must reset a reused cache to the supplied starter."""

from __future__ import annotations

import sys
from collections.abc import Sequence
from pathlib import Path

import pytest

from tools import project


def test_project_native_overrides_solution_firmware_in_existing_cache(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    root = tmp_path / "checkout"
    (root / "platform/firmware").mkdir(parents=True)
    (root / "internal/authoring/config").mkdir(parents=True)
    (root / "pyproject.toml").write_text("[project]\nname='fixture'\n")
    (root / "internal/authoring/config/catalog.toml").write_text("[catalog]\n")

    build = root / "build/native-b16"
    build.mkdir(parents=True)
    solution = root / "internal/owner/experiments/b16-controller"
    (build / "CMakeCache.txt").write_text(f"B8_FIRMWARE_DIR:PATH={solution}\n")

    calls: list[tuple[list[str], Path]] = []

    def capture(
        args: Sequence[str],
        command_root: Path,
        _extra_env: dict[str, str] | None = None,
    ) -> None:
        calls.append((list(args), command_root))

    monkeypatch.setattr(project, "run", capture)
    monkeypatch.setattr(
        sys,
        "argv",
        ["project.py", "native", "--root", str(root), "--device", "B16"],
    )

    project.main()

    configure, command_root = calls[0]
    assert command_root == root
    assert f"-DB8_FIRMWARE_DIR={root / 'platform/firmware'}" in configure
    assert f"-DB8_FIRMWARE_DIR={solution}" not in configure
