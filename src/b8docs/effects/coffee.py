"""Irregular absorbent cup rings and droplets in physical paper coordinates."""

from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image

from ..recipe import Coffee
from .common import pixels, to_image


def apply_coffee(
    image: Image.Image,
    cfg: Coffee,
    opacity: float,
    monochrome: bool,
    dpi: int,
    rng: np.random.Generator,
) -> tuple[Image.Image, dict[str, Any]]:
    w, h = image.size
    px_per_mm = dpi / 25.4
    radius_mm = float(rng.uniform(cfg.radius_mm.min, cfg.radius_mm.max))
    radius = radius_mm * px_per_mm
    cx = float(rng.uniform(cfg.center_x.min, cfg.center_x.max)) * w
    cy = float(rng.uniform(cfg.center_y.min, cfg.center_y.max)) * h
    yy, xx = np.ogrid[:h, :w]
    dx = (xx - cx) / radius
    dy = (yy - cy) / radius
    r = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)
    phases = rng.uniform(0, 2 * np.pi, 4)
    radius_wobble = 1 + cfg.irregularity * (
        0.60 * np.sin(5 * theta + phases[0])
        + 0.28 * np.sin(13 * theta + phases[1])
        + 0.12 * np.sin(37 * theta + phases[2])
    )
    rim_width = cfg.rim_width_mm / radius_mm
    patchiness = 1 - cfg.broken_rim * (0.5 + 0.5 * np.sin(3 * theta + phases[3])) ** 3
    outer = np.exp(-(((r - radius_wobble) / rim_width) ** 2)) * patchiness
    # A second pale, offset contact trace and capillary bloom are not concentric hard circles.
    capillary = np.exp(-(((r - radius_wobble - 0.018) / (rim_width * 3)) ** 2)) * 0.17
    wet_inside = 0.08 * np.exp(-(((r - 0.91) / 0.11) ** 2)) * patchiness
    stain = (outer + capillary + wet_inside) * opacity
    drops: list[dict[str, Any]] = []
    count = int(rng.integers(cfg.droplets.min, cfg.droplets.max + 1))
    for _ in range(count):
        angle = float(rng.uniform(-np.pi, np.pi))
        rho = float(rng.uniform(1.05, 1.48)) * radius
        x, y = cx + rho * np.cos(angle), cy + rho * np.sin(angle)
        drop_mm = float(rng.uniform(0.25, 1.1))
        dropr = drop_mm * px_per_mm
        dist = ((xx - x) / dropr) ** 2 + ((yy - y) / dropr) ** 2
        stain += opacity * 0.45 * np.exp(-dist * 1.5)
        drops.append({"center_px": [float(x), float(y)], "radius_mm": drop_mm})
    stain = np.clip(stain, 0, 0.65).astype(np.float32)
    color = np.array((0.40, 0.40, 0.40) if monochrome else (0.45, 0.28, 0.12), dtype=np.float32)
    arr = pixels(image)
    arr = arr * (1 - stain[:, :, None]) + color * stain[:, :, None]
    return to_image(arr), {
        "radius_mm": radius_mm,
        "center_px": [cx, cy],
        "rim_width_mm": cfg.rim_width_mm,
        "opacity": opacity,
        "droplets": drops,
        "monochrome": monochrome,
    }
