"""Explicit native C++ animated preview. No JavaScript model and no claim of Wasm execution."""

from __future__ import annotations

import argparse
import json
import math
import queue
import secrets
import struct
import subprocess
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, cast
from urllib.parse import urlsplit

from b8client import resolve_executable

MAX_BYTES = 1920 * 1440 * 4


class SceneEngine:
    def __init__(self, executable: Path, bench: bool = False) -> None:
        self.process = subprocess.Popen(
            [str(executable), *(["--bench"] if bench else [])],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        self.replies: queue.Queue[tuple[dict[str, Any], bytes] | Exception] = queue.Queue(maxsize=2)
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()
        self.initial = self._next()

    def _read(self) -> None:
        try:
            assert self.process.stdout is not None
            while True:
                line = self.process.stdout.readline(1_000_001)
                if not line:
                    raise RuntimeError("Native renderer exited")
                if len(line) > 1_000_000 or not line.endswith(b"\n"):
                    raise RuntimeError("Native reply header too large")
                data = json.loads(line)
                count = data.get("bytes")
                if type(count) is not int or not 0 <= count <= MAX_BYTES:
                    raise RuntimeError("Invalid native frame length")
                pixels = self.process.stdout.read(count)
                if len(pixels) != count:
                    raise RuntimeError("Truncated native frame")
                self.replies.put((data, pixels), timeout=2)
        except Exception as error:
            try:
                self.replies.put(error, timeout=2)
            except queue.Full:
                pass

    def _next(self) -> tuple[dict[str, Any], bytes]:
        try:
            result = self.replies.get(timeout=10)
        except queue.Empty as error:
            self.close()
            raise TimeoutError("NATIVE_HOST_TIMEOUT, not emulated watchdog recovery") from error
        if isinstance(result, Exception):
            self.close()
            raise result
        return result

    def command(self, line: str) -> tuple[dict[str, Any], bytes]:
        if "\n" in line or len(line) > 4096 or self.process.poll() is not None:
            raise ValueError("Invalid/dead native request")
        assert self.process.stdin is not None
        self.process.stdin.write((line + "\n").encode())
        self.process.stdin.flush()
        return self._next()

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=5)
        for stream in (self.process.stdin, self.process.stdout):
            if stream:
                stream.close()


def operation(data: dict[str, Any]) -> str:
    kind = data.get("type")
    if kind in {"status", "journal"}:
        return kind
    fields = {
        "frame": ("ms",),
        "event": ("kind", "identifier", "x", "y"),
        "resize": ("width", "height"),
    }
    if kind not in fields:
        raise ValueError("Unknown request")
    args: list[str] = []
    for key in fields[kind]:
        value = data.get(key)
        if type(value) not in (int, float) or not math.isfinite(cast(float, value)):
            raise ValueError("Finite numeric request required")
        if key not in {"ms", "x", "y"} and type(value) is not int:
            raise ValueError("Integer request field required")
        args.append(str(value))
    return str(kind) + " " + " ".join(args)


def serve(
    build: Path, root: Path, port: int = 8088, mode: str = "probe", open_browser: bool = False
) -> None:
    if mode not in {"probe", "student", "bench"} or not 0 <= port <= 65535:
        raise ValueError("Invalid mode/port")
    token = secrets.token_urlsafe(32)
    engine: SceneEngine | None = None
    lock = threading.Lock()
    assets = {
        ("/" + n): (root / "webview" / n).read_bytes()
        for n in ("scene-shell.mjs", "native-scene-worker.mjs")
    }
    html = (root / "webview/animated.html").read_text()
    html = html.replace("<body>", f'<body data-backend="native" data-token="{token}">')
    html = html.replace(
        "No network simulation service.",
        "Explicit native verification route: one local C++ process.",
    )
    html = html.replace(
        'Component and firmware diagnostic views remain available at <a href="diagnostics.html">the original workbench</a>.',
        "Use tools/b8.py serve for the separate native diagnostic workbench.",
    )
    assets["/"] = html.encode()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format: str, *args: Any) -> None:
            pass

        def valid_host(self) -> bool:
            p = cast(ThreadingHTTPServer, self.server).server_port
            return self.headers.get("Host") in {f"127.0.0.1:{p}", f"localhost:{p}"}

        def do_GET(self) -> None:
            path = urlsplit(self.path).path
            if not self.valid_host() or path not in assets:
                self.send_error(404)
                return
            body = assets[path]
            self.send_response(200)
            self.send_header("Content-Type", "text/html" if path == "/" else "text/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header(
                "Content-Security-Policy",
                "default-src 'self'; script-src 'self'; worker-src 'self'; style-src 'unsafe-inline'; object-src 'none'; frame-ancestors 'none'",
            )
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self) -> None:
            nonlocal engine
            try:
                origins = {
                    f"http://127.0.0.1:{cast(ThreadingHTTPServer, self.server).server_port}",
                    f"http://localhost:{cast(ThreadingHTTPServer, self.server).server_port}",
                }
                if (
                    not self.valid_host()
                    or self.path != "/scene"
                    or self.headers.get("Origin") not in origins
                    or self.headers.get("X-B8-Token") != token
                ):
                    self.send_error(403)
                    return
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 8192:
                    self.send_error(413)
                    return
                data = json.loads(self.rfile.read(length))
                if not isinstance(data, dict):
                    raise ValueError("Request object required")
                request = cast(dict[str, Any], data)
                with lock:
                    if request.get("type") == "init":
                        variant, bench = request.get("variant"), request.get("bench")
                        if variant not in {"probe", "student"} or type(bench) is not bool:
                            raise ValueError("Invalid native image")
                        if engine:
                            engine.close()
                        exe = resolve_executable(
                            build, "b8_view_probe_host" if variant == "probe" else "b8_view_host"
                        )
                        engine = SceneEngine(exe, bench)
                        header, pixels = engine.initial
                    else:
                        if engine is None:
                            raise ValueError("Initialize scene first")
                        header, pixels = engine.command(operation(request))
                h = json.dumps(header, separators=(",", ":")).encode()
                body = struct.pack("<I", len(h)) + h + pixels
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(body)
            except (ValueError, RuntimeError, OSError, TimeoutError) as error:
                self.send_error(400, str(error))

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    server.daemon_threads = True
    url = f"http://127.0.0.1:{server.server_port}/?mode={mode}"
    print(f"B8 native animated preview: {url}", flush=True)
    if open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever(poll_interval=0.1)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        if engine:
            engine.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--mode", choices=("probe", "student", "bench"), default="probe")
    parser.add_argument("--open", action="store_true")
    args = parser.parse_args()
    serve(args.build.resolve(), args.root.resolve(), args.port, args.mode, args.open)
