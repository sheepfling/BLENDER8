"""Exercise the Git history boundary, including deleted solutions and protected PR targets."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

from tools.branch_policy import SOLUTION_BRANCH, expected_role, violations


def command(root: Path, *arguments: str) -> None:
    subprocess.run(["git", *arguments], cwd=root, check=True, capture_output=True, text=True)


@pytest.fixture
def repository(tmp_path: Path) -> Path:
    root = tmp_path / "checkout with spaces"
    root.mkdir()
    command(root, "init", "-b", "feature/problem")
    command(root, "config", "user.email", "branch-test@example.invalid")
    command(root, "config", "user.name", "Branch policy test")
    (root / "README.md").write_text("Problem presentation\n")
    command(root, "add", ".")
    command(root, "commit", "-m", "Problem baseline")
    return root


def test_problem_history_passes(repository: Path) -> None:
    assert violations(repository, "problem") == []


@pytest.mark.parametrize(
    "path", ["TEACHING-SOLUTION.md", "internal/owner/experiments/b16-controller/firmware.cpp"]
)
def test_solution_files_are_rejected(repository: Path, path: str) -> None:
    target = repository / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("Worked example\n")
    assert any("Solution material" in error for error in violations(repository, "problem"))


def test_deleting_solution_does_not_erase_ancestry(repository: Path) -> None:
    target = repository / "TEACHING-SOLUTION.md"
    target.write_text("Solution\n")
    command(repository, "add", ".")
    command(repository, "commit", "-m", "Teaching solution")
    target.unlink()
    command(repository, "add", "-u")
    command(repository, "commit", "-m", "Remove visible solution")
    assert any("history is reachable" in error for error in violations(repository, "problem"))


def test_teaching_requires_guide_and_firmware(repository: Path) -> None:
    assert len(violations(repository, "teaching")) == 2
    for path in (
        "TEACHING-SOLUTION.md",
        "internal/owner/experiments/b16-controller/firmware.cpp",
    ):
        target = repository / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("Teaching material\n")
    assert violations(repository, "teaching") == []


@pytest.mark.parametrize("base", ["main", "master"])
def test_solution_pr_into_integration_is_problem_checked(base: str) -> None:
    assert expected_role("auto", base, SOLUTION_BRANCH) == "problem"
    assert expected_role("teaching", base, SOLUTION_BRANCH) == "problem"


def test_shared_updates_can_enter_teaching_branch() -> None:
    assert expected_role("auto", SOLUTION_BRANCH, "feature/b8-b16-workbench") == "teaching"
    assert expected_role("auto", "", SOLUTION_BRANCH) == "teaching"


def test_shallow_checkout_cannot_certify_absent_solution_history(repository: Path) -> None:
    clone = repository.parent / "shallow"
    command(repository.parent, "clone", "--depth", "1", repository.as_uri(), str(clone))
    assert any("Full history is required" in error for error in violations(clone, "problem"))
