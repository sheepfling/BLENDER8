"""Document Foundry CLI. Plan, render, sweep and replay the same validated recipe."""

from __future__ import annotations

import json
import shutil
from pathlib import Path
from typing import Annotated, Any

import typer

from .artifact import PIPELINE_ORDER
from .manuals import generation_source_path
from .models import Document, load_catalog
from .pipeline import (
    compile_document,
    corrupt_pdf,
    generate_values,
    merge_pdfs,
    replay_pdf,
    verify_generation_sequence,
    verify_pair,
    versions,
)
from .recipe import Recipe, load_recipe, recipe_hash

app = typer.Typer(no_args_is_help=True, help="Canonical LaTeX -> clean PDF -> reproducible copies.")
Root = Annotated[
    Path, typer.Option("--root", help="Checkout containing internal/authoring/ and latex/.")
]
Sets = Annotated[
    list[str] | None, typer.Option("--set", help="Repeatable dotted.key=TOML-value override.")
]
Json = Annotated[bool, typer.Option("--json", help="Machine-readable result.")]


def emit(value: dict[str, Any], as_json: bool) -> None:
    typer.echo(
        json.dumps(value, indent=2, sort_keys=True)
        if as_json
        else "\n".join(f"{key}: {item}" for key, item in value.items())
    )


def fail(exc: Exception, as_json: bool = False) -> None:
    typer.echo(
        json.dumps({"ok": False, "error": str(exc)}) if as_json else f"Error: {exc}", err=True
    )
    raise typer.Exit(1) from exc


def selected(root: Path, doc: str) -> Document:
    entries = [item for item in load_catalog(root).documents if item.id == doc]
    if len(entries) != 1:
        raise ValueError(
            f"Unknown document {doc!r}; choose an id from internal/authoring/config/catalog.toml."
        )
    return entries[0]


def resolved(
    root: Path,
    preset: str,
    recipe: Path | None,
    sets: list[str] | None,
    depth: int | None,
    dpi: int | None,
    document: Document | None = None,
) -> Recipe:
    return load_recipe(
        root,
        preset,
        recipe,
        document.appearance if document else None,
        tuple(sets or ()),
        depth,
        dpi,
    )


@app.command()
def doctor(root: Root = Path("."), as_json: Json = False) -> None:
    """Separate PDF-renderer readiness from the optional LaTeX authoring toolchain."""
    result = {
        "versions": versions(),
        "render_ready": (root / "internal/authoring/config/presets").is_dir(),
        "latexmk": shutil.which("latexmk"),
        "pdflatex": shutil.which("pdflatex"),
        "tectonic": shutil.which("tectonic"),
        "ruff": shutil.which("ruff"),
        "pyright": shutil.which("pyright"),
    }
    result["latex_ready"] = bool((result["latexmk"] and result["pdflatex"]) or result["tectonic"])
    emit(result, as_json)
    if not result["render_ready"]:
        raise typer.Exit(1)


@app.command()
def plan(
    root: Root = Path("."),
    preset: str | None = None,
    recipe: Path | None = None,
    doc: str | None = None,
    depth: int | None = None,
    dpi: int | None = None,
    sets: Sets = None,
    as_json: Json = False,
) -> None:
    """Validate and show effective settings without rendering anything."""
    try:
        entry = selected(root, doc) if doc else None
        config = resolved(
            root,
            preset or (entry.default_preset if entry else "shop_copy"),
            recipe,
            sets,
            depth,
            dpi,
            entry,
        )
        emit(
            {
                "ok": True,
                "recipe_sha256": recipe_hash(config),
                "effect_order": list(PIPELINE_ORDER),
                "recipe": config.model_dump(mode="json"),
            },
            as_json,
        )
    except (OSError, ValueError) as exc:
        fail(exc, as_json)


@app.command()
def schema(output: Path | None = None) -> None:
    """Emit the JSON Schema used for config validation and editor completion."""
    text = json.dumps(Recipe.model_json_schema(), indent=2) + "\n"
    if output is None:
        typer.echo(text)
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        typer.echo(str(output))


@app.command()
def generate(root: Root = Path(".")) -> None:
    """Validate the canonical mux contract and generate numerical TeX fragments."""
    try:
        typer.echo(str(generate_values(root.resolve())))
    except (OSError, ValueError) as exc:
        fail(exc)


