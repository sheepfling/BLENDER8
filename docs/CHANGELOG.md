# Changelog

## 0.7.0 / New Employee route

Moved authoring inputs, owner interpretation, controlled originals, and legacy records under
`internal/`. The New Employee guide now identifies the public B8 headers and keeps simulation,
emulation, and browser tooling distinct. Correspondence, handouts, and workbench pages use the
Half-A/Labs wordmark.

## 0.6.0 / P5

Correct engineer and contractor roles; replace formal brief with customer correspondence; add validated
source/diagrams and explicit private clause dispositions. No supplier behavior change.

## 0.5.0 / P4 / B8 interface 03

Added Meridion oscillator sheet and seventh handout; expanded B8
clock/register/watchdog/deadman/reset reference; added Kestrel power/reset schematic, clock design
acceptance and execution-supervision requirements R-031..R-040. Reference C++ engine now separately
shipped as 0.3.0. Kept vendor/product ownership and third-generation recipes. This is a behavioral
B8 revision, not a cosmetic issue of fixed-clock interface02.

## 0.4.0 / P3 — six-document ownership correction

- Product requirements and specific board wiring moved into an engineering email with retained
  attachments.
- Generic B8, LCD and mux interfaces kept independent of the assembled product.
- Button firmware-recognition and restart requirements removed from the assembly contract.
- Motor drive response distinguished from the external firmware lockout; no model behavior changed.
- AVT10 controller channel, ADC sequence and system threshold examples removed from the sensor
  paper.
- MD20 and AVT10 combined into one handout, preserving their vendor headers and local page
  numbering.
- Six-document assembly, source-to-handout hashes, ownership guard, regression tests and owner audit
  added.

### Earlier release history

## Changes

### 0.3.0 / 2026-10-06

Completed seven supplier/integration manuals (27 pages), keeping six vendor identities and moving
all teaching/provenance language out of the participant-facing pages and metadata. Publication issue
P2 is separate from behavioral baseline 0.2.0. Added hash-checked MCU register generation and a
`packet` command with bookmarked binders.

Added parameterized irregular cup marks, droplets and optional retained post-copy color;
protected-content-aware edge tears with fiber/shadow edges, explicit before/after-copy stages and
applied/skipped records; O(pixels) binary-mask dilation; optional deterministic JPEG page encoding.
All seven new recipes use depth three. Existing recipe schema 2 remains valid through defaults. New
verification covers damage placement, independent randomness, encoding, source conformance and
publication language.

### 0.2.0 / 2026-10-06

The existing foundry now has reusable, validated schema-2 recipes and true repeated reproduction.
Added affine placement with no-crop containment, document-wide copier streak identity, asymmetric
vignette, roller bands, repeated blur/toner/resampling, named independent random streams,
transformed protected regions, a cumulative cosmetic-difference cap, per-page patches, generation
sweeps, schema/plan commands and manifest replay. Depth zero preserves exact source PDF bytes.
Arbitrary existing PDFs can use the same rendering engine without calling the Blender-8 generator.

Preserved the six vendor styles, complete mux datasheet, Kestrel wiring note, canonical source
excerpts and behavioral revision. Recompiled clean mux bytes matched the previous clean master. The
old 44 test cases remain, adapted only where the appearance API became stricter; new coverage
exercises the schema-2 and copy-chain contracts.

Flat v1 recipe files are not silently interpreted as v2; see the migration section of the recipe
guide. Ruff/Pyright status is explicit in the QA report.

P5 completion: added the late seven-email-thread cadence with 36 messages including handoff;
jar-interlock clarification with revised r4 drawings; 14 pending interlock cases; explicit
reference-runtime gap. No C++ source changed.
