# Document Foundry: reusable recipe and rendering contract

## Engine 0.4.0 | schema 2 | publication P3 | October 6, 2026

## 1. Separate the three decisions

**Content** is the component contract and its clean LaTeX. **Vendor identity** is the typography,
logo, headings and document-control language in a vendor `.sty`. **Copy history** is an independent
appearance recipe. A different coffee ring is not a different settling delay, revision, package or
pinout.

The renderer accepts an existing PDF and a validated recipe. Its effects do not depend on Blender-8,
mux classes, motor models or the authoring catalog. The authoring wrapper compiles seven independent
source units, generates MCU register tables from checked source snapshots, and assembles exactly six
handouts through the packet command. Product requirements and component operation have separate
authorship roles.

The six existing styles are retained: Northstar for MCU documentation, PixelRiver for LCDs, TriAxis
for commodity logic, Kestrel for assembly paperwork, Vortek for motion hardware and ThermaSense for
analog sensing. Publication P3 contains six handouts totaling 30 pages: the requirements email with
acceptance and wiring attachments, B8, mux, LCD, motor with sensor papers, and the button assembly.
The motor and sensor retain separate supplier identities inside the fifth handout; no seventh wiring
document is issued. Legacy vendor-identity proof sources remain development material, outside the
supplier packet.

## 2. Files and modules

```text
config/
  catalog.toml                  document sources, vendor and default preset
  presets/*.toml                clean/lab/shop/archive/bad-xerox presets
  examples/*.toml               partial, reusable document-specific recipes
schemas/recipe-v2.schema.json   generated editor schema
latex/                         unchanged canonical content and six house styles
src/b8docs/
  recipe.py                    validation, merge, overrides and recipe hashing
  artifact.py                  copy-chain scheduler and protected-region cap
  effects/common.py            named random streams and raw-pixel hashing
  effects/affine.py            containment, translation, shear and transform records
  effects/optical.py           blur/toner, asymmetric vignette, copier lines
  effects/handling.py          paper, coffee, fold, staple, holes, dust and stamp
  pipeline.py                  TeX, rasterization, image-only PDF, manifests/replay
  cli.py                       human and JSON commands
```

Each effect consumes an image, a small typed settings object, a DPI and an explicit random
generator; it returns the result and, where applicable, a parameter record. It neither reads PDFs
nor reaches into the component models. The orchestrator owns the order and accumulated state.

## 3. Physical copy order

```text
source PDF -> visible-page raster
  -> initial paper tint / optional monochrome rendering
  -> handling marks, when configured before copying
  -> copy 1:
       place sheet on copier bed (rotation/shift/shear/contained scale)
       optical reproduction (blur, resample, contrast, toner)
       asymmetric platen/lid shadow
       new scanner-coordinate streaks and roller banding
       aggregate protected-region limit
  -> copy 2: consumes ALL of copy 1, not a fresh source raster
  -> copy 3 / copy 4: same rule
  -> optional handling marks applied after copying
  -> final protected-region limit
  -> flattened PDF + replay manifest
```

A previous scan's edge and copier streak travel with that sheet on the next pass. The next copier's
new streak is added in its current scanner coordinates. That is the crucial difference from
repeatedly sprinkling dirt on a pristine page.

`handling.stage` is `before_copy` or `after_copy`. Before-copy stains and stamps are reproduced with
the sheet; after-copy marks represent the final person handling the resulting sheet. This choice is
a recipe setting, not arbitrary effect reordering.

`generation_depth` is an actual pass count, bounded to 0–4. It does not secretly multiply every
parameter. Passes have independent named seeds, and cumulative damage comes from feeding their
output forward. Depth 0 means no wear. A wholly depth-zero PDF job preserves the exact original
bytes; mixed per-page depths are rasterized consistently, including pristine pages in that mixed
job.

## 4. Configuration precedence

Global resolution is **model defaults < named preset < catalog document appearance patch < custom
recipe file < repeated `--set` overrides < explicit `--depth` / `--dpi` flags**.

