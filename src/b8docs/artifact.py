"""Copy-generation orchestrator. Previous-copy pixels are the next-copy input."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

import numpy as np
from PIL import Image, ImageFilter

from .effects.affine import plan_placement, warp
from .effects.common import FloatImage, image_hash, pixels, rng_for, stream_seed, to_image
from .effects.handling import apply_handling, apply_paper
from .effects.optical import apply_repro, apply_streaks, apply_vignette
from .effects.tears import apply_tears
from .recipe import Recipe, Settings

PIPELINE_ORDER = (
    "paper",
    "handling_before",
    "tears_before",
    "repeat:placement",
    "repeat:reproduction",
    "repeat:vignette",
    "repeat:streaks",
    "repeat:protection",
    "handling_after",
    "tears_after",
    "final_protection",
    "encode",
    "flatten",
)


@dataclass(frozen=True)
class PageWear:
    page_seed: int
    rotation_deg: float
    final_width: int
    final_height: int
    image_sha256: str
    generation_depth: int
    cumulative_source_to_output: list[list[float]]
    passes: list[dict[str, Any]]
    protection: dict[str, Any]
    physical_damage: dict[str, Any] = field(default_factory=dict[str, Any])


def derive_seed(seed: int, document_key: str, page_index: int) -> int:
    if page_index < 0:
        raise ValueError("Page index must be nonnegative.")
    return stream_seed(seed, document_key, page_index)


def protected_mask(size: tuple[int, int], recipe: Settings) -> FloatImage:
    w, h = size
    mask = np.zeros((h, w), dtype=np.float32)
    if recipe.protection.enabled:
        for box in recipe.protection.regions:
            mask[
                int(box.y0 * h) : int(np.ceil(box.y1 * h)),
                int(box.x0 * w) : int(np.ceil(box.x1 * w)),
            ] = 1.0
    return mask


def protect(
    image: Image.Image, baseline: Image.Image, mask: Image.Image, recipe: Settings
) -> tuple[Image.Image, float]:
    arr, reference = pixels(image), pixels(baseline)
    selected = np.asarray(mask) > 0
    if recipe.protection.enabled and selected.any():
        # Aggregate channel cap relative to the geometric reference, not a per-layer allowance.
        cap = np.floor(recipe.protection.max_delta * 255) / 255
        arr[selected] = np.clip(arr[selected], reference[selected] - cap, reference[selected] + cap)
    result = to_image(arr)
    diff = np.abs(pixels(result) - reference)
    observed = float(diff[selected].max()) if selected.any() else 0.0
    return result, observed


def make_artifact(
    image: Image.Image,
    recipe: Recipe | Settings,
    seed: int,
    document_key: str,
    page_index: int,
    dpi: int,
) -> tuple[Image.Image, PageWear]:
    if not 150 <= dpi <= 450:
        raise ValueError("DPI must be between 150 and 450.")
    cfg = recipe.for_page(page_index + 1) if isinstance(recipe, Recipe) else recipe
    page_seed = derive_seed(seed, document_key, page_index)
    source = image.convert("RGB")
    w, h = source.size
    total_matrix = np.eye(3)
    records: list[dict[str, Any]] = []
    if cfg.generation_depth == 0:
        out = image.copy()
        return out, PageWear(
            page_seed, 0, w, h, image_hash(out), 0, total_matrix.tolist(), [], {"mode": "pristine"}
        )
    baseline = source.copy()
    mask = Image.fromarray((protected_mask((w, h), cfg) * 255).astype(np.uint8))
    # Dilate in source coordinates to cover small interpolation support at ROI boundaries.
    mask = mask.filter(ImageFilter.MaxFilter(3))
    out = apply_paper(source, cfg.paper)
    physical_damage: dict[str, Any] = {}
    if cfg.handling.stage == "before_copy":
        out = apply_handling(
            out,
            cfg.handling,
            cfg.paper.monochrome,
            dpi,
            rng_for(stream_seed(page_seed, "handling")),
            physical_damage,
        )
    if cfg.tears.stage == "before_copy":
        out, tear_record = apply_tears(
            out, cfg.tears, dpi, rng_for(stream_seed(page_seed, "tears")), mask, cfg.paper.bed_rgb
        )
        physical_damage["tears"] = tear_record
    max_delta = 0.0
    for generation in range(1, cfg.generation_depth + 1):
        input_hash = image_hash(out)
        seeds = {
            effect: stream_seed(page_seed, generation, effect)
            for effect in ("placement", "repro", "vignette")
        }
        # Copier defects are shared across a job, independent of document/page content.
        seeds["streaks"] = stream_seed(seed, cfg.copier_id, generation, "streaks")
        matrix, affine_record = plan_placement(
            (w, h), dpi, cfg.placement, rng_for(seeds["placement"])
        )
        total_matrix = matrix @ total_matrix
        cumulative_scale = float(np.sqrt(np.linalg.det(total_matrix[:2, :2])))
        if cumulative_scale < cfg.protection.min_cumulative_scale:
            raise ValueError(
                f"Cumulative scale {cumulative_scale:.3f} below recipe readability floor."
            )
        # Old copier artifacts rotate with their sheet on the NEXT pass.
        out = warp(out, matrix, cfg.paper.bed_rgb)
        baseline = warp(baseline, matrix, cfg.paper.bed_rgb)
        mask = warp(mask, matrix, 0, mask=True)
        out, repro_record = apply_repro(out, cfg.repro, dpi, rng_for(seeds["repro"]))
        out, vignette_record = apply_vignette(out, cfg.vignette, dpi, rng_for(seeds["vignette"]))
        out, streak_record = apply_streaks(out, cfg.streaks, dpi, rng_for(seeds["streaks"]))
        out, max_delta = protect(out, baseline, mask, cfg)
        records.append(
            {
                "generation": generation,
                "input_image_sha256": input_hash,
                "output_image_sha256": image_hash(out),
                "seeds": seeds,
                "placement": affine_record,
                "repro": repro_record,
                "vignette": vignette_record,
                "streaks": streak_record,
                "cumulative_scale": cumulative_scale,
                "protected_max_delta": max_delta,
            }
        )
    color_handling = cfg.handling.stage == "after_copy" and cfg.handling.preserve_color_after_copy
    if cfg.paper.monochrome and color_handling:
        out = out.convert("L").convert("RGB")
    if cfg.handling.stage == "after_copy":
        out = apply_handling(
            out,
            cfg.handling,
            cfg.paper.monochrome and not color_handling,
            dpi,
            rng_for(stream_seed(page_seed, "handling")),
            physical_damage,
        )
    if cfg.tears.stage == "after_copy":
        out, tear_record = apply_tears(
            out, cfg.tears, dpi, rng_for(stream_seed(page_seed, "tears")), mask, cfg.paper.bed_rgb
        )
        physical_damage["tears"] = tear_record
    if cfg.paper.monochrome and not color_handling:
        out = out.convert("L").convert("RGB")
    out, max_delta = protect(out, baseline, mask, cfg)
    if (
        cfg.paper.monochrome
        and np.array_equal(np.asarray(out)[:, :, 0], np.asarray(out)[:, :, 1])
        and np.array_equal(np.asarray(out)[:, :, 1], np.asarray(out)[:, :, 2])
    ):
        out = out.convert("L")
    angle = float(np.degrees(np.arctan2(total_matrix[1, 0], total_matrix[0, 0])))
    return out, PageWear(
        page_seed,
        angle,
        w,
        h,
        image_hash(out),
        cfg.generation_depth,
        total_matrix.tolist(),
        records,
        {
            "enabled": cfg.protection.enabled,
            "max_delta_allowed": cfg.protection.max_delta,
            "max_delta_observed": max_delta,
            "protected_pixels": int((np.asarray(mask) > 0).sum()),
            "scope": (
                "Pre-encoding cosmetic difference from geometry-only reference; "
                "not an OCR or legibility proof."
            ),
        },
        physical_damage,
    )
