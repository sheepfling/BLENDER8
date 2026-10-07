"""Loopback-only browser front panel.

Physical stimuli only; live observations never enter firmware.
"""

from __future__ import annotations

import argparse
import json
import secrets
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, cast

from b8client import Engine, EngineError


def serve(
    executable: Path,
    root: Path,
    port: int = 8088,
    *,
    bench: bool = False,
    open_browser: bool = False,
) -> None:
    if not 0 <= port <= 65535:
        raise ValueError("port out of range")
    token = secrets.token_urlsafe(32)
    lock = threading.Lock()
    with Engine(executable, bench=bench) as engine:

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, format: str, *args: Any) -> None:
                pass

            def reply(self, code: int, body: bytes, content_type: str) -> None:
                self.send_response(code)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                self.wfile.write(body)

            def valid_host(self) -> bool:
                port = cast(ThreadingHTTPServer, self.server).server_port
                return self.headers.get("Host", "") in {
                    f"127.0.0.1:{port}",
                    f"localhost:{port}",
                }

            def do_GET(self) -> None:
                if not self.valid_host() or self.path != "/":
                    self.reply(404, b"not found", "text/plain")
                    return
                page = (root / "ui/index.html").read_text(encoding="utf-8")
                page = page.replace("__B8_TOKEN__", json.dumps(token)).replace(
                    "__B8_HELLO__", json.dumps(engine.hello)
                )
                self.reply(200, page.encode(), "text/html; charset=utf-8")

            def do_POST(self) -> None:
                port = cast(ThreadingHTTPServer, self.server).server_port
                allowed_origins = {
                    f"http://127.0.0.1:{port}",
                    f"http://localhost:{port}",
                }
                if (
                    not self.valid_host()
                    or self.path != "/command"
                    or self.headers.get("X-B8-Token") != token
                    or self.headers.get("Origin") not in allowed_origins
                    or self.headers.get("Content-Type") != "application/json"
                ):
                    self.reply(403, b"forbidden", "text/plain")
                    return
                try:
                    length = int(self.headers.get("Content-Length", "0"))
                    if not 0 < length <= 8192:
                        raise ValueError("request size")
                    request = json.loads(self.rfile.read(length))
                    if not isinstance(request, dict) or set(cast(dict[Any, Any], request)) != {
                        "command"
                    }:
                        raise ValueError("command object required")
                    command = cast(dict[str, Any], request).get("command")
                    if not isinstance(command, str):
                        raise ValueError("command string required")
                    with lock:
                        result = engine.command(command)
                    self.reply(
                        200, json.dumps(result, allow_nan=False).encode(), "application/json"
                    )
                except (ValueError, EngineError, OSError) as exc:
                    self.reply(
                        400,
                        json.dumps({"ok": False, "error": str(exc)}).encode(),
                        "application/json",
                    )

        server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
        server.daemon_threads = True
        address = f"http://127.0.0.1:{server.server_port}"
        print(
            f"B8 front panel: {address}\nFirmware: {engine.hello['firmware']}\n"
            "Logical time; no physical hardware connected. Ctrl-C exits.",
            flush=True,
        )
        if open_browser:
            webbrowser.open(address)
        try:
            server.serve_forever(poll_interval=0.1)
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--open", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "ui/index.html").is_file():
        parser.error(f"not a B8 platform root: {root}")
    serve(args.exe, root, args.port, bench=args.bench, open_browser=args.open)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