`extends` in a custom TOML file selects one built-in preset. Recursive arbitrary-file inheritance is
deliberately not supported. A full custom file may replace every field; omitted fields retain the
selected preset/model defaults.

Nested objects are recursively merged. Arrays, including protected-region lists, are replaced as a
whole rather than implicitly concatenated. Unknown keys, invalid ranges, NaN/infinity, impossible
colors and unsupported generation counts are rejected. Validation is performed after merging; no
unchecked `model_copy(update=...)` bypass is used.

Page-specific patches are resolved after the global settings. Their page numbers are **1-based**. A
page can appear in only one rule. Page patches cannot change DPI, PDF output encoding, or introduce
another layer of page rules. An explicit CLI `--depth` is global and removes page-specific depth
overrides. Other global CLI edits do not erase deliberately more-specific per-page patches.

## 5. Practical custom recipe

This is the supplied `config/examples/mux-returned-copy.toml`:

```toml
extends = "bad_xerox"
name = "mux_returned_copy"
generation_depth = 2
copier_id = "service-room-1979"

[placement]
rotation_deg = { min = -1.4, max = 1.4 }
translate_x_mm = { min = -2.0, max = 2.0 }

[handling]
coffee = false
stamp = "RETURN TO LOGIC FILE"

[[page_overrides]]
pages = [2]
[page_overrides.patch.protection]
max_delta = 0.075
[page_overrides.patch.repro]
toner_bloom = 0.04
threshold_mix = 0.06
```

The small timing labels on page 2 receive a stricter cosmetic limit than the rest of the document.
This is not a claim that the clean page needs a different electrical specification.

## 6. Parameter vocabulary

| Setting                                      | Meaning                                                             | Supported envelope                                     |
| -------------------------------------------- | ------------------------------------------------------------------- | ------------------------------------------------------ |
| `dpi`                                        | Raster sampling density                                             | Integer 150–450; document-wide                         |
| `generation_depth`                           | Number of chained reproduction passes                               | Integer 0–4                                            |
| `placement.rotation_deg`                     | Per-pass sheet rotation; positive is clockwise in image coordinates | Ordered min/max, within -5 to +5 degrees               |
| `placement.translate_x_mm`, `translate_y_mm` | Requested placement offset                                          | Ordered ranges within -10 to +10 mm                    |
| `placement.shear_deg`                        | Small horizontal shear                                              | -1 to +1 degree                                        |
| `placement.scale`                            | Requested linear page scale                                         | 0.90–1.05, additionally constrained by no-crop fitting |
| `placement.inset_mm`                         | Minimum retained border around the transformed sheet                | 0–12 mm                                                |
| `vignette.strength`                          | Peak shadow intensity range                                         | 0–0.50                                                 |
| `vignette.width_mm`                          | Edge falloff width                                                  | 0.1–40 mm                                              |
| `vignette.edge`                              | Dominant copier-bed edge                                            | left/right/top/bottom/random                           |
| `vignette.asymmetry`                         | How much the chosen edge dominates                                  | 0–1                                                    |
| `streaks.horizontal`, `vertical`             | Numbers of new lines per pass                                       | Integer min/max, 0–30                                  |
| `streaks.width_mm`                           | Nominal Gaussian line width                                         | 0.02–3 mm                                              |
| `streaks.opacity`                            | Per-line brightness/darkness fraction                               | 0–0.35                                                 |
| `streaks.light_fraction`                     | Chance that a line is a light dropout rather than dark toner        | 0–1                                                    |
| `streaks.waviness_mm`                        | Small line-position oscillation                                     | 0–2 mm                                                 |
| `streaks.roller_pitch_mm`                    | Period of repeating roller marks                                    | 2–100 mm                                               |
| `repro.blur_mm`                              | Gaussian reproduction blur scale                                    | 0–0.12 mm                                              |
| `repro.resample`                             | Downsample/upscale ratio, applied each pass                         | 0.60–1.00                                              |
| `repro.contrast`                             | Contrast multiplier about middle gray                               | 0.90–1.40                                              |
| `repro.threshold_mix`                        | Blend toward a thresholded copy                                     | 0–0.50                                                 |
| `repro.toner_bloom`                          | Blend toward a 3-pixel minimum-filter result                        | 0–0.40                                                 |
| `repro.grain_sigma`                          | Gray-level standard deviation before quantization                   | 0–3 on the 0–255 scale                                 |
| `handling.*`                                 | Coffee, fold, staple, punch holes, dust and a cosmetic stamp        | Independently optional                                 |
| `protection.max_delta`                       | Maximum cosmetic RGB-channel difference inside a protected region   | 0–0.25 of full channel scale                           |
| `protection.min_cumulative_scale`            | Reject excessive reduction across several copies                    | 0.50–1.00; default 0.78                                |

