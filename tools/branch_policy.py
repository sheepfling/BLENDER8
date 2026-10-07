"""Keep the problem presentation branch and its history free of the teaching solution."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

SOLUTION_BRANCH = "feature/b16-solution"
PROTECTED_BRANCHES = {"main", "master"}
SOLUTION_PATHS = (
    "TEACHING-SOLUTION.md",
    "tools/teaching.py",
    "internal/owner/experiments",
    "platform/tests/wasm/candidate-view.test.mjs",
)


def git(root: Path, *arguments: str) -> str:
    """Read this explicit checkout without inferring paths from the script location."""
    return subprocess.check_output(["git", *arguments], cwd=root, text=True).strip()


def expected_role(requested: str, base: str, source: str) -> str:
    """Protected integration targets always require a problem-only history."""
    if base in PROTECTED_BRANCHES or source in PROTECTED_BRANCHES:
        return "problem"
    if requested != "auto":
        return requested
    return "teaching" if SOLUTION_BRANCH in (base, source) else "problem"


def violations(root: Path, role: str) -> list[str]:
    """Check solution presence and, for problem branches, every reachable ancestor."""
    if role == "teaching":
        required = (
            "TEACHING-SOLUTION.md",
            "internal/owner/experiments/b16-controller/firmware.cpp",
        )
        return [
            f"Teaching branch is missing {name}" for name in required if not (root / name).is_file()
        ]
    errors: list[str] = []
    present = [name for name in SOLUTION_PATHS if (root / name).exists()]
    if present:
        errors.append("Solution material is present: " + ", ".join(present))
    if git(root, "rev-parse", "--is-shallow-repository") == "true":
        errors.append("Full history is required; fetch with depth 0 before checking the boundary.")
        return errors
    ancestor = git(root, "log", "-1", "--format=%h", "HEAD", "--", *SOLUTION_PATHS)
    if ancestor:
        errors.append(
            f"Solution history is reachable at {ancestor}; deleting its files is insufficient."
        )
    return errors


def main() -> int:
    """Resolve the CI target or explicit local role, then report a nonzero boundary failure."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="explicit checkout root")
    parser.add_argument("--expect", choices=("auto", "problem", "teaching"), default="auto")
    parser.add_argument("--base-branch", default=os.environ.get("GITHUB_BASE_REF", ""))
    parser.add_argument(
        "--source-branch",
        default=os.environ.get("GITHUB_HEAD_REF") or os.environ.get("GITHUB_REF_NAME", ""),
    )
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        source = args.source_branch or git(root, "branch", "--show-current")
        role = expected_role(args.expect, args.base_branch, source)
        errors = violations(root, role)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Cannot verify branch boundary: {error}")
        return 1
    for error in errors:
        print(error)
    if errors:
        return 1
    print(f"Branch boundary passed: {role} ({source or 'detached checkout'}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
