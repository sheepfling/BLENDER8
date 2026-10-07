"""Protocol, scenario oracle, local browser server and honest host-timeout tests."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, cast

import acceptance
from b8client import Engine, EngineError, HostTimeout, resolve_executable
from scenario import evaluate, load_scenario, run_scenario

platform_root = Path(os.environ.get("B8_PLATFORM_ROOT", Path.cwd())).resolve()
build_dir = Path(os.environ.get("B8_TEST_BUILD", platform_root / "build"))


class ProtocolTests(unittest.TestCase):
    def test_handshake(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            self.assertEqual(e.hello["protocol"], 1)
            self.assertEqual(e.hello["chassis"], 4)
            self.assertFalse(e.state()["drive_enabled"])

    def test_chunking(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            e.run(100000)
            serial = e.state()["mcu"]["reset_serial"]
            for _ in range(5):
                e.run(10000)
            self.assertEqual(e.state()["mcu"]["reset_serial"], serial)

    def test_no_register_poke(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            with self.assertRaises(EngineError):
                e.command("write 0x29 1")
            self.assertEqual(e.state()["mcu"]["gpiob_out"], 0)

    def test_invalid_parse_recovers(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            for cmd in (
                "speed 9",
                "run -1",
                "trace 1000 0",
                "load nan",
                "pulse 1 extra",
                "nonsense",
            ):
                with self.assertRaises(EngineError):
                    e.command(cmd)
            self.assertEqual(e.state()["time_us"], 0)

    def test_timeline(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            e.command("schedule 50000 speed 3")
            self.assertEqual(e.run(60000)["contacts"], 4)

    def test_probe_pixels(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_probe_emulator")) as e:
            e.run(100000)
            e.command("speed 4")
            s = e.run(100000)
            self.assertIn("1", s["lcd_pixels"])
            self.assertFalse(s["drive_enabled"])

    def test_trace(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            t = e.trace(100000, 1000)
            self.assertEqual(len(t["samples"]), 100)
            self.assertTrue(t["events"])
            self.assertEqual(t["state"]["time_us"], 100000)

    def test_snapshot_nonmutating(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator"), bench=True) as e:
            e.run(60000)
            e.command("write 0xA7 195")
            a = e.state()
            e.state()
            e.command("write 0xA7 60")
            e.state()
            e.command("write 0xA8 165")
            self.assertEqual(e.state()["mcu"]["clock_status"] & 8, 0)
            self.assertEqual(a["time_us"], e.state()["time_us"])

    def test_host_timeout_not_wdt(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_hang_emulator"), timeout=0.5) as e:
            with self.assertRaisesRegex(HostTimeout, "not a simulated watchdog"):
                e.run(100000)
            self.assertIsNotNone(e.process.poll())

    def test_bus_trace(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bus.csv"
            with Engine(resolve_executable(build_dir, "b8_starter_emulator"), bus_trace=path) as e:
                e.run(40000)
            text = path.read_text()
            self.assertIn("time_us,operation,address,value,reset_serial", text)
            self.assertIn(",W,", text)

    def test_clock_missing_fixture(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator")) as e:
            e.command("clock_failed 1")
            s = e.run(100000)
            self.assertEqual(s["mcu"]["clock_status"] & 1, 0)

    def test_sensor_stall(self) -> None:
        with Engine(resolve_executable(build_dir, "b8_starter_emulator"), bench=True) as e:
            e.run(60000)
            e.command("adc_stalled 1")
            e.command("write 0x90 3")
            s = e.run(1000)
            self.assertEqual(s["mcu"]["adc_status"] & 3, 1)

    def test_scenario_oracle_rejects_wrong_expected(self) -> None:
        path = platform_root / "scenarios/conformance/power_qualification.json"
        data = load_scenario(path)
        data["checks"][0]["value"] = True
        with tempfile.TemporaryDirectory() as d:
            bad = Path(d) / "bad.json"
            bad.write_text(json.dumps(data))
            self.assertFalse(
                run_scenario(resolve_executable(build_dir, "b8_starter_emulator"), bad)["passed"]
            )

    def test_scenario_bad_time(self) -> None:
        data = {
            "version": 1,
            "name": "x",
            "mode": "bench",
            "events": [{"at_us": -1, "command": "jar 0"}],
            "checks": [],
        }
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bad.json"
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                load_scenario(path)

    def test_equality_types(self) -> None:
        self.assertFalse(evaluate(True, "eq", 1))
        self.assertTrue(evaluate(3, "between", [2, 4]))

    def test_product_starter_rejected(self) -> None:
        r = acceptance.run(
            resolve_executable(build_dir, "b8_starter_emulator"),
            ["startup.external_clock", "panel.speed_1"],
        )
        self.assertEqual(r["counts"]["fail"], 2)
        self.assertFalse(r["accepted"])

    def test_review_is_not_pass(self) -> None:
        r = acceptance.run(
            resolve_executable(build_dir, "b8_starter_emulator"), ["review.progress_authorization"]
        )
        self.assertEqual(r["counts"]["review"], 1)
        self.assertFalse(r["accepted"])

    def test_output_deadline_boundaries(self) -> None:
        def state(at: int, on: bool) -> dict[str, Any]:
            return {
                "time_us": at,
                "drive_enabled": on,
                "mcu": {
                    "gpiob_out": int(on),
                    "pwm_enabled": int(on),
                    "pwm_shadow": 128 if on else 0,
                },
            }

        session = acceptance.Session(cast(Engine, None), None)
        session.off_by(
            {"samples": [state(1, True), state(10000, False)], "state": state(10001, False)}, 10000
        )
        with self.assertRaises(AssertionError):
            session.off_by(
                {"samples": [state(1, True), state(10000, True)], "state": state(10001, False)},
                10000,
            )
        with self.assertRaises(AssertionError):
            session.off_by({"samples": [state(1, False)], "state": state(10001, True)}, 10000)

    def test_review_bound_to_binary(self) -> None:
        import hashlib

        exe = resolve_executable(build_dir, "b8_starter_emulator")
        entry = {
            "reviewer": "test fixture only",
            "approved_at": "2026-10-07T00:00:00Z",
            "evidence": "synthetic record validation, not product approval",
        }
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "review.json"
            path.write_text(
                json.dumps(
                    {
                        "executable_sha256": "wrong",
                        "approvals": {"review.progress_authorization": entry},
                    }
                )
            )
            with self.assertRaises(ValueError):
                acceptance.load_reviews(path, exe)
            path.write_text(
                json.dumps(
                    {
                        "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
                        "approvals": {"review.progress_authorization": entry},
                    }
                )
            )
            reviews = acceptance.load_reviews(path, exe)
            r = acceptance.run(exe, ["review.progress_authorization"], reviews=reviews)
            self.assertEqual(r["counts"]["pass"], 1)
            self.assertFalse(r["accepted"])  # a selected subset is never full product acceptance

    def test_bad_glyphs_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "glyphs.json"
            path.write_text('{"0":["0101"]}')
            with self.assertRaises(ValueError):
                acceptance.load_glyphs(path)

    def test_display_oracle_deadline(self) -> None:
        class Fake:
            def state(self):
                return {"time_us": 200, "lcd_pixels": "1" * 512}

        session = acceptance.Session(cast(Engine, Fake()), {"IL": ["1" * 512]})
        trace = {
            "samples": [
                {"time_us": 99, "lcd_pixels": "0" * 512},
                {"time_us": 100, "lcd_pixels": "1" * 512},
            ],
            "state": Fake().state(),
        }
        session.label("IL", trace, 100)
        with self.assertRaises(AssertionError):
            session.label("IL", trace, 99)

    def test_local_server_security(self) -> None:
        proc = subprocess.Popen(
            [
                sys.executable,
                str(platform_root / "tools/emulator.py"),
                "--root",
                str(platform_root),
                "--exe",
                str(resolve_executable(build_dir, "b8_probe_emulator")),
                "--port",
                "0",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            assert proc.stdout is not None
            line = proc.stdout.readline().strip()
            self.assertTrue(line.startswith("B8 front panel: "))
            url = line.split(": ", 1)[1]
            with urllib.request.urlopen(url, timeout=5) as reply:
                page = reply.read().decode()
            self.assertIn('canvas id="lcd"', page)
            self.assertNotIn("__B8_TOKEN__", page)
            req = urllib.request.Request(
                url + "/command",
                data=b'{"command":"jar 0"}',
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with self.assertRaises(urllib.error.HTTPError) as caught:
                urllib.request.urlopen(req, timeout=5)
            self.assertEqual(caught.exception.code, 403)
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(url + "/../../CMakeLists.txt", timeout=5)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=2)
            for stream in (proc.stdout, proc.stderr):
                if stream:
                    stream.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--build", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    platform_root = args.root.resolve()
    build_dir = args.build.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