These are supported configuration bounds, not recommended operating settings for every sheet. The
supplied presets are substantially more restrained. The JSON Schema exposes the field structure and
basic constraints; ordered-range and contextual checks also run in Pydantic validators.

Angles and physical distances are independent of the output resolution. Pixel quantization, font
rasterization, grain and the three-pixel morphological bloom still depend on resolution; identical
pixels at different DPIs are neither expected nor claimed.

### Preset defaults

| Preset         | Depth |      Per-pass rotation | Intended copy history                                            |
| -------------- | ----: | ---------------------: | ---------------------------------------------------------------- |
| `clean`        |     0 |           None applied | Canonical PDF passthrough                                        |
| `lab_copy`     |     1 | -0.45 to +0.45 degrees | Restrained engineering-file copy                                 |
| `shop_copy`    |     2 |   -0.9 to +0.9 degrees | Warm paper, coffee, fold and workshop copier                     |
| `archive_scan` |     2 | -0.55 to +0.55 degrees | Binder shadow, punch holes and gray archive scan                 |
| `bad_xerox`    |     3 |   -1.6 to +1.6 degrees | Crooked repeated reproduction, platen shadows and multiple lines |

Per-pass rotations can add or cancel. A depth-three document is not guaranteed to have exactly three
times the final angle of a depth-one document. Actual angles, translations and the cumulative matrix
are recorded.

## 7. Geometry and protection

The input sheet's **entire rectangle** is fitted into each new canvas. The matrix is computed around
the sheet center, scaled to retain its outer corners, then translated only as far as the remaining
border permits. A requested 2-mm displacement may be clamped by containment; both the requested and
actual values appear in the manifest. Shear and enlargement are similarly subordinate to the no-crop
rule.

Protected rectangles are normalized source-page coordinates with origin at the top left: x increases
right; y increases down. A small source-space dilation covers interpolation support. The same affine
matrix is applied to the page, a geometry-only reference and the protection mask at every pass. No
protection rectangle remains stranded in the old coordinates after rotation.

The damage limit is **aggregate**, not a fresh allowance for every stain and line. Within the
transformed mask, each channel of the candidate output is bounded relative to the geometrically
transformed clean reference. The reference itself includes geometric interpolation but no cosmetic
wear. The maximum observed difference is recorded after each pass and at the end.

This limit is useful but not a reading-comprehension test. It does not detect a misleading copied
diagram, guarantee that a tiny serif survived downsampling, or protect a region not included in the
mask. Maintain the clean companion and review small values at actual size. Do not call an optical
limit a safety certification.

The PDF retains its displayed page sizes and order, including sources with a rotated page or
CropBox. Internally fitted artwork is reduced slightly: these copies are **not dimensionally
calibrated drawings**. Existing fictional diagrams are logical illustrations, not physical measuring
templates.

## 8. Randomness and copier identity

The renderer explicitly uses PCG64 generators seeded by SHA-256-framed keys. It never uses
process-randomized Python `hash()` or global NumPy randomness.

Page geometry, reproduction and vignette use streams keyed by seed, document key, zero-based
internal page index, pass number and effect name. Toggling the coffee ring or vignette does not
shift the later geometry/streak random draws. The generated pixels will still change because the
effects interact physically.

