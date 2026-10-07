"""Exercise the real C++ scene process and HTTP path; no browser/Wasm substitution."""

from __future__ import annotations

import argparse
import json
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import time
import unittest
from pathlib import Path
from typing import Any, cast
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from animated_server import SceneEngine, operation
from b8client import resolve_executable

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
parser.add_argument("--build", required=True, type=Path)
ARGS, REST = parser.parse_known_args()
platform_root = ARGS.root.resolve()
build_dir = ARGS.build.resolve()


class NativeSceneTests(unittest.TestCase):
    def test_rgba_and_held_input(self) -> None:
        engine = SceneEngine(resolve_executable(build_dir, "b8_view_probe_host"))
        self.addCleanup(engine.close)
        header, pixels = engine.initial
        self.assertTrue(header["ok"])
        self.assertEqual(len(pixels), 1280 * 900 * 4)
        self.assertGreater(len(set(pixels)), 20)
        first = header["state"]["machine"]
        # Key P and pointer P are independent owners of the same momentary actuation.
        for cmd in ("event 4 80 0 0", "event 0 7 530 660", "event 5 80 0 0"):
            header, _ = engine.command(cmd)
        self.assertEqual(header["state"]["view"]["held_inputs"], 1)
        header, _ = engine.command("event 1 7 -99 -99")
        self.assertEqual(header["state"]["view"]["held_inputs"], 0)
        header, _ = engine.command("frame 0")
        self.assertTrue(header["ok"])
        self.assertEqual(header["state"]["machine"]["state"]["time_us"], first["state"]["time_us"])

    def test_invalid_dimensions_do_not_destroy_session(self) -> None:
        engine = SceneEngine(resolve_executable(build_dir, "b8_view_host"))
        self.addCleanup(engine.close)
        header, pixels = engine.command("resize 100000 900")
        self.assertFalse(header["ok"])
        self.assertEqual(pixels, b"")
        header, pixels = engine.command("resize 640 450")
        self.assertTrue(header["ok"])
        self.assertEqual(len(pixels), 640 * 450 * 4)

    def test_typed_operation(self) -> None:
        self.assertEqual(
            operation({"type": "event", "kind": 0, "identifier": 1, "x": 2.5, "y": 3}),
            "event 0 1 2.5 3",
        )
        for obj in (
            {"type": "event", "kind": True},
            {"type": "write"},
            {"type": "frame", "ms": float("nan")},
            {"type": "frame", "ms": True},
        ):
            with self.assertRaises(ValueError):
                operation(obj)


class NativeHTTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            cls.port = sock.getsockname()[1]
        cls.base = f"http://127.0.0.1:{cls.port}"
        cls.proc = subprocess.Popen(
            [
                sys.executable,
                str(platform_root / "tools/animated_server.py"),
                "--root",
                str(platform_root),
                "--build",
                str(build_dir),
                "--port",
                str(cls.port),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            creationflags=(
                cast(int, getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
                if os.name == "nt"
                else 0
            ),
        )
        for _ in range(100):
            try:
                with urlopen(cls.base, timeout=0.5) as response:
                    html = response.read().decode()
                token = re.search(r'data-token="([^"]+)"', html)
                if token is None:
                    raise RuntimeError("Native route did not provide a token")
                cls.token = token.group(1)
                if 'data-backend="native"' not in html:
                    raise RuntimeError("Native route must identify itself")
                return
            except OSError:
                time.sleep(0.05)
        cls.tearDownClass()
        raise RuntimeError("Native HTTP server did not start")

    @classmethod
    def tearDownClass(cls) -> None:
        # Request graceful shutdown so the server disposes its C++ child on each platform.
        try:
            cls.proc.send_signal(signal.CTRL_BREAK_EVENT if os.name == "nt" else signal.SIGINT)
        except (OSError, ValueError):
            cls.proc.terminate()
        try:
            cls.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            cls.proc.kill()
            cls.proc.wait()
        if cls.proc.stderr:
            cls.proc.stderr.close()

    def request(self, data: object, **headers: str) -> tuple[dict[str, Any], bytes]:
        merged = {
            "Origin": self.base,
            "X-B8-Token": self.token,
            "Content-Type": "application/json",
        } | headers
        req = Request(self.base + "/scene", data=json.dumps(data).encode(), headers=merged)
        with urlopen(req, timeout=20) as response:
            body = response.read()
        length = struct.unpack_from("<I", body)[0]
        return cast(dict[str, Any], json.loads(body[4 : 4 + length])), body[4 + length :]

    def test_actual_http_cpp_frames(self) -> None:
        h, p = self.request({"type": "init", "variant": "probe", "bench": False})
        self.assertTrue(h["ok"])
        self.assertEqual(len(p), h["bytes"])
        h, p = self.request({"type": "event", "kind": 4, "identifier": 51, "x": 0, "y": 0})
        self.assertTrue(h["ok"])
        h, p = self.request({"type": "resize", "width": 640, "height": 450})
        self.assertEqual(len(p), 640 * 450 * 4)
        h, p = self.request({"type": "journal"})
        self.assertEqual(p, b"")
        self.assertIn("speed 3", h["journal"]["commands"])
        self.assertTrue(h["journal"]["complete"])

    def test_missing_token_and_foreign_origin(self) -> None:
        for override in (
            {"X-B8-Token": "bad"},
            {"Origin": "https://other.example"},
            {"Host": "attacker.example"},
        ):
            with self.assertRaises(HTTPError) as error:
                self.request({"type": "status"}, **override)
            self.assertEqual(error.exception.code, 403)

    def test_bad_payloads(self) -> None:
        for obj, code in cast(
            tuple[tuple[object, int], ...],
            (
                ([], 400),
                ({"type": "frame", "ms": True}, 400),
                ({"type": "x", "data": "x" * 9000}, 413),
            ),
        ):
            with self.assertRaises(HTTPError) as error:
                self.request(obj)
            self.assertEqual(error.exception.code, code)

    def test_asset_allowlist(self) -> None:
        for path in ("/debug-log.mjs", "/debug-panel.mjs"):
            with urlopen(self.base + path, timeout=3) as response:
                self.assertEqual(response.status, 200)
                self.assertIn("javascript", response.headers.get_content_type())
        for path in (
            "/../CMakeLists.txt",
            "/scene-worker.mjs",
            "/b8_probe.wasm",
            "/native-scene-worker.mjs/../secret",
        ):
            with self.assertRaises(HTTPError) as error:
                urlopen(self.base + path, timeout=3)
            self.assertEqual(error.exception.code, 404)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *REST], verbosity=2)
