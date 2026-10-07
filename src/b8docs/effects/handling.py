"""Optional paper-attached artifacts. Drawn once, before or after the copy chain."""

from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from ..recipe import Handling, Paper
from .coffee import apply_coffee
from .common import pixels, to_image


def apply_paper(image: Image.Image, cfg: Paper) -> Image.Image:
    source = image.convert("L").convert("RGB") if cfg.monochrome else image.convert("RGB")
    return to_image(pixels(source) * (np.array(cfg.rgb, dtype=np.float32) / 255))


def apply_handling(
    image: Image.Image,
    cfg: Handling,
    monochrome: bool,
    dpi: int,
    rng: np.random.Generator,
    records: dict[str, Any] | None = None,
) -> Image.Image:
    if not cfg.enabled:
        return image
    w, h = image.size
    yy = np.linspace(0, 1, h, dtype=np.float32)[:, None]
    xx = np.linspace(0, 1, w, dtype=np.float32)[None, :]
    arr = pixels(image)
    if cfg.coffee:
        image, coffee_record = apply_coffee(
            image, cfg.coffee_profile, cfg.opacity, monochrome, dpi, rng
        )
        arr = pixels(image)
        if records is not None:
            records["coffee"] = coffee_record
    if cfg.fold:
        line = 0.055 * np.exp(-(((yy - 0.51 - 0.013 * xx) / 0.0018) ** 2))
        arr *= 1 - line[:, :, None]
    out = to_image(arr)
    draw = ImageDraw.Draw(out)
    if cfg.staple:
        x, y = w * 0.028, h * 0.022
        draw.line(
            (x + 2, y + 3, x + w * 0.035 + 2, y + h * 0.007 + 3),
            fill=(155, 155, 152),
            width=max(2, round(dpi * 0.025)),
        )
        draw.line(
            (x, y, x + w * 0.035, y + h * 0.007),
            fill=(81, 81, 79),
            width=max(1, round(dpi * 0.012)),
        )
    if cfg.punch_holes:
        r = max(2, round(2.5 * dpi / 25.4))
        for fraction in (0.23, 0.50, 0.77):
            x, y = round(w * 0.025), round(h * fraction)
            draw.ellipse((x - r - 2, y - r, x + r + 3, y + r + 3), fill=(159, 159, 155))
            draw.ellipse((x - r, y - r, x + r, y + r), fill=(224, 224, 220))
    for _ in range(cfg.dust_count):
        x, y = int(rng.integers(0, w)), int(rng.integers(0, h))
        radius = max(1, round(float(rng.uniform(0.05, 0.12)) * dpi / 25.4))
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=(153, 150, 145))
    if cfg.stamp:
        font = ImageFont.load_default(size=max(8, round(dpi * 6.5 / 72)))
        bbox = font.getbbox(cfg.stamp)
        pad = max(2, round(dpi / 100))
        label = Image.new("RGBA", (int(bbox[2]) + 2 * pad, int(bbox[3]) + 2 * pad), (0, 0, 0, 0))
        pen = ImageDraw.Draw(label)
        color = (75, 75, 73, 145) if monochrome else (120, 71, 42, 155)
        pen.rectangle(
            (0, 0, label.width - 1, label.height - 1), outline=color, width=max(1, pad // 2)
        )
        pen.text((pad, pad), cfg.stamp, fill=color, font=font)
        label = label.rotate(90, expand=True)
        out.paste(label, (round(w * 0.009), round(h * 0.56)), label)
    return out
