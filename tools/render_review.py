"""Render actual PDF pages and labeled review contact sheets. No OCR or substitute artwork."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, cast

import pypdfium2 as pdfium
from PIL import Image, ImageDraw, ImageFont


def render(path: Path, output: Path, dpi: int = 110) -> None:
    output.mkdir(parents=True, exist_ok=True)
    thumbs: list[Image.Image] = []
    names: list[str] = []
    with pdfium.PdfDocument(path) as document:
        for index in range(len(document)):
            page = cast(Any, document[index])
            bitmap = page.render(scale=dpi / 72)
            try:
                image: Image.Image = bitmap.to_pil().convert("RGB").copy()
            finally:
                bitmap.close()
                page.close()
            image.save(output / f"page-{index + 1:02d}.png")
            image.thumbnail((440, 595), Image.Resampling.LANCZOS)
            thumbs.append(image)
            names.append(f"{path.stem} / {index + 1}")
    font = ImageFont.load_default(size=17)
    for start in range(0, len(thumbs), 8):
        selected = thumbs[start : start + 8]
        canvas = Image.new("RGB", (1920, 1340), (215, 213, 207))
        draw = ImageDraw.Draw(canvas)
        for offset, image in enumerate(selected):
            x = (offset % 4) * 480 + (480 - image.width) // 2
            y = (offset // 4) * 670 + 42
            canvas.paste(image, (x, y))
            draw.text((x, y - 28), names[start + offset], font=font, fill=(35, 35, 35))
        canvas.save(output / f"contact-{start // 8 + 1:02d}.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dpi", type=int, default=110)
    args = parser.parse_args()
    if not 72 <= args.dpi <= 300:
        raise SystemExit("Review DPI must be between 72 and 300.")
    render(args.pdf, args.output, args.dpi)


if __name__ == "__main__":
    main()
