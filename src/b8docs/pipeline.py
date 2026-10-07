"""LaTeX compilation, image-only PDF assembly, and traceable QA manifests."""

from __future__ import annotations

import hashlib
import importlib.metadata
import json
import os
import platform
import re
import shutil
import subprocess
import zlib
from collections.abc import Iterable, Sequence
from dataclasses import asdict
from importlib.resources import files
from importlib.resources.abc import Traversable
from io import BytesIO
from itertools import pairwise
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any, cast

import pypdfium2 as pdfium
from PIL import Image
from pypdf import PdfReader, PdfWriter
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen.canvas import Canvas

from . import __version__
from .artifact import PIPELINE_ORDER, make_artifact
from .models import Document, Recipe, load_mux
from .recipe import PdfOutput, recipe_hash


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def within(root: Path, relative: str) -> Path:
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError(f"Path escapes project root: {relative}")
    return path


def versions() -> dict[str, str]:
    pending: list[tuple[str, Traversable]] = [("", files("b8docs"))]
    sources: list[tuple[str, bytes]] = []
    while pending:
        prefix, directory = pending.pop()
        for entry in directory.iterdir():
            relative = prefix + entry.name
            if entry.is_dir():
                pending.append((relative + "/", entry))
            elif entry.name.endswith(".py"):
                sources.append((relative, entry.read_bytes()))
    source_fingerprint = hashlib.sha256()
    for relative, content in sorted(sources):
        source_fingerprint.update(relative.encode())
        source_fingerprint.update(b"\0")
        source_fingerprint.update(content)
    result = {
        "b8docs": __version__,
        "python": platform.python_version(),
        "platform": platform.system(),
        "machine": platform.machine(),
        "zlib": zlib.ZLIB_RUNTIME_VERSION,
        "engine_sha256": source_fingerprint.hexdigest(),
    }
    for package in ("numpy", "Pillow", "pydantic", "typer", "pypdfium2", "pypdf", "reportlab"):
        result[package] = importlib.metadata.version(package)
    return result


def generate_values(root: Path) -> Path:
    spec = load_mux(root)
    if digest(within(root, spec.source_file)) != spec.source_sha256:
        raise ValueError(
            "Canonical source snapshot changed: review it and update its hash deliberately."
        )
    folder = root / "generated"
    folder.mkdir(exist_ok=True)
    text = "% Generated from internal/authoring/spec/mx8-1.toml. Do not edit.\n"
    for key, value in {
        "PartNumber": spec.part,
        "BehaviorRevision": spec.behavior_revision,
        "PresentationRevision": spec.presentation_revision,
        "SpecDate": spec.date,
        "DocumentNumber": spec.document,
        "InputCount": spec.input_count,
        "SelectCount": spec.select_count,
        "SettlingUs": spec.settling_us,
    }.items():
        # Current model accepts only the fixed identifiers and simple revision/date tokens.
        if not re.fullmatch(r"[a-zA-Z0-9.\-]+", str(value)):
            raise ValueError(f"Unexpected TeX token in {key}")
        text += f"\\newcommand{{\\{key}}}{{{value}}}\n"
    target = folder / "mx8-1-values.tex"
    target.write_text(text, encoding="utf-8")
    rows = [
        f"{(i >> 2) & 1} & {(i >> 1) & 1} & {i & 1} & $X_{{{i}}}$ \\\\"
        for i in range(spec.input_count)
    ]
    (folder / "mx8-1-truth.tex").write_text(
        "\\newcommand{\\MuxTruthRows}{%\n" + "\n".join(rows) + "%\n}\n", encoding="utf-8"
    )
    return target


