"""Tooling-only checks. Temporary WASM headers are fixtures, NOT compiled C++ modules."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import patch

from wasm_check import compare
from wasm_server import verified_assets
from wasm_site import MODULES, assemble

platform_root = Path.cwd().resolve()


class WasmToolingTests(unittest.TestCase):
    def fixture(self, root: Path) -> Path:
        web = root / "web"
        web.mkdir(parents=True)
        for name in MODULES:
            (web / (name + ".wasm")).write_bytes(b"\x00asm\x01\x00\x00\x00")
            (web / (name + ".mjs")).write_text("// UNIT-TEST FIXTURE; contains no C++ simulation\n")
        (web / "b8_hang.wasm").write_bytes(b"not-for-publication")
        with patch("wasm_site.subprocess.check_output", return_value="emcc (UNIT TEST FIXTURE)\n"):
            assemble(platform_root, root, "fixture-compiler")
        return root / "site"

    def test_missing_products_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileNotFoundError):
                assemble(platform_root, Path(temp), "fixture")
            with self.assertRaises(FileNotFoundError):
                verified_assets(Path(temp))

    def test_allowlist_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            site = self.fixture(Path(temp))
            assets = verified_assets(site)
            self.assertIn("/index.html", assets)
            self.assertIn("/b8_probe.wasm", assets)
            self.assertNotIn("/b8_hang.wasm", assets)
            self.assertNotIn("/CMakeLists.txt", assets)
            self.assertNotIn("__B8_TOKEN__", assets["/index.html"].decode())
            data = json.loads(assets["/build-info.json"])
            self.assertEqual(data["bridge_abi"], 1)
            self.assertEqual(data["scene_abi"], 1)
            self.assertIn("/scene-worker.mjs", assets)
            self.assertIn("/debug-log.mjs", assets)
            self.assertIn("/debug-panel.mjs", assets)
            self.assertIn("/diagnostics.html", assets)
            self.assertNotIn("/native-scene-worker.mjs", assets)
            self.assertIn("scene-shell.mjs", assets["/index.html"].decode())

    def test_tamper_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            site = self.fixture(Path(temp))
            (site / "b8_probe.wasm").write_bytes(b"wrong")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                verified_assets(site)

    def test_output_guard(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            with self.assertRaises(ValueError):
                assemble(platform_root, build, "unused", platform_root)
            output = build / "manual-files"
            output.mkdir()
            (output / "keep.txt").write_text("keep")
            with self.assertRaises(ValueError):
                assemble(platform_root, build, "unused", output)
            self.assertTrue((output / "keep.txt").is_file())

    def test_comparator_exact_discrete(self) -> None:
        with self.assertRaises(AssertionError):
            compare({"time_us": 1}, {"time_us": 1.0000000001})
        with self.assertRaises(AssertionError):
            compare({"contacts": 1}, {"contacts": True})
        with self.assertRaises(AssertionError):
            compare({"lcd_pixels": "10"}, {"lcd_pixels": "01"})
        with self.assertRaises(AssertionError):
            compare({"mcu": {"adc_result": 100}}, {"mcu": {"adc_result": 101}})

    def test_comparator_float_bounds(self) -> None:
        compare({"motor": {"rpm": 1000.0}}, {"motor": {"rpm": 1000.000000001}})
        with self.assertRaises(AssertionError):
            compare({"motor": {"rpm": 1000}}, {"motor": {"rpm": 1000.01}})
        with self.assertRaises(AssertionError):
            compare({"motor": {"rpm": float("nan")}}, {"motor": {"rpm": 0}})
        with self.assertRaises(AssertionError):
            compare({"sensor_c": None}, {"sensor_c": 25})

    def test_server_static_only_actual_http(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            site = self.fixture(Path(temp))
            proc = subprocess.Popen(
                [
                    sys.executable,
                    str(platform_root / "tools/wasm_server.py"),
                    str(site),
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
                self.assertTrue(line.startswith("B8 WebAssembly: "), line)
                url = line.split(": ", 1)[1].split("?", 1)[0]
                with urllib.request.urlopen(url + "b8_probe.wasm", timeout=5) as response:
                    self.assertEqual(response.headers.get_content_type(), "application/wasm")
                    self.assertEqual(response.read()[:4], b"\0asm")
                    self.assertIn("wasm-unsafe-eval", response.headers["Content-Security-Policy"])
                with urllib.request.urlopen(url + "transport.mjs", timeout=5) as response:
                    self.assertEqual(response.headers.get_content_type(), "text/javascript")
                for path in ("../../CMakeLists.txt", "b8_hang.wasm", "command"):
                    with self.assertRaises(urllib.error.HTTPError):
                        urllib.request.urlopen(url + path, timeout=5)
                request = urllib.request.Request(url + "command", data=b"{}", method="POST")
                with self.assertRaises(urllib.error.HTTPError) as caught:
                    urllib.request.urlopen(request, timeout=5)
                self.assertEqual(caught.exception.code, 405)
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
    import argparse

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args, remaining = parser.parse_known_args()
    platform_root = args.root.resolve()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
