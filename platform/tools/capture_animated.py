"""Capture real native C++ scene frames. Optional Pillow is required for PNG/GIF.

This is NOT a WebAssembly screenshot tool or a separate animation/physics model.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from animated_server import SceneEngine
from b8client import resolve_executable


def capture(build: Path, output: Path) -> None:
    from PIL import Image

    output.mkdir(parents=True, exist_ok=True)
    engine = SceneEngine(resolve_executable(build, "b8_view_host"), bench=True)
    frames: list[Image.Image] = []
    states: list[dict[str, Any]] = []
    try:
        engine.command("resize 960 675")
        # C++ owns the coordinate mapping and activates its explicit bench electrical script.
        engine.command("event 0 1 765 555")
        engine.command("event 1 1 765 555")
        for frame in range(140):
            if frame == 48:  # Contact opens while driven; actual hardware gate drops.
                engine.command("event 4 74 0 0")
                engine.command("event 5 74 0 0")
            if frame == 90:  # Reseating alone cannot restore permission.
                engine.command("event 4 74 0 0")
                engine.command("event 5 74 0 0")
            header, rgba = engine.command("frame 20")
            if not header["ok"]:
                raise RuntimeError(header)
            state = header["state"]
            frames.append(Image.frombytes("RGBA", (960, 675), rgba).convert("RGB"))
            states.append(state)
        frames[38].save(output / "native-motor-running.png")
        frames[60].save(output / "native-jar-coast.png")
        frames[0].save(
            output / "native-animation.gif",
            save_all=True,
            append_images=frames[1:],
            duration=40,
            loop=0,
            optimize=False,
        )
        # 20 ms simulated per frame, played back in 40 ms: intentionally half speed.
        (output / "native-animation.json").write_text(
            json.dumps(
                {
                    "backend": "native C++",
                    "not_wasm": True,
                    "simulated_ms_per_frame": 20,
                    "playback_ms_per_frame": 40,
                    "states": states,
                },
                indent=2,
            )
            + "\n"
        )
        (output / "native-animation-journal.json").write_text(
            json.dumps(engine.command("journal")[0]["journal"], indent=2) + "\n"
        )
    finally:
        engine.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    capture(args.build.resolve(), args.output.resolve())