Copier streaks use a job-level stream based on **seed + copier_id + pass number**, not the page
index. Same-size pages therefore share recurring defect positions while their sheet placement
varies. Positions are normalized scanner coordinates; line widths and roller spacing are physical
millimeters. A differently sized page does not preserve the exact same absolute line position.

Page insertion changes subsequent page-index keys; stable semantic page IDs are not implemented.
Give explicitly related jobs the same `--key` and `copier_id` when that relationship matters.
Otherwise an arbitrary PDF defaults to its filename stem as its document key.

## 9. Manifests and replay

Each output has an adjacent `.manifest.json` with the source and output hashes, complete resolved
recipe, recipe hash, document key, seed, DPI, effect order, package/runtime versions, engine
Python-source fingerprint, page mapping and actual per-pass parameters. Pass records include their
input/output pixel hashes, demonstrating the copy chain. Pixel hashes cover image mode, dimensions
and raw pixels, not PNG-encoder metadata.

There is no wall-clock timestamp mixed into deterministic render identity. Supplier PDF metadata now
stays in-world; the sidecar manifest and private project notes retain synthetic/provenance
information. For a real build service, record wall-clock job events in a separate operational log.

```sh
b8-docs replay clean.pdf old-copy.manifest.json reproduced.pdf --json
```

Replay checks the clean-source hash and resolved recipe, compares the recorded toolchain/engine
fingerprint and rebuilds using the recorded keys. Renaming the clean PDF does not change the
recorded document identity. A byte mismatch returns a nonzero status rather than a success claim.
`--allow-toolchain-change` permits an explicitly best-effort attempt; the output hash is still
compared and reported.

Exact byte reproduction has been exercised in the supplied environment, not universally across
operating systems, PDFium builds, TeX distributions or codec implementations. Pinning Python
dependencies improves traceability but does not erase those distinctions. A manifest is an audit
record, not a cryptographic signature or tamper-proof attestation.

## 10. CLI contract

| Command    | Work performed                                                                                            |
| ---------- | --------------------------------------------------------------------------------------------------------- |
| `doctor`   | Separate existing-PDF rendering readiness from optional TeX tools                                         |
| `plan`     | Resolve and validate a recipe; emit its full settings and hash without rendering                          |
| `schema`   | Emit the versioned JSON Schema                                                                            |
| `generate` | Validate the existing mux snapshot and generate TeX constants                                             |
| `build`    | Compile one catalog document; optionally create its `*.field.pdf`                                         |
| `corrupt`  | Apply a recipe to an existing PDF without recompiling LaTeX                                               |
| `sweep`    | Render a controlled list of generation-depth variants                                                     |
| `replay`   | Reconstruct a recorded build and check the output hash                                                    |
| `proofs`   | Compile legacy development identity proofs; not part of the supplier packet                               |
| `packet`   | Audit and compile seven source units, render their recipes, assemble six handouts and convenience binders |
| `audit`    | Check declared document-ownership boundaries and exact six-document membership                            |
| `verify`   | Check source/output hashes, page mapping, displayed geometry and field structure                          |

Most operational commands support `--json`. Configuration/runtime validation errors produce exit
code 1; a replay that completed but did not reproduce the expected hash returns 2. Typer's
argument-syntax errors also follow its CLI error handling.

The renderer accepts 1–250 pages and limits each raster to 25 megapixels. It advances one page image
at a time, but the PDF writer still retains compressed object data: constant-memory operation for
arbitrary giant packets is not claimed. The trusted local TeX build disables shell escape and checks
compilation, expected page count and layout overflow. Neither parser is advertised as a sandbox for
hostile PDFs or hostile LaTeX.

Clean-source overwrites, including same-file hard-link aliases, are refused. Temporary rendering
output is structurally verified before replacement. The PDF and sidecar are individually replaced
atomically on the local filesystem, not as an atomic two-file transaction; a consumer should always
validate their paired hashes.

## 11. Migration from 0.1.0

