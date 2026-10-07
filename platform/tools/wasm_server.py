"""Static-files-only loopback server. No native process, command API, uploads or remote assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, cast
from urllib.parse import urlsplit

MIME = {
    ".html": "text/html; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".wasm": "application/wasm",
    ".json": "application/json",
}


def verified_assets(site: Path) -> dict[str, bytes]:
    manifest = site / "build-info.json"
    if not manifest.is_file():
        raise FileNotFoundError(f"No compiled Wasm site at {site}. Run wasm-build first.")
    data = json.loads(manifest.read_text(encoding="utf-8"))
    if data.get("schema") != 1 or data.get("backend") != "wasm":
        raise ValueError("invalid web build manifest")
    assets = {"/build-info.json": manifest.read_bytes()}
    for record in data["files"]:
        name = record["path"]
        if (
            not isinstance(name, str)
            or "/" in name
            or "\\" in name
            or name.startswith(".")
            or Path(name).suffix not in MIME
        ):
            raise ValueError("invalid web asset name")
        path = site / name
        if path.is_symlink():
            raise ValueError("symlink is not a static release asset")
        content = path.read_bytes()
        if (
            len(content) != record["bytes"]
            or hashlib.sha256(content).hexdigest() != record["sha256"]
        ):
            raise ValueError(f"web asset hash mismatch: {name}; rebuild site")
        assets["/" + name] = content
    if "/index.html" not in assets:
        raise ValueError("missing workbench entry")
    # Snapshot all bytes at startup. A concurrent rebuild cannot mix module versions.
    assets["/"] = assets["/index.html"]
    return assets


def serve(site: Path, port: int = 8088, *, mode: str = "probe", open_browser: bool = False) -> None:
    if mode not in {"student", "probe", "bench"} or not 0 <= port <= 65535:
        raise ValueError("invalid server mode/port")
    assets = verified_assets(site.resolve())

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format: str, *args: Any) -> None:
            pass

        def do_GET(self) -> None:
            port = cast(ThreadingHTTPServer, self.server).server_port
            hosts = {f"localhost:{port}", f"127.0.0.1:{port}"}
            path = urlsplit(self.path).path
            if self.headers.get("Host", "") not in hosts or path not in assets:
                self.send_error(404)
                return
            payload = assets[path]
            self.send_response(200)
            self.send_header(
                "Content-Type", MIME.get(Path(path).suffix, "text/html; charset=utf-8")
            )
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header(
                "Content-Security-Policy",
                "default-src 'self'; script-src 'self' blob: 'wasm-unsafe-eval'; worker-src 'self'; connect-src 'self'; style-src 'self' 'unsafe-inline'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
            )
            self.end_headers()
            self.wfile.write(payload)

        def do_POST(self) -> None:
            self.send_error(405, "This is a static Wasm site; no command service exists")

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    server.daemon_threads = True
    address = f"http://127.0.0.1:{server.server_port}/?mode={mode}"
    print(
        f"B8 WebAssembly: {address}\nStatic assets only; C++ executes inside a browser worker. Ctrl-C exits.",
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--mode", choices=("probe", "student", "bench"), default="probe")
    parser.add_argument("--open", action="store_true")
    parser.add_argument("--check", action="store_true", help="validate a compiled static site")
    args = parser.parse_args()
    if args.check:
        verified_assets(args.site.resolve())
    else:
        serve(args.site, args.port, mode=args.mode, open_browser=args.open)
