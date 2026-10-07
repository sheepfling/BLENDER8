"""Black-box acceptance against the current correspondence, not the retired algorithm rubric.

A safe or pixel-probe starter is EXPECTED to fail functional cases. Hardware conformance,
firmware acceptance, font review and physical safety validation are separate outcomes.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime
from itertools import pairwise
from pathlib import Path
from typing import Any, cast

from b8client import Engine, EngineError

State = dict[str, Any]


class ReviewNeeded(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def requests_off(s: State) -> bool:
    m = s["mcu"]
    return m["gpiob_out"] & 1 == 0 and m["pwm_enabled"] == 0 and m["pwm_shadow"] == 0


def observations(trace: dict[str, Any]) -> list[State]:
    # Events preserve exact logical transition times. Keep native insertion order for ties.
    return sorted(
        trace.get("events", []) + trace.get("samples", []) + [trace["state"]],
        key=lambda s: s["time_us"],
    )


@dataclass(frozen=True)
class Case:
    name: str
    source: str
    function: Callable[[Session], None]


class Session:
    def __init__(self, engine: Engine, glyphs: dict[str, list[str]] | None) -> None:
        self.engine = engine
        self.glyphs = glyphs
        self.evidence: dict[str, Any] = {}

    def state(self) -> State:
        return self.engine.state()

    def command(self, command: str) -> State:
        return self.engine.command(command)["state"]

    def trace(self, duration: int, period: int = 1000) -> dict[str, Any]:
        if duration // period > 1000:
            period = (duration + 999) // 1000
        t = self.engine.trace(duration, period)
        self.evidence["last_trace"] = t
        return t

    def cold_ready(self) -> None:
        # No register pokes, no hidden WDT services, no host-selected PLL tuple.
        self.engine.run(2_300_000)
        self.command("stop 1")
        self.engine.run(50_000)
        self.command("stop 0")
        self.engine.run(50_000)

    def running(self, speed: int = 4) -> State:
        self.cold_ready()
        self.command(f"speed {speed}")
        t = self.trace(1_045_000, 5000)
        s = t["state"]
        require(
            any(x["drive_enabled"] for x in observations(t)),
            "PRECONDITION: firmware never started a permitted command",
        )
        require(
            s["drive_enabled"] and s["motor"]["rpm"] > 1000,
            "PRECONDITION: normal operation was not sustained",
        )
        return s

    def off_by(self, trace: dict[str, Any], at: int, *, hold: bool = True) -> None:
        states = observations(trace)
        off = [s for s in states if s["time_us"] <= at and requests_off(s)]
        require(bool(off), f"firmware's enable/PWM/pending duty were not all cleared by {at}us")
        if hold:
            first = off[0]["time_us"]
            require(
                all(
                    requests_off(s) and not s["drive_enabled"]
                    for s in states
                    if s["time_us"] >= first
                ),
                "drive request returned without recovery",
            )

    def label(
        self, expected: str, trace: dict[str, Any] | None = None, deadline: int | None = None
    ) -> None:
        s = self.state()
        self.evidence["expected_label"] = expected
        self.evidence["scanned_pixels"] = s["lcd_pixels"]
        frames = observations(trace) if trace is not None else [s]
        if deadline is not None:
            frames = [x for x in frames if x["time_us"] <= deadline]
            self.evidence["display_deadline_us"] = deadline
        self.evidence["observed_frames"] = [
            {"time_us": x["time_us"], "pixels": x["lcd_pixels"]}
            for x in frames
            if x["lcd_pixels"] is not None
        ]
        if self.glyphs is None or expected not in self.glyphs:
            raise ReviewNeeded(
                f"Review scanned frame for {expected}; supply approved --glyphs templates "
                "for automatic recognition (no mandatory font)"
            )
        require(
            any(x["lcd_pixels"] in self.glyphs[expected] for x in frames),
            f"scanned display does not match an approved {expected} frame "
            "within the observed interval",
        )


def startup_off(s: Session) -> None:
    s.command("speed 7")
    t = s.trace(2_100_000, 5000)
    require(
        all(requests_off(x) and not x["drive_enabled"] for x in observations(t)),
        "startup/held selection energized outputs",
    )


def startup_clock(s: Session) -> None:
    t = s.trace(180_000, 1000)
    states = observations(t)
    ready = next((x for x in states if x["ready"]), None)
    if ready is None:
        raise AssertionError("no reset release")
    valid = [
        x
        for x in states
        if x["mcu"]["clock_source"] == 2 and x["time_us"] <= ready["time_us"] + 100_000
    ]
    require(bool(valid), "external PLL not active within 100ms of first execution opportunity")
    m = valid[0]["mcu"]
    require(
        8e6 <= m["system_hz"] <= 32e6 and abs(m["peripheral_hz"] - 1e6) <= 1000,
        "clock outside agreed ranges",
    )
    require(all(requests_off(x) for x in states), "startup switched clock with a drive request")


def monitors(s: Session) -> None:
    s.cold_ready()
    s.command("speed 1")
    t = s.trace(100_000)
    driving = [x for x in observations(t) if x["drive_enabled"]]
    require(bool(driving), "no drive; cannot establish before-drive monitor configuration")
    for x in driving:
        m = x["mcu"]
        require(
            m["wdt_control"] & 128 and m["dmt_enabled"] and m["dmt_locked"],
            "both monitors must be locked before any drive",
        )
        require(256 * 2 ** m["wdt_scale"] / 9000 <= 0.250, "slowest watchdog timeout exceeds 250ms")
        # Source can be 50ppm slow. Nominal SYSCLK and source-derived relative tolerance.
        require(
            m["dmt_limit"] * 1024 / (m["system_hz"] * (1 - 0.00005)) <= 0.100,
            "slowest DMT expiry exceeds 100ms",
        )
        require(m["dmt_window"] < m["dmt_limit"], "invalid DMT service window")


def speed_case(n: int) -> Callable[[Session], None]:
    def check(s: Session) -> None:
        s.cold_ready()
        start = s.state()["time_us"]
        s.command(f"speed {n}")
        t = s.trace(1_000_000, 5000)
        states = observations(t)
        require(
            any(x["drive_enabled"] and x["time_us"] <= start + 45_000 for x in states),
            "stable selection missed 40ms after the 5ms bounce envelope",
        )
        target = 3000 + (n - 1) * 2500
        actual = t["state"]["motor"]["rpm"]
        require(
            t["state"]["drive_enabled"] and abs(actual - target) <= max(100, 0.02 * target),
            f"speed {n}: {actual:.1f} RPM, target {target}",
        )

    return check


def pulse_idle(s: Session) -> None:
    s.cold_ready()
    s.command("pulse 1")
    state = s.engine.run(1_045_000)
    require(
        state["drive_enabled"] and abs(state["motor"]["rpm"] - 2000) <= 100,
        "standalone PULSE must reach 2000 RPM",
    )
    start = state["time_us"]
    s.command("pulse 0")
    t = s.trace(100_000)
    s.off_by(t, start + 45_000)


def pulse_boost(s: Session) -> None:
    s.running(7)
    s.command("pulse 1")
    state = s.engine.run(1_045_000)
    require(
        state["drive_enabled"] and abs(state["motor"]["rpm"] - 20000) <= 400,
        "speed7 PULSE capped at20000RPM",
    )
    s.command("pulse 0")
    state = s.engine.run(1_045_000)
    require(abs(state["motor"]["rpm"] - 18000) <= 360, "PULSE release restores selected request")


def stop_cutoff(s: Session) -> None:
    before = s.running()
    s.command("stop 1")
    t = s.trace(100_000)
    s.off_by(t, before["time_us"] + 10_000)
    require(t["state"]["motor"]["rpm"] > 0, "cutoff must not reset rotor state")
    s.command("stop 0")
    t = s.trace(150_000)
    require(all(requests_off(x) for x in observations(t)), "STOP release restored old requests")


def pulse_through_stop(s: Session) -> None:
    s.running()
    s.command("pulse 1")
    s.engine.run(100_000)
    s.command("stop 1")
    s.engine.run(30_000)
    s.command("stop 0")
    t = s.trace(150_000)
    require(all(requests_off(x) for x in observations(t)), "held PULSE resumed after STOP")


def jar_removal(s: Session) -> None:
    before = s.running()
    s.command("jar 0")
    t = s.trace(100_000)
    states = observations(t)
    require(
        any(not x["drive_enabled"] and x["time_us"] <= before["time_us"] + 1000 for x in states),
        "jar hardware inhibit missed1ms",
    )
    s.off_by(t, before["time_us"] + 10_000)


def jar_reseat(s: Session) -> None:
    s.running()
    s.command("jar 0")
    s.engine.run(20_000)
    s.command("jar 1")
    t = s.trace(250_000)
    require(
        all(requests_off(x) and not x["drive_enabled"] for x in observations(t)),
        "reseating or time restored prior command",
    )


def jar_quick(s: Session) -> None:
    state = s.running()
    t0 = state["time_us"] + 10
    s.command("jar_bounce 0")
    s.command(f"schedule {t0} jar 0")
    s.command(f"schedule {t0 + 1} jar 1")
    t = s.trace(100_000)
    s.off_by(t, t0 + 10_000)
    require(t["state"]["jar_ok"] and not t["state"]["jar_permit"], "brief dropout not latched")


def jar_broken(s: Session) -> None:
    before = s.running()
    s.command("jar_fault open")
    t = s.trace(100_000)
    s.off_by(t, before["time_us"] + 10_000)


def jam_start(s: Session) -> None:
    s.cold_ready()
    s.command("jam 1")
    start = s.state()["time_us"]
    s.command("speed 4")
    t = s.trace(900_000)
    states = observations(t)
    require(
        any(x["drive_enabled"] for x in states),
        "no start attempt; blocked-start behavior not exercised",
    )
    # Ignore initial off state before the request has actually been applied.
    first = next(x["time_us"] for x in states if x["drive_enabled"])
    t2 = {"samples": [x for x in states if x["time_us"] > first], "state": t["state"]}
    s.off_by(t2, start + 750_000)
    s.command("jam 0")
    t = s.trace(300_000)
    require(all(requests_off(x) for x in observations(t)), "jam removal automatically restarted")


def jam_running(s: Session) -> None:
    before = s.running()
    s.command("jam 1")
    t = s.trace(500_000)
    s.off_by(t, before["time_us"] + 400_000)


def jam_grace_not_renewed(s: Session) -> None:
    before = s.running()
    t0 = before["time_us"]
    s.command("jam 1")
    s.command(f"schedule {t0 + 100000} speed 6")
    s.command(f"schedule {t0 + 200000} pulse 1")
    t = s.trace(500_000)
    s.off_by(t, t0 + 400_000)


def tach_missing(s: Session) -> None:
    before = s.running()
    s.command("tach low")
    t = s.trace(500_000)
    s.off_by(t, before["time_us"] + 400_000)


def load_allowed(s: Session) -> None:
    s.running()
    s.command("load 1")
    t = s.trace(1_000_000, 5000)
    require(all(x["drive_enabled"] for x in observations(t)), "permitted load falsely stopped")
    require(t["state"]["motor"]["rpm"] > 1000, "loaded motion must remain nonzero")


def invalid_inputs(s: Session) -> None:
    before = s.running(1)
    s.command("contact 1 closed")
    t = s.trace(150_000)
    s.off_by(t, before["time_us"] + 50_000)


def sensor_fault(fault: str) -> Callable[[Session], None]:
    def check(s: Session) -> None:
        before = s.running()
        s.command(f"sensor {fault}")
        t = s.trace(100_000)
        s.off_by(t, before["time_us"] + 20_000)

    return check


def thermal_trip(s: Session) -> None:
    s.running()
    s.command("temperature 120")
    t = s.trace(800_000, 1000)
    states = observations(t)
    last: tuple[int, int] | None = None
    qualify = None
    hot = 0
    for x in states:
        m = x["mcu"]
        key = (m["reset_serial"], m["adc_fresh_reads"])
        if key == last or not m["adc_fresh_reads"] or m["last_adc_read_channel"] != 0:
            continue
        last = key
        temperature = (3.3 * m["last_adc_read_code"] / 1023 - 0.5) / 0.01
        hot = hot + 1 if temperature >= 85 else 0
        if hot >= 2:
            qualify = cast(int, m["last_adc_read_us"])
            break
    if qualify is None:
        raise AssertionError("firmware did not acquire two hot fresh readings")
    after = {"samples": [x for x in states if x["time_us"] >= qualify], "state": t["state"]}
    s.off_by(after, qualify + 10_000)
    require(t["state"]["motor"]["rpm"] > 0, "thermal trip must remove drive, not erase momentum")


def adc_stale(s: Session) -> None:
    before = s.running()
    s.command("adc_stalled 1")
    t = s.trace(200_000)
    s.off_by(t, before["time_us"] + 110_001)


def fresh_after_stop(s: Session) -> None:
    s.running()
    s.command("stop 1")
    s.engine.run(30_000)
    t = s.trace(150_000)
    reads = sorted(
        {
            x["mcu"]["last_adc_read_us"]
            for x in observations(t)
            if x["mcu"]["adc_fresh_reads"] and x["mcu"]["last_adc_read_channel"] == 0
        }
    )
    require(len(reads) >= 14, "fresh sampling stopped with motor off")
    require(
        all(b - a <= 10_000 for a, b in pairwise(reads)),
        "temperature acquisition interval exceeds10ms",
    )


def cooling_no_ack(s: Session) -> None:
    s.running()
    s.command("sensor open")
    s.engine.run(100_000)
    s.command("stop 1")
    s.command("sensor healthy")
    s.engine.run(2_300_000)
    s.command("stop 0")
    s.engine.run(50_000)
    s.command("speed 1")
    t = s.trace(100_000)
    require(
        all(requests_off(x) for x in observations(t)),
        "STOP held during recovery improperly queued acknowledgment",
    )
    s.command("stop 1")
    s.engine.run(50_000)
    s.command("stop 0")
    s.engine.run(50_000)
    s.command("speed 1")
    t = s.trace(100_000)
    require(t["state"]["drive_enabled"], "fresh recovery should permit later new selection")


def missing_clock(s: Session) -> None:
    s.command("clock_failed 1")
    s.command("speed 7")
    t = s.trace(500_000)
    require(
        all(requests_off(x) and not x["drive_enabled"] for x in observations(t)),
        "missing external clock allowed drive",
    )


def clock_loss(s: Session) -> None:
    before = s.running()
    s.command("clock_failed 1")
    t = s.trace(100_000, 100)
    require(
        any(
            not x["drive_enabled"] and x["time_us"] <= before["time_us"] + 1000
            for x in observations(t)
        ),
        "clock loss inhibit missed1ms",
    )
    s.command("clock_failed 0")
    t = s.trace(500_000)
    require(all(requests_off(x) for x in observations(t)), "clock recovery automatically resumed")


def brownout(s: Session) -> None:
    before = s.running()
    s.command("voltage 2.7")
    t = s.trace(30_000, 100)
    require(
        any(
            not x["drive_enabled"] and x["time_us"] <= before["time_us"] + 1000
            for x in observations(t)
        ),
        "brownout gate failed",
    )
    s.command("voltage auto")
    t = s.trace(2_200_000, 5000)
    require(
        all(requests_off(x) and not x["drive_enabled"] for x in observations(t)),
        "brownout recovery resumed old request",
    )


def power_retention(s: Session) -> None:
    s.running()
    s.command("power 0")
    s.engine.run(100_000)
    s.command("power 1")
    t = s.trace(2_400_000, 5000)
    require(
        all(requests_off(x) and not x["drive_enabled"] for x in observations(t)),
        "retained speed/power cycle restarted",
    )


def watchdog_halt(s: Session) -> None:
    before = s.running()
    s.command("halt 1")
    t = s.trace(350_000)
    states = observations(t)
    require(
        any(x["mcu"]["reset_causes"] & 4 for x in states),
        "core halt did not produce watchdog reset",
    )
    require(
        any(not x["drive_enabled"] and x["time_us"] <= before["time_us"] + 250_000 for x in states),
        "watchdog bound exceeded",
    )
    require(not t["state"]["drive_enabled"], "watchdog reset restarted")


def lost_foreground(s: Session) -> None:
    before = s.running()
    s.command("foreground 0")
    t = s.trace(150_000)
    states = observations(t)
    require(
        any(x["mcu"]["reset_causes"] & 8 for x in states),
        "lost foreground progress did not produce DMT reset; possible blind ISR feeding",
    )
    require(
        any(not x["drive_enabled"] and x["time_us"] <= before["time_us"] + 100_000 for x in states),
        "DMT expiry bound exceeded",
    )
    s.evidence["timer_irqs_before"] = before["mcu"]["irq_deliveries"][:2]
    s.evidence["timer_irqs_before_reset"] = [
        x["mcu"]["irq_deliveries"][:2]
        for x in states
        if x["mcu"]["reset_serial"] == before["mcu"]["reset_serial"]
    ][-1:]
    s.evidence["injection"] = (
        "Foreground calls suppressed; peripheral ticks and IRQ dispatch continue. "
        "Review that this firmware's tick ISR actually fires."
    )


def display_case(label: str) -> Callable[[Session], None]:
    def check(s: Session) -> None:
        # Customer leaves the font open. Observe SCANOUT, not private framebuffer state.
        if label == "0":
            s.cold_ready()
            s.label(label)
            return
        if label in {str(n) for n in range(1, 8)}:
            s.running(int(label))
            s.label(label)
            return
        if label == "P" or label.endswith("P"):
            if label == "P":
                s.cold_ready()
            else:
                s.running(int(label[:-1]))
            s.command("pulse 1")
            s.engine.run(100_000)
            s.label(label)
            return
        if label == "CK":
            s.command("clock_failed 1")
            trace = s.trace(250_000)
            # Missing-clock classification time is not prescribed by the email;
            # require the reason in this bounded observation, with the separate no-drive case.
            s.label(label, trace)
            return
        s.running(1 if label == "IF" else 4)
        start = s.state()["time_us"]
        stimulus = {
            "IL": "jar 0",
            "TS": "sensor open",
            "TH": "temperature 120",
            "ST": "jam 1",
            "IF": "contact 1 closed",
            "WD": "halt 1",
            "DM": "foreground 0",
        }[label]
        s.command(stimulus)
        duration = {
            "IL": 100_000,
            "TS": 110_000,
            "TH": 900_000,
            "ST": 500_000,
            "IF": 140_000,
            "WD": 400_000,
            "DM": 250_000,
        }[label]
        trace = s.trace(duration, 1000)
        states = observations(trace)
        if label in {"WD", "DM"}:
            bit = 4 if label == "WD" else 8
            release = cast(
                int | None,
                next(
                    (x["time_us"] for x in states if x["ready"] and x["mcu"]["reset_causes"] & bit),
                    None,
                ),
            )
            if release is None:
                raise AssertionError("reset/release not observed")
            deadline = release + 100_000
        elif label == "TH":
            last = None
            hot = 0
            qualified = None
            for x in states:
                m = x["mcu"]
                key = (m["reset_serial"], m["adc_fresh_reads"])
                if key == last or not m["adc_fresh_reads"] or m["last_adc_read_channel"] != 0:
                    continue
                last = key
                hot = hot + 1 if (3.3 * m["last_adc_read_code"] / 1023 - 0.5) / 0.01 >= 85 else 0
                if hot >= 2:
                    qualified = cast(int, m["last_adc_read_us"])
                    break
            if qualified is None:
                raise AssertionError("two fresh hot acquisitions not observed")
            deadline = qualified + 100_000
        else:
            # Worst latest externally justified qualification + allowed display latency.
            deadline = start + {"IL": 100_000, "TS": 110_000, "ST": 500_000, "IF": 140_000}[label]
        s.label(label, trace, deadline)

    return check


def review_progress(s: Session) -> None:
    raise ReviewNeeded(
        "Review firmware evidence that stale input/acquisition/protection/output completions "
        "cannot authorize a second renewal. The device's service writes alone cannot prove "
        "semantic progress."
    )


def cases() -> list[Case]:
    result = [
        Case("startup.outputs_off", "stop-04/interlock-04", startup_off),
        Case("startup.external_clock", "clock-04", startup_clock),
        Case("supervision.config_before_drive", "supervision-04", monitors),
    ]
    result += [Case(f"panel.speed_{n}", "panel-04", speed_case(n)) for n in range(1, 8)]
    for name, source, fn in [
        ("panel.pulse_idle", "panel-04", pulse_idle),
        ("panel.pulse_boost_restore", "panel-04", pulse_boost),
        ("stop.cutoff_and_release", "stop-04", stop_cutoff),
        ("stop.held_pulse", "stop-04", pulse_through_stop),
        ("jar.removal", "interlock-04", jar_removal),
        ("jar.reseat", "interlock-04", jar_reseat),
        ("jar.quick_dropout", "interlock-04", jar_quick),
        ("jar.broken_wire", "interlock-04", jar_broken),
        ("motion.blocked_start", "motion-04", jam_start),
        ("motion.running_jam", "motion-04", jam_running),
        ("motion.no_grace_renewal", "motion-04", jam_grace_not_renewed),
        ("motion.tach_missing", "motion-04", tach_missing),
        ("motion.permitted_load", "panel-04/motion-04", load_allowed),
        ("panel.invalid_combination", "panel-04", invalid_inputs),
        ("thermal.trip", "temperature-04", thermal_trip),
        ("thermal.stale_adc", "temperature-04", adc_stale),
        ("thermal.sampling_after_stop", "temperature-04", fresh_after_stop),
        ("recovery.no_queued_ack", "temperature-04", cooling_no_ack),
        ("clock.missing_at_boot", "clock-04", missing_clock),
        ("clock.loss_during_run", "clock-04", clock_loss),
        ("power.logic_brownout", "clock-04/supervision-04", brownout),
        ("power.retained_selection", "stop-04/interlock-04", power_retention),
        ("supervision.halted_core", "supervision-04", watchdog_halt),
        ("supervision.lost_foreground", "supervision-04", lost_foreground),
    ]:
        result.append(Case(name, source, fn))
    result += [
        Case(f"thermal.sensor_{f}", "temperature-04", sensor_fault(f))
        for f in ("open", "ground", "supply")
    ]
    result += [
        Case(
            f"display.{label}",
            "panel-04/temperature-04/supervision-04/interlock-04",
            display_case(label),
        )
        for label in (
            "0",
            "1",
            "2",
            "3",
            "4",
            "5",
            "6",
            "7",
            "P",
            "1P",
            "2P",
            "3P",
            "4P",
            "5P",
            "6P",
            "7P",
            "IL",
            "TS",
            "TH",
            "ST",
            "IF",
            "CK",
            "WD",
            "DM",
        )
    ]
    result.append(Case("review.progress_authorization", "supervision-04", review_progress))
    result.append(
        Case(
            "review.fault_priority_retention",
            "temperature-04/supervision-04/interlock-04",
            review_priority,
        )
    )
    return result


def review_priority(s: Session) -> None:
    raise ReviewNeeded(
        "Review combined-fault evidence: CK > WD > DM > TS > TH > IF > IL > ST, subordinate "
        "causes retained, rail codes not also TH, intentional interlock coastdown not ST. "
        "Internal fault-bit assignments are not prescribed."
    )


def load_glyphs(path: Path | None) -> dict[str, list[str]] | None:
    if path is None:
        return None
    loaded = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(loaded, dict):
        raise ValueError("glyph file maps labels to lists of approved512-bit scanned frames")
    glyphs: dict[str, list[str]] = {}
    for label, frames in cast(dict[Any, Any], loaded).items():
        if not isinstance(label, str) or not isinstance(frames, list) or not frames:
            raise ValueError("invalid glyph entry")
        if any(
            not isinstance(f, str) or len(f) != 512 or set(f) - {"0", "1"}
            for f in cast(list[Any], frames)
        ):
            raise ValueError("each approved frame must be512binary pixels")
        glyphs[label] = cast(list[str], frames)
    return glyphs


def load_reviews(path: Path | None, executable: Path) -> dict[str, dict[str, str]]:
    """An explicit review record is evidence, not a switch that suppresses failed tests."""
    if path is None:
        return {}
    loaded = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(loaded, dict) or set(cast(dict[Any, Any], loaded)) != {
        "executable_sha256",
        "approvals",
    }:
        raise ValueError("review record requires executable_sha256 and approvals")
    data = cast(dict[str, Any], loaded)
    if data["executable_sha256"] != hashlib.sha256(executable.read_bytes()).hexdigest():
        raise ValueError("review record belongs to a different executable")
    raw_approvals = data["approvals"]
    if not isinstance(raw_approvals, dict):
        raise ValueError("approvals must be a case-keyed object")
    approvals = cast(dict[Any, Any], raw_approvals)
    allowed = {c.name for c in cases() if c.name.startswith(("review.", "display."))}
    for case, approval in approvals.items():
        if case not in allowed or not isinstance(approval, dict):
            raise ValueError("invalid review case or record fields")
        fields = cast(dict[str, Any], approval)
        if set(fields) != {"reviewer", "approved_at", "evidence"}:
            raise ValueError("invalid review case or record fields")
        if any(not isinstance(x, str) or not x.strip() for x in fields.values()):
            raise ValueError("reviewer, timestamp and evidence must be nonempty strings")
        approved_at = cast(str, fields["approved_at"])
        if datetime.fromisoformat(approved_at.replace("Z", "+00:00")).utcoffset() is None:
            raise ValueError("review timestamp requires a timezone")
    return cast(dict[str, dict[str, str]], approvals)


def run(
    executable: Path,
    selected: list[str] | None = None,
    glyphs: dict[str, list[str]] | None = None,
    reviews: dict[str, dict[str, str]] | None = None,
    *,
    engine_factory: Callable[[], Engine] | None = None,
) -> dict[str, Any]:
    inventory = cases()
    complete_suite = not selected or set(selected) == {c.name for c in inventory}
    if selected:
        names = {c.name for c in inventory}
        if set(selected) - names:
            raise ValueError(f"unknown cases: {set(selected) - names}")
        inventory = [c for c in inventory if c.name in selected]
    results: list[dict[str, Any]] = []
    identities: list[dict[str, Any]] = []
    for case in inventory:
        evidence: dict[str, Any] = {}
        try:
            with engine_factory() if engine_factory else Engine(executable, timeout=15) as engine:
                identity = {
                    key: engine.hello.get(key) for key in ("device", "interface", "chassis")
                }
                if identity not in identities:
                    identities.append(identity)
                session = Session(engine, glyphs)
                try:
                    case.function(session)
                finally:
                    evidence = {"commands": engine.transcript, **session.evidence}
            status = "pass"
            message = "observed behavior met this case"
        except ReviewNeeded as exc:
            if reviews and case.name in reviews:
                status = "pass"
                message = "explicit human approval; not an automated behavioral proof"
                evidence["human_review"] = reviews[case.name]
            else:
                status = "review"
                message = str(exc)
        except (AssertionError, EngineError, ValueError, KeyError, TypeError) as exc:
            status = "fail"
            message = str(exc)
        # Failed behavioral checks can never be overridden by a review record.
        if "last_trace" in evidence:
            trace = evidence.pop("last_trace")
            evidence["last_state"] = trace["state"]
            evidence["transition_events"] = trace.get("events", [])
        print(f"{case.name}: {status} — {message}", file=sys.stderr, flush=True)
        results.append(
            {
                "case": case.name,
                "source_messages": case.source,
                "status": status,
                "message": message,
                "evidence": evidence,
            }
        )
    counts = {k: sum(r["status"] == k for r in results) for k in ("pass", "fail", "review")}
    selected_passed = counts["fail"] == 0 and counts["review"] == 0
    return {
        "contract": "Half-A/Labs correspondence P5; published selected-device interface; chassis04",
        "observed_devices": identities,
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "counts": counts,
        "full_suite": complete_suite,
        "selected_passed": selected_passed,
        "accepted": complete_suite and selected_passed,
        "results": results,
        "limits": (
            "Finite observed cases plus identified human reviews, not a proof of all behavior. "
            "Physical HWIL, braking/guarding and certification are outside this suite. "
            "No fixed font or retired debounce/estimator algorithm required."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--glyphs", type=Path)
    parser.add_argument(
        "--reviews",
        type=Path,
        help="Explicit approved review records tied to this executable SHA-256",
    )
    parser.add_argument("--case", action="append", dest="selected")
    parser.add_argument(
        "--expect-incomplete",
        action="store_true",
        help="Harness self-test only: success requires a selected functional case to fail",
    )
    args = parser.parse_args()
    report = run(
        args.exe, args.selected, load_glyphs(args.glyphs), load_reviews(args.reviews, args.exe)
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
        )
    print(
        json.dumps(
            {
                "counts": report["counts"],
                "accepted": report["accepted"],
                "full_suite": report["full_suite"],
                "results": [
                    {k: r[k] for k in ("case", "status", "message")} for r in report["results"]
                ],
            },
            indent=2,
        )
    )
    if args.expect_incomplete:
        return 0 if report["counts"]["fail"] > 0 else 1
    return 0 if report["selected_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
