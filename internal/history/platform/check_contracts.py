"""Check document/manifest identity and basic numerical invariants, not firmware behavior."""

from __future__ import annotations

import argparse
import json
import math
import re
from itertools import pairwise
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    root = parser.parse_args().root.resolve()
    archive = root / "internal/history/platform"
    platform = root / "platform"
    profile = json.loads((archive / "spec/teaching_profile.json").read_text())
    requirements = json.loads((archive / "spec/firmware_requirements.json").read_text())[
        "requirements"
    ]
    cases = json.loads((archive / "scenarios/acceptance_cases.json").read_text())["cases"]
    req_ids = {item["id"] for item in requirements}
    assert len(req_ids) == len(requirements) == 30
    assert len({item["id"] for item in cases}) == len(cases) == 38
    assert all(set(case["requirements"]) <= req_ids for case in cases)
    document = (archive / "docs/requirements/FW_REQUIREMENTS.md").read_text()
    # Markdown wrapping and escaping leave the rendered requirement text unchanged.
    normalized_document = " ".join(document.replace(r"\*", "*").split())
    for requirement in requirements:
        assert requirement["id"] in document
        assert " ".join(requirement["shall"].split()) in normalized_document
    speeds = profile["speed_targets_rpm"]
    assert len(speeds) == 7 and speeds[0] > 0
    assert len({b - a for a, b in pairwise(speeds)}) == 1
    for target in [profile["pulse_increment_rpm"], *speeds]:
        duty = 0.12 + 0.88 * (target / profile["rated_rpm"]) ** (1 / 1.6)
        code = min(255, math.floor(256 * duty + 0.5))
        effective = 1 if code == 255 else code / 256
        achieved = profile["rated_rpm"] * ((effective - 0.12) / 0.88) ** 1.6
        assert abs(achieved - target) <= max(100, 0.02 * target)
    assert profile["thermal_rearm_celsius"] < profile["thermal_trip_celsius"]
    assert profile["stall_floor_rpm"] < profile["pulse_increment_rpm"]
    assert profile["temperature_sample_max_period_ms"] < profile["temperature_freshness_limit_ms"]
    register_manifest = json.loads((platform / "spec/registers.json").read_text())["registers"]
    assert len({item["address"] for item in register_manifest}) == len(register_manifest)
    reference_inventory = json.loads((platform / "spec/reference_test_inventory.json").read_text())[
        "test_names"
    ]
    source = (platform / "tests/reference_tests.cpp").read_text()
    names = ["reference." + name for name in re.findall(r'\{"([a-z0-9_]+)",[a-z0-9_]+\}', source)]
    assert reference_inventory == names and len(names) == len(set(names))
    print(
        f"Checked {len(requirements)} requirements, {len(cases)} planned acceptance cases, "
        f"{len(names)} reference cases and numerical-profile invariants."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
