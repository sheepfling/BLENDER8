"""Shared pixel and random-stream conventions."""

from __future__ import annotations

import hashlib
import json
from typing import Any

import numpy as np
from numpy.typing import NDArray
from PIL import Image

from ..recipe import Span

FloatImage = NDArray[np.float32]
Matrix = NDArray[np.float64]


def stream_seed(seed: int, *parts: object) -> int:
    if seed < 0:
        raise ValueError("Seed must be nonnegative.")
    # JSON framing prevents collisions from separators in user-supplied identifiers.
    key = json.dumps(["b8docs-effects-v2", seed, *parts], separators=(",", ":")).encode()
    return int.from_bytes(hashlib.sha256(key).digest()[:8], "big")


def rng_for(seed: int) -> np.random.Generator:
    return np.random.Generator(np.random.PCG64(seed))


def sample(span: Span, rng: np.random.Generator) -> float:
    return float(rng.uniform(span.min, span.max))


def pixels(image: Image.Image) -> FloatImage:
    return np.asarray(image.convert("RGB"), dtype=np.float32) / np.float32(255)


def to_image(array: NDArray[np.floating[Any]]) -> Image.Image:
    return Image.fromarray(np.clip(np.rint(array * 255), 0, 255).astype(np.uint8))


def image_hash(image: Image.Image) -> str:
    header = f"{image.mode}:{image.width}:{image.height}:".encode()
    return hashlib.sha256(header + image.tobytes()).hexdigest()
