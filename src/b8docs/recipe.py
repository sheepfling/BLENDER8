"""Versioned appearance contract. Distances are physical mm, not arbitrary pixels."""

from __future__ import annotations

import copy
import hashlib
import json
import tomllib
from pathlib import Path
from typing import Annotated, Any, Literal, Self, cast

from pydantic import BaseModel, ConfigDict, Field, model_validator


class Strict(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True, allow_inf_nan=False)


class Span(Strict):
    min: float
    max: float

    @model_validator(mode="after")
    def ordered(self) -> Self:
        if self.min > self.max:
            raise ValueError("Range min must be <= max.")
        return self

    def bounded(self, low: float, high: float) -> None:
        if self.min < low or self.max > high:
            raise ValueError(f"Range must be inside [{low}, {high}].")


class CountSpan(Strict):
    min: int = Field(ge=0, le=30, strict=True)
    max: int = Field(ge=0, le=30, strict=True)

    @model_validator(mode="after")
    def ordered(self) -> Self:
        if self.min > self.max:
            raise ValueError("Count min must be <= max.")
        return self


class Box(Strict):
    """Normalized, top-left-origin source-page rectangle; applied before any transform."""

    x0: float = Field(ge=0, le=1)
    y0: float = Field(ge=0, le=1)
    x1: float = Field(ge=0, le=1)
    y1: float = Field(ge=0, le=1)

    @model_validator(mode="after")
    def ordered(self) -> Self:
        if self.x0 >= self.x1 or self.y0 >= self.y1:
            raise ValueError("Protected rectangle must have positive area.")
        return self


class Placement(Strict):
    enabled: bool = True
    rotation_deg: Span = Span(min=-0.8, max=0.8)
    translate_x_mm: Span = Span(min=-1.0, max=1.0)
    translate_y_mm: Span = Span(min=-1.2, max=1.2)
    shear_deg: Span = Span(min=-0.08, max=0.08)
    scale: Span = Span(min=0.995, max=1.0)
    inset_mm: float = Field(default=1.0, ge=0, le=12)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.rotation_deg.bounded(-5, 5)
        self.shear_deg.bounded(-1, 1)
        self.translate_x_mm.bounded(-10, 10)
        self.translate_y_mm.bounded(-10, 10)
        self.scale.bounded(0.9, 1.05)
        return self


class Vignette(Strict):
    enabled: bool = True
    strength: Span = Span(min=0.06, max=0.12)
    width_mm: Span = Span(min=4.0, max=10.0)
    edge: Literal["left", "right", "top", "bottom", "random"] = "left"
    asymmetry: float = Field(default=0.7, ge=0, le=1)
    corner_strength: float = Field(default=0.07, ge=0, le=0.35)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.strength.bounded(0, 0.5)
        self.width_mm.bounded(0.1, 40)
        return self


class Streaks(Strict):
    enabled: bool = True
    horizontal: CountSpan = CountSpan(min=1, max=3)
    vertical: CountSpan = CountSpan(min=1, max=2)
    opacity: Span = Span(min=0.035, max=0.10)
    width_mm: Span = Span(min=0.08, max=0.40)
    light_fraction: float = Field(default=0.3, ge=0, le=1)
    waviness_mm: float = Field(default=0.15, ge=0, le=2)
    roller_pitch_mm: float = Field(default=24, ge=2, le=100)
    roller_opacity: float = Field(default=0.012, ge=0, le=0.08)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.opacity.bounded(0, 0.35)
        self.width_mm.bounded(0.02, 3)
        return self


class Repro(Strict):
    enabled: bool = True
    blur_mm: Span = Span(min=0.025, max=0.050)
    resample: Span = Span(min=0.92, max=0.98)
    contrast: Span = Span(min=1.01, max=1.06)
    threshold_mix: float = Field(default=0.10, ge=0, le=0.5)
    threshold: int = Field(default=176, ge=80, le=230)
    toner_bloom: float = Field(default=0.08, ge=0, le=0.4)
    grain_sigma: float = Field(default=0.5, ge=0, le=3)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.blur_mm.bounded(0, 0.12)
        self.resample.bounded(0.6, 1.0)
        self.contrast.bounded(0.9, 1.4)
        return self


