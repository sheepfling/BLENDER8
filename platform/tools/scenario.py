"""Timestamped fixtures and observable assertions. No implicit clock configuration or WDT feed."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from collections.abc import Callable
from pathlib import Path
from typing import Any, cast

from b8client import Engine


def field_value(state: dict[str, Any], path: str) -> Any:
    value: Any = state
    for key in path.split("."):
        if not isinstance(value, dict) or key not in value:
            raise ValueError(f"unknown/unavailable observation {path}")
        value = cast(dict[str, Any], value)[key]
    return value


def evaluate(actual: Any, operator: str, expected: Any) -> bool:
    if operator == "eq":
        return (
            (type(actual) in (int, float) and type(expected) in (int, float))
            or type(actual) is type(expected)
        ) and actual == expected
    if operator == "ne":
        return actual != expected
    if operator == "finite":
        return isinstance(actual, (int, float)) and math.isfinite(actual)
    if operator == "between":
        if not isinstance(expected, list):
            return False
        bounds = cast(list[Any], expected)
        if len(bounds) != 2:
            return False
        return bounds[0] <= actual <= bounds[1]
    if operator == "gt":
        return actual > expected
    if operator == "ge":
        return actual >= expected
    if operator == "lt":
        return actual < expected
    if operator == "le":
        return actual <= expected
    raise ValueError(f"unsupported operator {operator}")


def load_scenario(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if set(data) - {"version", "name", "mode", "profile", "notes", "events", "checks"}:
        raise ValueError("unknown scenario field")
    if data.get("version") != 1 or data.get("mode") not in {"bench", "firmware"}:
        raise ValueError("scenario version 1 and explicit mode required")
    if data.get("profile", "chassis04") not in {"chassis04", "legacy02"}:
        raise ValueError("unknown profile")
    if data.get("profile") == "legacy02" and data["mode"] != "bench":
        raise ValueError("legacy profile is bench-only")
    for key in ("events", "checks"):
        if not isinstance(data.get(key), list) or len(data[key]) > 10000:
            raise ValueError(f"bounded {key} list required")
        for item in data[key]:
            if type(item.get("at_us")) is not int or not 0 <= item["at_us"] <= 60_000_000:
                raise ValueError("scenario times must be integer microseconds in [0,60000000]")
    for event in data["events"]:
        if set(event) != {"at_us", "command"} or not isinstance(event["command"], str):
            raise ValueError("event requires only at_us and command")
        if "\n" in event["command"] or len(event["command"]) > 4096:
            raise ValueError("invalid event command")
    for check in data["checks"]:
        if set(check) - {"at_us", "field", "op", "value", "why"}:
            raise ValueError("unknown check field")
        if not isinstance(check.get("field"), str) or check.get("op") not in {
            "eq",
            "ne",
            "finite",
            "between",
            "gt",
            "ge",
            "lt",
            "le",
        }:
            raise ValueError("invalid field/operator")
    if not data["checks"]:
        raise ValueError("scenario must make at least one assertion")
    return data


def run_scenario(
    executable: Path,
    path: Path,
    *,
    engine_factory: Callable[[bool, bool], Engine] | None = None,
) -> dict[str, Any]:
    spec = load_scenario(path)
    checks: list[dict[str, Any]] = []
    bench, legacy = spec["mode"] == "bench", spec.get("profile") == "legacy02"
    with (
        engine_factory(bench, legacy)
        if engine_factory
        else Engine(executable, bench=bench, legacy=legacy) as engine
    ):
        # Validation/parsing of all scheduled commands occurs before advancing time.
        for event in spec["events"]:
            engine.command(f"schedule {event['at_us']} {event['command']}")
        state = engine.state()
        for check in sorted(spec["checks"], key=lambda x: x["at_us"]):
            while state["time_us"] < check["at_us"]:
                state = engine.run(min(10_000_000, check["at_us"] - state["time_us"]))
            actual = field_value(state, check["field"])
            passed = evaluate(actual, check["op"], check.get("value"))
            checks.append(
                {**check, "actual": actual, "observed_at_us": state["time_us"], "passed": passed}
            )
        transcript = list(engine.transcript)
    return {
        "scenario": spec["name"],
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "mode": spec["mode"],
        "passed": all(x["passed"] for x in checks),
        "checks": checks,
        "final_state": state,
        "commands": transcript,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = run_scenario(args.exe, args.scenario)
    text = json.dumps(result, indent=2, allow_nan=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