The old flat `config/recipes.toml` is replaced by one schema-2 preset per file. A reference copy of
the old values is retained in `docs/migration/`; it is not silently accepted as a v2 recipe.

| Old appearance field            | New home                                             |
| ------------------------------- | ---------------------------------------------------- |
| `rotation_deg` magnitude        | `placement.rotation_deg = {min=-a,max=a}`            |
| `inset` fraction                | `placement.inset_mm` physical inset                  |
| `blur_px_at_300dpi`             | `repro.blur_mm`; convert pixels x 25.4 / 300         |
| `bands`                         | separate horizontal/vertical streak count ranges     |
| `edge_shadow`                   | asymmetric `vignette` settings                       |
| `coffee`, `fold`, `stamp`, etc. | `handling` settings                                  |
| `protected` rectangles          | `protection.regions`                                 |
| per-mark `protected_effect_cap` | cumulative reference-relative `protection.max_delta` |

The public CLI name remains `b8-docs`. `build` uses a stable `*.field.pdf` output name; use
`corrupt` or `sweep` for explicitly named variants. v1 render manifests cannot promise v2 pixel
reproduction; retain the old engine when preserving an old copy byte-for-byte matters. Behavioral
spec revision 0.2.0 remains unchanged. The current publication issue is P3; component interface
revision 02 is retained.

## 12. Review and future work

The delivered acceptance evidence covers configuration rejection, source preservation, geometry,
deterministic streams, real copy chaining, aggregate protection, rotated/cropped PDFs, replay,
image-only structure and the existing component-document tests. Visual review covers actual
generated pages, with special attention to timing symbols and small footer text.

Perspective curl, warped book-spine text, semantic page IDs, a GUI recipe editor, automatic
content-region extraction, OCR measurement, physical scanner calibration and physical hardware
validation are not implemented by this release. The LCD, MCU, motor and other component manuals are
now delivered. Those are distinct additions, not hidden requirements for reusing the working
renderer.

### Implementation references

These describe the library contracts used by the engine, not the fictional component behavior:

- Pillow, ImageTransform: <https://pillow.readthedocs.io/en/stable/reference/ImageTransform.html>
- Pydantic, models and nested validation: <https://docs.pydantic.dev/latest/concepts/models/>
- NumPy, random compatibility policy:
  <https://numpy.org/doc/stable/reference/random/compatibility.html>

Consulted October 6, 2026. The locally tested versions are recorded separately; the newest
documentation version is not a claim that the project uses an untested newer dependency.

## 13. Coffee, tears and packet publication

Use `b8-docs packet --seed 4815` to build the six-document, 30-page handoff. Seven source PDFs are
compiled independently to preserve supplier authorship. Recipes are in `config/manuals/`; all use
three chained copies. The component PDFs and binder omit out-of-world teaching/provenance language.
Keep `docs/HANDOFF.md`, source snapshots, QA records and manifests outside the participant-facing
packet.

### Independent handling and tear histories

`handling.stage` schedules coffee/folds/stamps before or after copying. `tears.stage` independently
schedules missing paper before or after copying. Before-copy damage moves with later placements;
after-copy damage belongs to the final output sheet. Tear candidates are tested against the current
transformed protection mask plus physical clearance. Unsafe candidates are retried up to the
configured limit, then skipped and recorded. The engine does not bleach erased text back over a
tear.

The following is a complete minimal override recipe; unspecified values inherit `bad_xerox`:

```toml
extends = "bad_xerox"
name = "returned_service_copy"
generation_depth = 3
dpi = 220

[handling]
stage = "after_copy"
coffee = true
preserve_color_after_copy = true
opacity = 0.45
stamp = "SERVICE FILE"

[handling.coffee_profile]
radius_mm = {min = 21.0, max = 27.0}
center_x = {min = 0.96, max = 1.02}
center_y = {min = 0.76, max = 0.88}
rim_width_mm = 0.60
irregularity = 0.07
broken_rim = 0.50
droplets = {min = 2, max = 5}

[tears]
enabled = true
stage = "before_copy"
count = {min = 1, max = 2}
edges = ["right", "bottom"]
depth_mm = {min = 1.5, max = 4.0}
length_mm = {min = 5.0, max = 13.0}
clearance_mm = 1.0

[output]
encoding = "jpeg"
jpeg_quality = 96
```

