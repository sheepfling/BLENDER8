from __future__ import annotations

import sys
from collections.abc import Sequence
from pathlib import Path

import pytest

from tools import project

ROOT = Path(__file__).resolve().parents[1]


def skip_run(_args: Sequence[str], _root: Path, _extra_env: dict[str, str] | None = None) -> None:
    return None


def fake_which(name: str, path: str | None = None) -> str | None:
    del path
    return {"c++": "/usr/bin/c++", "clang++": "/usr/bin/clang++", "size": "/usr/bin/size"}.get(name)


def test_project_doctor_exits_nonzero_when_cmake_is_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(sys, "argv", ["project.py", "doctor", "--root", str(ROOT)])
    monkeypatch.setattr(project, "run", skip_run)
    monkeypatch.setattr(project.shutil, "which", fake_which)

    with pytest.raises(SystemExit) as error:
        project.main()

    assert error.value.code == 1
