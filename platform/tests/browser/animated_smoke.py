"""Optional real-browser gate: requires a REAL compiled static site and Playwright.

Not included as a passing CTest on hosts without the compiler/browser. This starts only
an HTTP static server; no native simulation process and no command proxy are used.
"""

from __future__ import annotations

import argparse
import importlib
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any
from urllib.request import urlopen


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="platform root")
    parser.add_argument("--site", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8089)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--browser-executable", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output or root / "reports/browser-scene"
    site = args.site.resolve()
    subprocess.run(
        [sys.executable, str(root / "tools/wasm_server.py"), str(site), "--check"], check=True
    )
    # Reject placeholder eight-byte headers independently of generic asset validation.
    if (site / "b8_probe.wasm").stat().st_size < 1024:
        raise RuntimeError("Actual compiled C++ output required")
    sync_playwright = importlib.import_module("playwright.sync_api").sync_playwright

    output.mkdir(parents=True, exist_ok=True)
    server = subprocess.Popen(
        [
            sys.executable,
            str(root / "tools/b8.py"),
            "--root",
            str(root),
            "wasm-serve",
            "--site",
            str(site),
            "--port",
            str(args.port),
            "--probe",
        ],
        stdout=subprocess.DEVNULL,
    )
    try:
        url = f"http://127.0.0.1:{args.port}/?mode=probe"
        for _ in range(100):
            try:
                urlopen(url, timeout=0.5).close()
                break
            except OSError:
                time.sleep(0.05)
        with sync_playwright() as p:
            browser = p.chromium.launch(
                **(
                    {"executable_path": str(args.browser_executable)}
                    if args.browser_executable
                    else {}
                )
            )
            page = browser.new_page(viewport={"width": 1320, "height": 1030})
            errors: list[str] = []
            posts: list[str] = []

            def record_error(error: Any) -> None:
                errors.append(str(error))

            def record_request(request: Any) -> None:
                if request.method == "POST":
                    posts.append(str(request.url))

            page.on("pageerror", record_error)
            page.on("request", record_request)
            page.goto(url)
            page.wait_for_function(
                "document.querySelector('#status').textContent.includes('WASM / C++')",
                timeout=30000,
            )
            canvas = page.locator("canvas")
            box = canvas.bounding_box()
            assert box

            def click(x: float, y: float) -> None:
                page.mouse.click(
                    box["x"] + x * box["width"] / 1280, box["y"] + y * box["height"] / 900
                )

            click(390, 116)  # Run: C++ pacing control, not JS simulation.
            page.wait_for_timeout(250)
            click(270, 660)  # Select physical speed 3, through C++ hit-testing.
            page.wait_for_timeout(250)
            page.screenshot(path=str(output / "appliance-wasm.png"))
            click(236, 116)
            page.screenshot(path=str(output / "chassis-wasm.png"))
            # Actual key/mouse releases, resize and focus transitions in the browser.
            canvas.focus()
            page.keyboard.down("p")
            page.keyboard.press("Space")
            page.keyboard.up("p")
            page.set_viewport_size({"width": 760, "height": 820})
            page.locator("#journal").focus()
            page.wait_for_timeout(50)
            canvas.focus()
            assert not errors, errors
            assert not posts, f"Browser app contacted a simulation endpoint: {posts}"
            result: dict[str, Any] = {
                "status": "passed",
                "browser": browser.version,
                "site": str(site),
                "errors": errors,
                "post_requests": posts,
                "scope": "Real compiled-Wasm load, drawing, raw controls, focus and resize smoke; native/Wasm machine parity is the separate wasm-test gate.",
            }
            (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
            browser.close()
    finally:
        server.terminate()
        server.wait(timeout=10)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
