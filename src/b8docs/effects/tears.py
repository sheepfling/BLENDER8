"""Paper-edge notches with fibers. Never remove protected source content."""

from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from ..recipe import Tears


def dilate_mask(mask: Image.Image, radius: int) -> np.ndarray:
    """Square Boolean dilation in O(width * height), without a large rank filter."""
    data = np.asarray(mask.convert("L")) > 0
    if radius == 0:
        return data
    window = 2 * radius + 1
    padded = np.pad(data, ((0, 0), (radius, radius)), mode="constant")
    sums = np.pad(np.cumsum(padded, axis=1, dtype=np.int32), ((0, 0), (1, 0)))
    horizontal = (sums[:, window:] - sums[:, :-window]) > 0
    padded = np.pad(horizontal, ((radius, radius), (0, 0)), mode="constant")
    sums = np.pad(np.cumsum(padded, axis=0, dtype=np.int32), ((1, 0), (0, 0)))
    return (sums[window:, :] - sums[:-window, :]) > 0


def apply_tears(
    image: Image.Image,
    cfg: Tears,
    dpi: int,
    rng: np.random.Generator,
    protection_mask: Image.Image,
    bed_rgb: tuple[int, int, int],
) -> tuple[Image.Image, dict[str, Any]]:
    if not cfg.enabled:
        return image, {"enabled": False, "applied": [], "skipped": 0}
    out = image.convert("RGB").copy()
    w, h = out.size
    pxmm = dpi / 25.4
    guard = max(1, int(np.ceil((cfg.clearance_mm + 2 * cfg.fiber_mm) * pxmm)))
    forbidden = dilate_mask(protection_mask, guard)
    records: list[dict[str, Any]] = []
    requested = int(rng.integers(cfg.count.min, cfg.count.max + 1))
    for _ in range(requested):
        accepted = False
        for attempt in range(cfg.attempts_per_tear):
            edge = cfg.edges[int(rng.integers(len(cfg.edges)))]
            length_mm = float(rng.uniform(cfg.length_mm.min, cfg.length_mm.max))
            depth_mm = float(rng.uniform(cfg.depth_mm.min, cfg.depth_mm.max))
            length, depth = length_mm * pxmm, depth_mm * pxmm
            edge_extent = h if edge in ("left", "right") else w
            if length + 4 >= edge_extent:
                continue
            center = float(rng.uniform(length / 2 + 1, edge_extent - length / 2 - 1))
            # Jagged half-wave: width at the page edge, irregular apex in the margin.
            t = np.linspace(0, 1, 19)
            inward = depth * np.sin(np.pi * t) ** 0.72
            inward *= 1 + rng.uniform(-cfg.roughness, cfg.roughness, t.size)
            along = center + (t - 0.5) * length
            if edge == "left":
                line = list(zip(inward, along, strict=True))
            elif edge == "right":
                line = list(zip(w - 1 - inward, along, strict=True))
            elif edge == "top":
                line = list(zip(along, inward, strict=True))
            else:
                line = list(zip(along, h - 1 - inward, strict=True))
            points = [(round(float(x)), round(float(y))) for x, y in line]
            cut = Image.new("L", (w, h))
            ImageDraw.Draw(cut).polygon(points, fill=255)
            cut_pixels = np.asarray(cut) > 0
            if not cut_pixels.any() or np.any(cut_pixels & forbidden):
                continue
            # A soft seam on retained material, then the absent material and exposed fibers.
            shadow = Image.new("L", (w, h))
            d = ImageDraw.Draw(shadow)
            d.line(points, fill=155, width=max(2, round(0.55 * pxmm)))
            shadow = shadow.filter(ImageFilter.GaussianBlur(0.28 * pxmm))
            shade = Image.new("RGB", (w, h), (114, 111, 106))
            out = Image.composite(shade, out, shadow)
            out.paste(bed_rgb, (0, 0, w, h), cut)
            draw = ImageDraw.Draw(out)
            draw.line(points, fill=(244, 241, 232), width=max(1, round(cfg.fiber_mm * pxmm)))
            for i in range(1, len(points) - 1, 2):
                x, y = points[i]
                dx, dy = float(rng.uniform(-0.3, 0.3)) * pxmm, float(rng.uniform(-0.3, 0.3)) * pxmm
                draw.line((x, y, x + dx, y + dy), fill=(252, 250, 242), width=1)
            records.append(
                {
                    "edge": edge,
                    "depth_mm": depth_mm,
                    "length_mm": length_mm,
                    "polygon_px": [list(p) for p in points],
                    "removed_pixels": int(cut_pixels.sum()),
                    "attempt": attempt + 1,
                }
            )
            accepted = True
            break
        if not accepted:
            continue
    return out, {
        "enabled": True,
        "stage": cfg.stage,
        "requested": requested,
        "applied": records,
        "skipped": requested - len(records),
        "rule": "Reject material removal intersecting protected content plus clearance.",
    }
