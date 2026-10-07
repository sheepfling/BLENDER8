"""No-crop affine placement; records the real source-to-output transform."""

from __future__ import annotations

import math
from typing import Any

import numpy as np
from PIL import Image

from ..recipe import Placement
from .common import Matrix, sample


def plan_placement(
    size: tuple[int, int], dpi: int, config: Placement, rng: np.random.Generator
) -> tuple[Matrix, dict[str, Any]]:
    w, h = size
    if not config.enabled:
        return np.eye(3), {
            "enabled": False,
            "matrix": np.eye(3).tolist(),
            "effective_scale": 1.0,
            "rotation_deg": 0.0,
        }
    angle = sample(config.rotation_deg, rng)
    shear = sample(config.shear_deg, rng)
    requested_scale = sample(config.scale, rng)
    dx = sample(config.translate_x_mm, rng) * dpi / 25.4
    dy = sample(config.translate_y_mm, rng) * dpi / 25.4
    inset = config.inset_mm * dpi / 25.4
    if 2 * inset >= min(w, h) - 2:
        raise ValueError("Page too small for requested inset.")
    # Positive rotation is clockwise in top-left image coordinates (y increases down).
    theta = math.radians(angle)
    rotation = np.array([[math.cos(theta), -math.sin(theta)], [math.sin(theta), math.cos(theta)]])
    linear = rotation @ np.array([[1.0, math.tan(math.radians(shear))], [0.0, 1.0]])
    # Use outer pixel boundaries rather than only pixel centers to retain all edges.
    corners = np.array([[0.0, 0.0], [float(w), 0.0], [float(w), float(h)], [0.0, float(h)]])
    center = np.array([w / 2, h / 2])
    warped = (corners - center) @ linear.T
    extent = np.ptp(warped, axis=0)
    scale = min(requested_scale, (w - 2 * inset) / extent[0], (h - 2 * inset) / extent[1])
    linear *= scale
    warped = (corners - center) @ linear.T
    lo, hi = warped.min(axis=0), warped.max(axis=0)
    # Clamp translation to whatever room remains after containment.
    safe_dx = float(np.clip(dx, inset - center[0] - lo[0], w - inset - center[0] - hi[0]))
    safe_dy = float(np.clip(dy, inset - center[1] - lo[1], h - inset - center[1] - hi[1]))
    matrix = np.eye(3)
    matrix[:2, :2] = linear
    matrix[:2, 2] = center + [safe_dx, safe_dy] - linear @ center  # noqa: RUF005
    final_corners = corners @ linear.T + matrix[:2, 2]
    record = {
        "enabled": True,
        "rotation_deg": angle,
        "shear_deg": shear,
        "requested_scale": requested_scale,
        "effective_scale": float(scale),
        "requested_translation_mm": [dx * 25.4 / dpi, dy * 25.4 / dpi],
        "effective_translation_mm": [safe_dx * 25.4 / dpi, safe_dy * 25.4 / dpi],
        "source_to_output": matrix.tolist(),
        "corners_px": final_corners.tolist(),
        "inset_mm": config.inset_mm,
    }
    return matrix, record


def warp(
    image: Image.Image, matrix: Matrix, fill: tuple[int, int, int] | int, *, mask: bool = False
) -> Image.Image:
    if np.array_equal(matrix, np.eye(3)):
        return image.copy()
    inverse = np.linalg.inv(matrix)
    coefficients = tuple(float(v) for v in inverse[:2].flat)
    return image.transform(
        image.size,
        Image.Transform.AFFINE,
        coefficients,
        resample=Image.Resampling.NEAREST if mask else Image.Resampling.BICUBIC,
        fillcolor=fill,
    )