`center_x` and `center_y` are normalized sheet coordinates; values just outside 0–1 allow a partial
cup ring running off the edge. Coffee is an irregular, interrupted contact rim with weak seepage,
secondary bloom and droplets, not a perfectly circular dark outline. Its dimensions, location and
droplets are recorded. `preserve_color_after_copy` only retains colored handling when the handling
stage is after copying.

| New setting                         | Supported range / meaning                     |
| ----------------------------------- | --------------------------------------------- |
| `handling.coffee_profile.radius_mm` | Ordered 4–50 mm                               |
| `center_x`, `center_y`              | Ordered -0.15 to 1.15, relative to sheet      |
| `rim_width_mm`                      | 0.1–3 mm                                      |
| `irregularity`                      | 0–0.2                                         |
| `broken_rim`                        | 0–0.85                                        |
| `droplets`                          | Integer count range 0–30                      |
| `tears.depth_mm`                    | Ordered 0.2–12 mm                             |
| `tears.length_mm`                   | Ordered 1–40 mm                               |
| `tears.edges`                       | Unique nonempty list of left/right/top/bottom |
| `tears.clearance_mm`                | 0–5 mm from protected content                 |
| `tears.roughness`                   | 0–0.5                                         |
| `tears.fiber_mm`                    | 0.05–0.8 mm edge treatment                    |
| `tears.attempts_per_tear`           | Integer 1–128, default 32                     |
| `output.encoding`                   | `lossless` (default) or `jpeg`                |
| `output.jpeg_quality`               | Integer 85–100; applies only to JPEG          |

Coffee and tears have named independent random streams, so changing them does not reseed page
placement or copier lines. Tear records include accepted polygons, removed-pixel counts, stage, and
skipped requests. The tear clearance uses a binary mask; it is not a text-recognition system.

### Encoding and recorded hashes

The release manual recipes use 220 DPI and JPEG quality 96, with chroma subsampling disabled.
Default recipes remain lossless. The manifest explicitly distinguishes raw pixel hashes **before
encoding** from the complete encoded PDF hash. JPEG is lossy, so the pre-encoding protected-region
RGB limit is not an exact bound on the decoded PDF pixels. Tear exclusion and placement bounds are
geometric checks before that encoding step. Review the encoded PDF, not just the pre-encoding
raster.

Use `--set 'output.encoding="lossless"'` when a lossless image stream is required; this may make the
packet substantially larger. Neither choice recovers vector text or removes the earlier
rasterization/resampling stages. No OCR or perceptual-reading guarantee follows from a pixel hash.

### Compatibility

Recipe schema 2 is retained: coffee/tear/output fields have defaults that make older recipes parse.
Appearance output changes across engine versions and is tracked by source fingerprint; retain the
recorded engine when reproducing earlier manifests. Publication P3 changes document ownership and
packaging, not register addresses or motor physics.

## 14. Product brief versus supplier papers

`spec/document-roles.json` declares the ownership rules. `b8-docs audit --json` checks lexical
leakage and six-document membership; `packet` invokes that audit before publication. This is a
regression guard, not an automated semantic proof. The source papers retain manual review
requirements.

`dist/recipient/` and `dist/recipient-clean/` are managed directories regenerated by `packet`; keep
unrelated files out of them. Each contains six PDF documents in the same order. Document 1 is the
boss email plus acceptance criteria and chassis wiring attachments. Document 5 joins the
independently authored motor and sensor PDFs, retaining both vendors and local page numbers.
`dist/packet-build.json` records the source-unit-to-handout mapping and file hashes.

The combined clean and G3 binders have six top-level bookmarks. Standalone source-paper manifests
support replay of each original rendering; the bundle mapping supports verification after
concatenation. The former standalone board document is not in the current catalog and survives only
as an owner-side historical source.