def compile_document(root: Path, document: Document) -> Path:
    generate_values(root)
    if document.id == "requirements":
        from .correspondence import generate_correspondence

        generate_correspondence(root)
    from .manuals import generate_controller_tables

    generate_controller_tables(root)
    latexmk_ready = bool(shutil.which("latexmk") and shutil.which("pdflatex"))
    tectonic_ready = bool(shutil.which("tectonic"))
    if not latexmk_ready and not tectonic_ready:
        raise RuntimeError(
            "Install Tectonic or a TeX distribution with latexmk and pdflatex, then run doctor."
        )
    source = within(root, document.source)
    work = root / "build" / document.id
    work.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.setdefault("SOURCE_DATE_EPOCH", "1791244800")
    env["FORCE_SOURCE_DATE"] = "1"
    # Source trees are trusted local author inputs; shell escape stays disabled.
    tex_roots = [root / "latex/common", root / "latex/vendors", root / "generated", root]
    env["TEXINPUTS"] = (
        os.pathsep.join(str(p) for p in tex_roots) + os.pathsep + env.get("TEXINPUTS", "")
    )
    if latexmk_ready:
        command = [
            "latexmk",
            "-pdf",
            "-interaction=nonstopmode",
            "-halt-on-error",
            "-file-line-error",
            "-pdflatex=pdflatex %O -no-shell-escape %S",
            f"-outdir={work}",
            str(source),
        ]
    else:
        command = [
            "tectonic",
            "--keep-logs",
            "--keep-intermediates",
            "-Z",
            f"search-path={root / 'latex/common'}",
            "-Z",
            f"search-path={root / 'latex/vendors'}",
            "-Z",
            f"search-path={root / 'generated'}",
            "-Z",
            f"search-path={root}",
            "--outdir",
            str(work),
            str(source),
        ]
    try:
        completed = subprocess.run(
            command, cwd=root, env=env, capture_output=True, text=True, timeout=180, check=False
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(f"LaTeX compilation timed out for {document.id}.") from exc
    (work / "build.stdout.txt").write_text(completed.stdout + completed.stderr, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"LaTeX failed for {document.id}. Read {work / 'build.stdout.txt'}")
    log = (work / (source.stem + ".log")).read_text(encoding="utf-8", errors="replace")
    problems = re.findall(r"Overfull \\[hv]box[^\n]*", log)
    if problems:
        raise RuntimeError(f"Layout overflow in {document.id}: " + "; ".join(problems))
    generated = work / (source.stem + ".pdf")
    reader = PdfReader(generated)
    if len(reader.pages) != document.expected_pages:
        raise RuntimeError(
            f"Expected {document.expected_pages} pages in {document.id}; got {len(reader.pages)}."
        )
    if any(len((page.extract_text() or "").strip()) < 100 for page in reader.pages):
        raise RuntimeError(f"Unexpected text-empty clean page in {document.id}.")
    output = root / "build/docs/clean" / f"{document.output_stem}.pdf"
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(generated, output)
    return output


def assemble_images(
    pages: Iterable[tuple[Image.Image, tuple[float, float]]],
    output: Path,
    title: str,
    author: str = "Technical Records",
    output_settings: PdfOutput | None = None,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    canvas = Canvas(str(output), pagesize=(612, 792), pageCompression=1, invariant=1)
    canvas.setTitle(title)
    canvas.setAuthor(author)
    canvas.setSubject("Technical reference / service-file reproduction")
    canvas.setCreator(f"Blender8 Document Foundry {__version__}")
    for image, (width, height) in pages:
        canvas.setPageSize((width, height))
        if output_settings is not None and output_settings.encoding == "jpeg":
            buffer = BytesIO()
            image.save(
                buffer,
                format="JPEG",
                quality=output_settings.jpeg_quality,
                subsampling=0,
                optimize=False,
                progressive=False,
            )
            buffer.seek(0)
            canvas.drawImage(ImageReader(buffer), 0, 0, width=width, height=height)  # pyright: ignore[reportUnknownMemberType]
        else:
            canvas.drawImage(ImageReader(image), 0, 0, width=width, height=height)  # pyright: ignore[reportUnknownMemberType]
        canvas.showPage()
    canvas.save()


def corrupt_pdf(
    source: Path,
    output: Path,
    recipe: Recipe,
    seed: int = 4815,
    dpi: int | None = None,
    document_key: str | None = None,
) -> Path:
    """Render one raster at a time; commit verified outputs from a temporary directory."""
    if seed < 0:
        raise ValueError("Seed must be nonnegative.")
    if dpi is not None:
        data = recipe.model_dump()
        data["dpi"] = dpi
        recipe = Recipe.model_validate(data)
    dpi = recipe.dpi
    if source.suffix.lower() != ".pdf" or output.suffix.lower() != ".pdf":
        raise ValueError("Source and output must have .pdf extensions.")
    if source.resolve() == output.resolve() or (
        output.exists() and os.path.samefile(source, output)
    ):
        raise ValueError("Refusing to overwrite the canonical clean PDF.")
    key = document_key or source.stem
    source_hash = digest(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, Any]] = []
    pdf = pdfium.PdfDocument(source)
    try:
        count = len(pdf)
        if count < 1 or count > 250:
            raise ValueError("Supported document size is 1..250 pages.")
        if any(p > count for rule in recipe.page_overrides for p in rule.pages):
            raise ValueError("A page override references a page beyond the source document.")
        # Entirely pristine jobs preserve the original PDF bytes. Mixed per-page
        # depths remain image-only, including the unweathered pages.
        pristine = all(recipe.for_page(i + 1).generation_depth == 0 for i in range(count))
        with TemporaryDirectory(prefix=".b8-foundry-", dir=output.parent) as folder:
            temporary = Path(folder) / "output.pdf"

            def pages() -> Iterable[tuple[Image.Image, tuple[float, float]]]:
                for index in range(count):
                    page = cast(Any, pdf[index])
                    try:
                        width, height = (float(v) for v in page.get_size())
                        if width * height * (dpi / 72) ** 2 > 25_000_000:
                            raise ValueError("Raster would exceed the 25-megapixel per-page limit.")
                        bitmap = page.render(scale=dpi / 72, draw_annots=True)
                        try:
                            image = bitmap.to_pil().copy()
                        finally:
                            bitmap.close()
                        dirty, wear = make_artifact(image, recipe, seed, key, index, dpi)
                        records.append(
                            {
                                "source_page": index + 1,
                                "output_page": index + 1,
                                "page_size_pt": [width, height],
                                "effective_settings": recipe.for_page(index + 1).model_dump(
                                    mode="json"
                                ),
                                **asdict(wear),
                            }
                        )
                        yield dirty, (width, height)
                    finally:
                        page.close()

            if pristine:
                shutil.copyfile(source, temporary)
                for index in range(count):
                    page = pdf[index]
                    try:
                        records.append(
                            {
                                "source_page": index + 1,
                                "output_page": index + 1,
                                "page_size_pt": list(page.get_size()),
                                "generation_depth": 0,
                                "mode": "clean_passthrough",
                            }
                        )
                    finally:
                        page.close()
            else:
                metadata = PdfReader(source).metadata
                title = str(metadata.title) if metadata and metadata.title else key
                author = (
                    str(metadata.author) if metadata and metadata.author else "Technical Records"
                )
                assemble_images(pages(), temporary, title, author, recipe.output)
            if digest(source) != source_hash:
                raise RuntimeError("Canonical input changed during the artifact build.")
            manifest = {
                "schema_version": 2,
                "pipeline_version": __version__,
                "document_key": key,
                "mode": "clean_passthrough" if pristine else "raster_copy",
                "provenance": (
                    "Synthetic reproduction of an in-universe source; "
                    "production record kept outside the manual."
                ),
                "pixel_hash_scope": "Before PDF image encoding; JPEG output is lossy.",
                "source_filename": source.name,
                "source_sha256": source_hash,
                "output_filename": output.name,
                "output_sha256": digest(temporary),
                "seed": seed,
                "dpi": dpi,
                "recipe": recipe.model_dump(mode="json"),
                "recipe_sha256": recipe_hash(recipe),
                "effect_order": list(PIPELINE_ORDER),
                "tool_versions": versions(),
                "pages": records,
                "text_layer": "preserved" if pristine else "none",
                "ocr_resistance": "Not measured or guaranteed",
                "reproducibility": (
                    "Same inputs, named RNG streams and tested toolchain; "
                    "not universal byte identity."
                ),
            }
            temporary_manifest = Path(folder) / "output.manifest.json"
            temporary_manifest.write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
            verify_pair(source, temporary, temporary_manifest)
            # Each replace is atomic on this filesystem, but the two-file pair is not a
            # transactional filesystem operation; consumers should always validate hashes.
            os.replace(temporary, output)
            os.replace(temporary_manifest, output.with_suffix(".manifest.json"))
    finally:
        pdf.close()
    return output


def verify_pair(clean: Path, field: Path, manifest_path: Path | None = None) -> dict[str, Any]:
    a, b = PdfReader(clean), PdfReader(field)
    if len(a.pages) != len(b.pages):
        raise ValueError("Field-copy page count differs from the clean source.")
    manifest: dict[str, Any] | None = None
    if manifest_path is not None:
        loaded = json.loads(manifest_path.read_text(encoding="utf-8"))
        if not isinstance(loaded, dict):
            raise ValueError("Manifest must be a JSON object.")
        manifest = cast(dict[str, Any], loaded)
        if manifest["source_sha256"] != digest(clean) or manifest["output_sha256"] != digest(field):
            raise ValueError("Manifest/source/output hashes do not match.")
        if manifest.get("schema_version") == 2:
            if recipe_hash(Recipe.model_validate(manifest["recipe"])) != manifest["recipe_sha256"]:
                raise ValueError("Manifest recipe hash does not match.")
            if len(manifest["pages"]) != len(a.pages):
                raise ValueError("Manifest page mapping differs from the document.")
    pristine = manifest is not None and manifest.get("mode") == "clean_passthrough"
    if pristine:
        if digest(clean) != digest(field):
            raise ValueError("Depth-zero output must be byte-identical to the clean source.")
        return {
            "passed": True,
            "pages": len(a.pages),
            "mode": "clean_passthrough",
            "byte_identical": True,
        }
    # Compare displayed page geometry, including /Rotate and CropBox, rather than
    # incorrectly treating those PDFs as if their MediaBox alone were visible.
    rendered = pdfium.PdfDocument(clean)
    try:
        for index, page in enumerate(b.pages):
            original = rendered[index]
            try:
                width, height = original.get_size()
            finally:
                original.close()
            if (
                abs(float(page.mediabox.width) - width) > 0.02
                or abs(float(page.mediabox.height) - height) > 0.02
            ):
                raise ValueError(f"Displayed page geometry changed on page {index + 1}.")
            if (page.extract_text() or "").strip():
                raise ValueError(f"Unexpected text layer on field page {index + 1}.")
            if len(page.images) != 1 or page.get("/Annots"):
                raise ValueError("Expected one flattened image and no annotations per page.")
            if manifest is not None:
                record = manifest["pages"][index]
                if (record["source_page"], record["output_page"]) != (index + 1, index + 1):
                    raise ValueError("Manifest page mapping changed.")
    finally:
        rendered.close()
    if b.attachments:
        raise ValueError("Unexpected embedded files in field copy.")
    return {
        "passed": True,
        "pages": len(a.pages),
        "same_page_geometry": True,
        "field_text_characters": 0,
        "embedded_files": 0,
        "manual_review_required": "Smallest numbers, overbars, arrows and footer legibility.",
    }


def verify_generation_sequence(clean: Path, copies: Sequence[Path]) -> dict[str, Any]:
    """Check that G1, G2 and G3 share one source and cumulative copier passes."""
    if len(copies) != 3:
        raise ValueError("A generation sequence needs G1, G2 and G3 PDFs.")
    manifests: list[dict[str, Any]] = []
    for depth, copy in enumerate(copies, start=1):
        manifest_path = copy.with_suffix(".manifest.json")
        verify_pair(clean, copy, manifest_path)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        pages = manifest["pages"]
        if any(page["generation_depth"] != depth or len(page["passes"]) != depth for page in pages):
            raise ValueError(f"G{depth} has an incorrect number of copier passes.")
        manifests.append(manifest)
    for page_index in range(len(manifests[0]["pages"])):
        for depth in (1, 2):
            prefix = manifests[depth - 1]["pages"][page_index]["passes"]
            later = manifests[depth]["pages"][page_index]["passes"]
            if prefix != later[:depth]:
                raise ValueError(f"G{depth + 1} does not extend G{depth} on page {page_index + 1}.")
        passes = manifests[2]["pages"][page_index]["passes"]
        if any(
            earlier["output_image_sha256"] != later["input_image_sha256"]
            for earlier, later in pairwise(passes)
        ):
            raise ValueError(f"Copier passes do not chain on page {page_index + 1}.")
    return {"passed": True, "pages": len(manifests[0]["pages"]), "generations": [1, 2, 3]}


def replay_pdf(
    source: Path, manifest_path: Path, output: Path, allow_toolchain_change: bool = False
) -> dict[str, Any]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 2:
        raise ValueError(
            "Replay requires a v2 manifest; keep the old engine for a v1 pixel reproduction."
        )
    if digest(source) != manifest["source_sha256"]:
        raise ValueError("Replay source hash differs from the recorded clean PDF.")
    recipe = Recipe.model_validate(manifest["recipe"])
    if recipe_hash(recipe) != manifest["recipe_sha256"]:
        raise ValueError("Replay recipe hash differs from the recorded recipe.")
    changed = {
        key: (value, versions().get(key))
        for key, value in manifest["tool_versions"].items()
        if versions().get(key) != value
    }
    if changed and not allow_toolchain_change:
        raise ValueError(
            "Toolchain changed; pass --allow-toolchain-change for a best-effort replay."
        )
    corrupt_pdf(source, output, recipe, manifest["seed"], manifest["dpi"], manifest["document_key"])
    identical = digest(output) == manifest["output_sha256"]
    return {
        "output": str(output),
        "byte_identical": identical,
        "toolchain_changes": changed,
        "passed": identical,
    }


def merge_pdfs(inputs: list[Path], output: Path) -> None:
    writer = PdfWriter()
    for path in inputs:
        writer.append(str(path))
    writer.add_metadata(
        {
            "/Title": "Blender-8 / Six fictional vendor identity proofs",
            "/Author": "Blender-8 teaching project",
            "/Creator": f"b8docs {__version__}",
        }
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        writer.write(stream)
    writer.close()
