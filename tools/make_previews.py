"""Make a contact sheet from actual supplier/correspondence PDF pages, not mockups."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, cast

import pypdfium2 as pdfium
from PIL import Image, ImageDraw, ImageFont

SHEETS: tuple[tuple[str, str, int], ...] = (
    ("Northstar / B8 reference", "NMD-B8-Controller", 0),
    ("TriAxis / logic catalog", "MX8-1", 0),
    ("PixelRiver / display module", "PRD-PX32-16", 0),
    ("Vortek / motor workbench copy", "VM-MD20", 0),
    ("ThermaSense / analog sensor", "TSC-AVT10", 0),
    ("Kestrel / assembly contacts", "KAS-BA-8", 1),
    ("Meridion / clock oscillator", "MFP-XO8-33", 0),
    ("Half-A/Labs / correspondence", "HAL-Customer-Correspondence", 6),
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    root = parser.parse_args().root.resolve()
    sheet = Image.new("RGB", (1920, 1390), (220, 218, 212))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=20)
    for index, (label, stem, page_number) in enumerate(SHEETS):
        with pdfium.PdfDocument(root / f"dist/field/{stem}.field.pdf") as document:
            page = cast(Any, document[page_number])
            bitmap = page.render(scale=1.5)
            try:
                image: Image.Image = bitmap.to_pil().convert("RGB").copy()
            finally:
                bitmap.close()
                page.close()
        image.thumbnail((444, 628), Image.Resampling.LANCZOS)
        x = (index % 4) * 480 + (480 - image.width) // 2
        y = (index // 4) * 695 + 43
        draw.text((x, y - 29), label, font=font, fill=(40, 40, 40))
        sheet.paste(image, (x, y))
    out = root / "build/docs/Supplier-Manuals-Preview.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()
