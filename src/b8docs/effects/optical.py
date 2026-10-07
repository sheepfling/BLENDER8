"""Scanner-coordinate vignette, fixed copier defects, and repeated optical loss."""

from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image, ImageFilter

from ..recipe import Repro, Streaks, Vignette
from .common import pixels, sample, to_image


def apply_repro(
    image: Image.Image, cfg: Repro, dpi: int, rng: np.random.Generator
) -> tuple[Image.Image, dict[str, Any]]:
    if not cfg.enabled:
        return image, {"enabled": False}
    blur = sample(cfg.blur_mm, rng)
    factor = sample(cfg.resample, rng)
    contrast = sample(cfg.contrast, rng)
    w, h = image.size
    out = image.filter(ImageFilter.GaussianBlur(blur * dpi / 25.4))
    if factor < 1.0:
        out = out.resize(
            (max(1, round(w * factor)), max(1, round(h * factor))), Image.Resampling.BOX
        ).resize((w, h), Image.Resampling.BICUBIC)
    # Blend morphological toner spread; not wholesale binary thresholding of symbols.
    if cfg.toner_bloom:
        out = Image.blend(out, out.filter(ImageFilter.MinFilter(3)), cfg.toner_bloom)
    arr = pixels(out)
    arr = np.clip((arr - 0.5) * contrast + 0.5, 0, 1)
    if cfg.threshold_mix:
        bilevel = (arr >= cfg.threshold / 255).astype(np.float32)
        arr = (1 - cfg.threshold_mix) * arr + cfg.threshold_mix * bilevel
    if cfg.grain_sigma:
        grain = rng.normal(0, cfg.grain_sigma / 255, (h, w, 1)).astype(np.float32)
        arr += grain
    return to_image(arr), {
        "enabled": True,
        "blur_mm": blur,
        "resample": factor,
        "contrast": contrast,
        "threshold_mix": cfg.threshold_mix,
        "toner_bloom": cfg.toner_bloom,
    }


def apply_vignette(
    image: Image.Image, cfg: Vignette, dpi: int, rng: np.random.Generator
) -> tuple[Image.Image, dict[str, Any]]:
    if not cfg.enabled:
        return image, {"enabled": False}
    w, h = image.size
    strength = sample(cfg.strength, rng)
    width = sample(cfg.width_mm, rng)
    edge = str(rng.choice(["left", "right", "top", "bottom"])) if cfg.edge == "random" else cfg.edge
    xx = np.arange(w, dtype=np.float32)[None, :] * 25.4 / dpi
    yy = np.arange(h, dtype=np.float32)[:, None] * 25.4 / dpi
    width_page, height_page = w * 25.4 / dpi, h * 25.4 / dpi
    distances = {"left": xx, "right": width_page - xx, "top": yy, "bottom": height_page - yy}
    falloff = np.zeros((h, w), dtype=np.float32)
    for name, distance in distances.items():
        weight = 1.0 if name == edge else (1 - cfg.asymmetry) * 0.55
        falloff += strength * weight * np.exp(-np.square(distance / width))
    # Irregular top-corner lid shadow, not a radially symmetric photo vignette.
    corner_x = 0.0 if edge != "right" else width_page
    corner_y = height_page if edge == "bottom" else 0.0
    falloff += cfg.corner_strength * np.exp(
        -(((xx - corner_x) / (width * 2.0)) ** 2) - ((yy - corner_y) / (width * 1.7)) ** 2
    )
    phase = float(rng.uniform(0, 6.283))
    falloff *= 0.90 + 0.10 * np.sin(yy / 21 + phase)
    out = pixels(image) * (1 - np.clip(falloff[:, :, None], 0, 0.65))
    return to_image(out), {
        "enabled": True,
        "edge": edge,
        "strength": strength,
        "width_mm": width,
        "phase": phase,
    }


def apply_streaks(
    image: Image.Image, cfg: Streaks, dpi: int, rng: np.random.Generator
) -> tuple[Image.Image, dict[str, Any]]:
    if not cfg.enabled:
        return image, {"enabled": False}
    w, h = image.size
    xx = np.arange(w, dtype=np.float32)[None, :] * 25.4 / dpi
    yy = np.arange(h, dtype=np.float32)[:, None] * 25.4 / dpi
    arr = pixels(image)
    records: list[dict[str, Any]] = []
    for orientation, count_range in (("horizontal", cfg.horizontal), ("vertical", cfg.vertical)):
        count = int(rng.integers(count_range.min, count_range.max + 1))
        for _ in range(count):
            center = float(rng.uniform(0.018, 0.982))
            width = sample(cfg.width_mm, rng)
            opacity = sample(cfg.opacity, rng)
            light = bool(rng.random() < cfg.light_fraction)
            phase = float(rng.uniform(0, 6.283))
            if orientation == "horizontal":
                distance = yy - center * h * 25.4 / dpi - cfg.waviness_mm * np.sin(xx / 32 + phase)
            else:
                distance = xx - center * w * 25.4 / dpi - cfg.waviness_mm * np.sin(yy / 40 + phase)
            band = opacity * np.exp(-0.5 * (distance / max(width / 2.355, 0.01)) ** 2)
            alpha = band[:, :, None]
            arr = arr * (1 - alpha) + (alpha if light else 0)
            records.append(
                {
                    "orientation": orientation,
                    "position_fraction": center,
                    "width_mm": width,
                    "opacity": opacity,
                    "light": light,
                    "phase": phase,
                }
            )
    roller_phase = float(rng.uniform(0, cfg.roller_pitch_mm))
    if cfg.roller_opacity:
        periodic = 0.5 + 0.5 * np.cos(2 * np.pi * (yy + roller_phase) / cfg.roller_pitch_mm)
        arr *= 1 - cfg.roller_opacity * periodic[:, :, None] ** 10
    return to_image(arr), {
        "enabled": True,
        "lines": records,
        "roller_phase_mm": roller_phase,
        "roller_pitch_mm": cfg.roller_pitch_mm,
    }
