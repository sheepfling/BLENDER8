"""Validated build inputs. All coordinates are fractions of the upright page."""

from __future__ import annotations

import tomllib
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator


class StrictModel(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True, allow_inf_nan=False)


class Region(StrictModel):
    x0: float = Field(ge=0, le=1)
    y0: float = Field(ge=0, le=1)
    x1: float = Field(ge=0, le=1)
    y1: float = Field(ge=0, le=1)

    @model_validator(mode="after")
    def ordered(self) -> Region:
        if self.x1 <= self.x0 or self.y1 <= self.y0:
            raise ValueError("Protected rectangles must have positive width and height.")
        return self


class Document(StrictModel):
    id: str
    title: str
    source: str
    output_stem: str
    vendor: str
    default_preset: str = "lab_copy"
    appearance: dict[str, object] = Field(default_factory=dict)
    artifact: bool = True
    expected_pages: int = Field(ge=1)


class Catalog(StrictModel):
    version: str
    documents: tuple[Document, ...]


class MuxSpec(StrictModel):
    part: Literal["MX8-1"]
    document: Literal["MUX-001"]
    behavior_revision: Literal["0.2.0"]
    presentation_revision: str
    date: str
    input_count: Literal[8]
    select_count: Literal[3]
    output_count: Literal[1]
    settling_us: Literal[2]
    source_file: str
    source_sha256: str


def load_catalog(root: Path) -> Catalog:
    with (root / "internal/authoring/config/catalog.toml").open("rb") as stream:
        return Catalog.model_validate(tomllib.load(stream))


def load_mux(root: Path) -> MuxSpec:
    with (root / "internal/authoring/spec/mx8-1.toml").open("rb") as stream:
        return MuxSpec.model_validate(tomllib.load(stream))


# Kept as import aliases for existing caller code.
from .recipe import Recipe, load_recipe  # noqa: E402

__all__ = ["Recipe", "load_recipe"]