Byte = Annotated[int, Field(ge=0, le=255, strict=True)]


class Paper(Strict):
    monochrome: bool = True
    rgb: tuple[Byte, Byte, Byte] = (249, 249, 249)
    bed_rgb: tuple[Byte, Byte, Byte] = (238, 238, 236)


class Coffee(Strict):
    """Paper-space cup mark dimensions; coordinates are normalized to the sheet."""

    radius_mm: Span = Span(min=19.0, max=26.0)
    center_x: Span = Span(min=0.94, max=1.01)
    center_y: Span = Span(min=0.74, max=0.88)
    rim_width_mm: float = Field(default=0.65, ge=0.1, le=3)
    irregularity: float = Field(default=0.07, ge=0, le=0.2)
    broken_rim: float = Field(default=0.35, ge=0, le=0.85)
    droplets: CountSpan = CountSpan(min=2, max=5)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.radius_mm.bounded(4, 50)
        self.center_x.bounded(-0.15, 1.15)
        self.center_y.bounded(-0.15, 1.15)
        return self


class Tears(Strict):
    """Missing edge material, rejected whenever it touches protected content."""

    enabled: bool = False
    stage: Literal["before_copy", "after_copy"] = "before_copy"
    count: CountSpan = CountSpan(min=1, max=2)
    edges: tuple[Literal["left", "right", "top", "bottom"], ...] = ("right", "bottom")
    depth_mm: Span = Span(min=1.5, max=4.0)
    length_mm: Span = Span(min=5.0, max=13.0)
    clearance_mm: float = Field(default=0.8, ge=0, le=5)
    roughness: float = Field(default=0.23, ge=0, le=0.5)
    fiber_mm: float = Field(default=0.20, ge=0.05, le=0.8)
    attempts_per_tear: int = Field(default=32, ge=1, le=128, strict=True)

    @model_validator(mode="after")
    def bounds(self) -> Self:
        self.depth_mm.bounded(0.2, 12)
        self.length_mm.bounded(1, 40)
        if not self.edges or len(set(self.edges)) != len(self.edges):
            raise ValueError("Choose one or more unique tear edges.")
        return self


class PdfOutput(Strict):
    encoding: Literal["lossless", "jpeg"] = "lossless"
    jpeg_quality: int = Field(default=95, ge=85, le=100, strict=True)


class Handling(Strict):
    enabled: bool = True
    stage: Literal["before_copy", "after_copy"] = "before_copy"
    coffee: bool = False
    coffee_profile: Coffee = Coffee()
    preserve_color_after_copy: bool = False
    fold: bool = False
    staple: bool = False
    punch_holes: bool = False
    dust_count: int = Field(default=45, ge=0, le=1000, strict=True)
    stamp: str = Field(default="", max_length=40, pattern=r"^[ -~]*$")
    opacity: float = Field(default=0.30, ge=0, le=0.6)


class Protection(Strict):
    enabled: bool = True
    regions: tuple[Box, ...] = (Box(x0=0.055, y0=0.035, x1=0.965, y1=0.975),)
    # Final normalized-channel difference versus a geometry-only reference, not per-effect.
    max_delta: float = Field(default=0.10, ge=0, le=0.25)
    min_cumulative_scale: float = Field(default=0.78, ge=0.5, le=1)


class Settings(Strict):
    dpi: int = Field(default=240, ge=150, le=450, strict=True)
    generation_depth: int = Field(default=2, ge=0, le=4, strict=True)
    copier_id: str = Field(default="copier-04", min_length=1, max_length=80)
    placement: Placement = Placement()
    vignette: Vignette = Vignette()
    streaks: Streaks = Streaks()
    repro: Repro = Repro()
    paper: Paper = Paper()
    handling: Handling = Handling()
    protection: Protection = Protection()
    tears: Tears = Tears()
    output: PdfOutput = PdfOutput()