@app.command()
def build(
    root: Root = Path("."),
    doc: str = "mux",
    preset: str | None = None,
    recipe: Path | None = None,
    seed: int = 4815,
    dpi: int | None = None,
    depth: int | None = None,
    sets: Sets = None,
    clean_only: bool = False,
    as_json: Json = False,
) -> None:
    """Compile a catalog document and save G1/G2 for review and G3 for delivery."""
    try:
        root = root.resolve()
        entry = selected(root, doc)
        recipe_file: Path | None = (
            recipe or root / "internal/authoring/config/manuals" / f"{entry.id}.toml"
        )
        if recipe is None and not recipe_file.is_file():
            recipe_file = None
        config = resolved(
            root, preset or entry.default_preset, recipe_file, sets, depth, dpi, entry
        )
        clean = compile_document(root, entry)
        result: dict[str, Any] = {"clean": str(clean)}
        if not clean_only and entry.artifact:
            if depth is None:
                copies: list[Path] = []
                for generation in (1, 2, 3):
                    stage_config = resolved(
                        root,
                        preset or entry.default_preset,
                        recipe_file,
                        sets,
                        generation,
                        dpi,
                        entry,
                    )
                    output = generation_source_path(root, entry.output_stem, generation)
                    corrupt_pdf(clean, output, stage_config, seed, document_key=entry.id)
                    copies.append(output)
                result.update(
                    generations={
                        f"G{generation}": str(output)
                        for generation, output in enumerate(copies, start=1)
                    },
                    field=str(copies[2]),
                    verification=verify_generation_sequence(clean, copies),
                )
            else:
                output = root / "build/docs/variants" / f"{entry.output_stem}.g{depth}.pdf"
                corrupt_pdf(clean, output, config, seed, document_key=entry.id)
                result.update(
                    variant=str(output), manifest=str(output.with_suffix(".manifest.json"))
                )
        emit(result, as_json)
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc, as_json)


@app.command()
def corrupt(
    source: Path,
    output: Path,
    root: Root = Path("."),
    preset: str = "shop_copy",
    recipe: Path | None = None,
    seed: int = 4815,
    dpi: int | None = None,
    key: str | None = None,
    depth: int | None = None,
    sets: Sets = None,
    as_json: Json = False,
) -> None:
    """Render an existing PDF; no TeX invocation or Blender-8 component dependency."""
    try:
        config = resolved(root, preset, recipe, sets, depth, dpi)
        corrupt_pdf(source, output, config, seed, document_key=key)
        emit(
            {
                "output": str(output),
                "manifest": str(output.with_suffix(".manifest.json")),
                "recipe_sha256": recipe_hash(config),
            },
            as_json,
        )
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc, as_json)


@app.command()
def sweep(
    source: Path,
    output_dir: Path,
    root: Root = Path("."),
    preset: str = "bad_xerox",
    recipe: Path | None = None,
    depths: str = "0,1,2,3",
    seed: int = 4815,
    dpi: int | None = None,
    key: str | None = None,
    sets: Sets = None,
    as_json: Json = False,
) -> None:
    """Render a controlled generation-depth comparison, retaining the same random streams."""
    try:
        values = [int(item.strip()) for item in depths.split(",")]
        if not values or len(set(values)) != len(values):
            raise ValueError("Depths must be a nonempty list without duplicates.")
        configs = [resolved(root, preset, recipe, sets, value, dpi) for value in values]
        outputs: list[str] = []
        for value, config in zip(values, configs, strict=True):
            # Sweep is explicitly global: page-depth overrides would confound the comparison.
            if any("generation_depth" in rule.patch for rule in config.page_overrides):
                raise ValueError("Remove per-page depth overrides before a global depth sweep.")
            output = output_dir / f"{source.stem}.g{value}.pdf"
            corrupt_pdf(source, output, config, seed, document_key=key)
            outputs.append(str(output))
        emit({"outputs": outputs, "depths": values, "seed": seed}, as_json)
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc, as_json)


@app.command()
def replay(
    source: Path,
    manifest: Path,
    output: Path,
    allow_toolchain_change: bool = False,
    as_json: Json = False,
) -> None:
    """Rebuild a recorded recipe and compare against its recorded output hash."""
    try:
        result = replay_pdf(source, manifest, output, allow_toolchain_change)
        emit(result, as_json)
        if not result["passed"]:
            raise typer.Exit(2)
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc, as_json)


@app.command()
def proofs(root: Root = Path(".")) -> None:
    """Compile the six existing vendor identity proofs."""
    try:
        root = root.resolve()
        inputs = [
            compile_document(root, doc)
            for doc in load_catalog(root).documents
            if doc.id.startswith("proof_")
        ]
        output = root / "dist/Vendor-Identity-Proofs.pdf"
        merge_pdfs(inputs, output)
        typer.echo(str(output))
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc)


@app.command()
def verify(clean: Path, field: Path, manifest: Path | None = None, as_json: Json = False) -> None:
    """Verify hashes, displayed page geometry, page mapping and flattened output."""
    try:
        emit(verify_pair(clean, field, manifest), as_json)
    except (OSError, ValueError, RuntimeError) as exc:
        fail(exc, as_json)


@app.command()
def packet(
    root: Root = Path("."),
    recipe_dir: Path | None = None,
    seed: int = 4815,
    clean_only: bool = False,
    as_json: Json = False,
) -> None:
    """Build seven handouts: correspondence, B8, mux, LCD, motor/sensor, buttons, oscillator."""
    from .manuals import build_packet

    try:
        result = build_packet(root.resolve(), recipe_dir, seed, clean_only)
        emit(result, as_json)
    except (OSError, ValueError, RuntimeError, KeyError) as exc:
        fail(exc, as_json)


@app.command()
def audit(root: Root = Path("."), as_json: Json = False) -> None:
    """Check declared ownership boundaries before compiling or degrading the papers."""
    from .boundaries import audit_boundaries

    try:
        result = audit_boundaries(root.resolve())
        emit(result, as_json)
        if not result["passed"]:
            raise typer.Exit(2)
    except (OSError, ValueError, KeyError) as exc:
        fail(exc, as_json)
