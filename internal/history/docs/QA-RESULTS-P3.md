# Release verification — engine 0.4.0 / publication P3

**Executed during the October 6, 2026 (America/Chicago) session; project-owner evidence, not a
supplier paper.**

## Automated results

`PYTHONPATH=src python -m pytest -q`: **174 passed, 0 failed, 0 skipped**, in **9.06 seconds**.
`PYTHONPATH=src python -m compileall -q src tests` completed successfully. Exact pytest output, the
installed tool inventory and machine-readable release checks are in `dist/`. Ruff and Pyright were
unavailable and not run.

The suite retains renderer, recipe, geometry, source generation, deterministic copy-chain, replay,
encoding, tear/coffee, manifest and CLI tests. New cases cover exact six-document membership,
removal of the seventh board entry, lexical ownership checks (including deliberate sensor-to-B8/AN0
leakage), all 30 original requirement identifiers, critical product limits, motor behavior without
an intrinsic obstruction-history latch, button bounce without a firmware hold policy, and the two
vendors in the combined motor/sensor handout.

## Compilation and behavioral baseline

Seven independently authored LaTeX units compiled with the expected page counts: email 6; B8 6; mux
3; LCD 5; motor 4; sensor 3; buttons 3. They form **six handouts totaling 30 pages**. Compilation
rejected overfull horizontal/vertical boxes. The original hash-checked source snapshots remain
unchanged, and B8 register tables are regenerated from those snapshots. The existing engineering C++
package was not changed.

The original R-001 through R-030 requirements are represented in the email attachments; product
numerical limits were retained. This is an editorial ownership revision, not a formal proof of every
sentence's equivalence to the C++ model. It does not constitute newly passed firmware acceptance,
physical HIL or appliance certification.

## Publication checks

All seven source-paper field PDFs passed recorded source/output hashes, page count, page geometry,
image-only content and no-embedded-file checks. The recipient folder contains exactly the six
declared PDFs with page counts **6, 6, 3, 5, 7, 3**. The motor/sensor handout retains separate
Vortek and ThermaSense pages and two section bookmarks. The clean and G3 binders each have **30
pages and six top-level bookmarks**, with the component subdivisions retained within document 5.

All **30 clean pages and 30 encoded field pages** were rendered and visually inspected. Review
included the new email and wiring attachment, rewritten button and sensor sections, MCU register
pages, LCD packing and timing, and motor/thermal diagrams. No visible clipping, overlaps or
destroyed technical region was accepted. This visual review is not an automated or universal
legibility guarantee.

In-world source/text checks passed for prohibited teaching/provenance terminology. The boss email is
correspondence, not a sixth vendor impersonation. Product targets, thresholds and fixed board wiring
belong there; supplier papers retain intrinsic component behavior and generic interface
instructions. The audit's lexical checks complement, but cannot replace, semantic review.

## Reproduction

The complete **three-page AVT10 field PDF** was replayed from its clean master and recorded
manifest: **byte-identical, passed, no toolchain changes and no override**. The result is in
`dist/replay-result.json`. The smaller unit-test replay and encoding checks also passed. The other
full-size papers and the full binder were verified structurally but were not separately replayed
during this release. Do not infer cross-platform reproducibility from a same-environment replay.

Release recipes use 220 DPI, three chained copy passes and JPEG quality 96 with chroma subsampling
disabled. Protection limits and raw-pixel hashes apply before encoding; decoded JPEG pixels can
differ. Tear clearance is a geometric exclusion test, not OCR. Clean vector-text editions and
optional lossless encoding remain available.

## Not performed

Ruff, Pyright, Windows/macOS builds, physical printer/copier calibration, OCR-resistance
measurements, exhaustive prose-to-model semantic validation, electrical or mechanical certification,
completed firmware acceptance and physical HIL acceptance were not performed. No standalone font
files, stock textures or real-company logos are distributed. No actual email was sent.