class PageOverride(Strict):
    pages: tuple[Annotated[int, Field(ge=1, strict=True)], ...] = Field(min_length=1)
    patch: dict[str, Any]


class Recipe(Settings):
    schema_version: Literal[2] = 2
    name: str = Field(min_length=1, max_length=80)
    page_overrides: tuple[PageOverride, ...] = ()

    @model_validator(mode="after")
    def validate_page_overrides(self) -> Self:
        base = self.base_settings().model_dump()
        used: set[int] = set()
        for item in self.page_overrides:
            if any(n in used for n in item.pages) or len(set(item.pages)) != len(item.pages):
                raise ValueError("A page may appear in only one page override.")
            if "dpi" in item.patch or "output" in item.patch:
                raise ValueError(
                    "DPI and PDF output encoding are document-wide; "
                    "they cannot be changed per page."
                )
            Settings.model_validate(deep_merge(base, item.patch))
            used.update(item.pages)
        return self

    def base_settings(self) -> Settings:
        return Settings.model_validate(
            self.model_dump(exclude={"name", "schema_version", "page_overrides"})
        )

    def for_page(self, number: int) -> Settings:
        if number < 1:
            raise ValueError("Page numbers are 1-based.")
        data = self.base_settings().model_dump()
        for item in self.page_overrides:
            if number in item.pages:
                data = deep_merge(data, item.patch)
        return Settings.model_validate(data)


def deep_merge(base: dict[str, Any], patch: dict[str, Any]) -> dict[str, Any]:
    """Recurse into mappings; replace arrays as a whole. Never mutate caller data."""
    result = copy.deepcopy(base)
    for key, value in patch.items():
        if isinstance(result.get(key), dict) and isinstance(value, dict):
            result[key] = deep_merge(cast(dict[str, Any], result[key]), cast(dict[str, Any], value))
        else:
            result[key] = copy.deepcopy(value)
    return result


def apply_set(data: dict[str, Any], expression: str) -> dict[str, Any]:
    """Dotted key = TOML literal. Typos are rejected by the final model validation."""
    if "=" not in expression:
        raise ValueError("--set expects dotted.key=TOML-value")
    key, raw = expression.split("=", 1)
    path = key.strip().split(".")
    if not all(p.isidentifier() for p in path):
        raise ValueError("Invalid --set key path.")
    value = tomllib.loads("value = " + raw)["value"]
    patch: dict[str, Any] = {}
    cursor = patch
    for part in path[:-1]:
        cursor[part] = {}
        cursor = cursor[part]
    cursor[path[-1]] = value
    return deep_merge(data, patch)


def load_recipe(
    root: Path,
    name: str,
    recipe_file: Path | None = None,
    document_overrides: dict[str, Any] | None = None,
    overrides: tuple[str, ...] = (),
    depth: int | None = None,
    dpi: int | None = None,
) -> Recipe:
    """Defaults < preset < document patch < custom file < CLI sets < depth/DPI."""
    custom: dict[str, Any] = {}
    if recipe_file is not None:
        with recipe_file.open("rb") as stream:
            custom = tomllib.load(stream)
        name = str(custom.pop("extends", name))
    if not name.replace("_", "").isalnum():
        raise ValueError("Preset name must be a plain identifier, not a path.")
    path = root / "internal/authoring/config/presets" / f"{name}.toml"
    if not path.is_file():
        raise ValueError(f"Unknown recipe: {name!r}")
    with path.open("rb") as stream:
        data = tomllib.load(stream)
    data = Recipe.model_validate(data).model_dump()
    data = deep_merge(data, document_overrides or {})
    data = deep_merge(data, custom)
    for expression in overrides:
        data = apply_set(data, expression)
    if depth is not None:
        data["generation_depth"] = depth
        for rule in data.get("page_overrides", []):
            rule["patch"].pop("generation_depth", None)
    if dpi is not None:
        data["dpi"] = dpi
    return Recipe.model_validate(data)


def recipe_hash(recipe: Recipe | Settings) -> str:
    return hashlib.sha256(
        json.dumps(recipe.model_dump(mode="json"), sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
