# Blender-8 Document Foundry — owner edition 0.4.0

**Publication P3 • 6 recipient documents • 30 pages • component interface 02 unchanged.**

This is the source/owner package. The recipient-facing papers are immersive documents from invented
suppliers, not real procurement or appliance-safety specifications. The boss email is authored
in-universe correspondence; no actual email has been sent. No font files are included.

## The six-document handoff

The `dist/recipient/` folder contains exactly six image-only third-generation PDFs:

1. `01-Requirements-Email.pdf` — Kestrel engineering email, acceptance criteria, evidence
   expectations, and chassis wiring retained as attachments (6 pages).
2. `02-B8-Microcontroller.pdf` — Northstar B8 generic microcontroller user manual (6 pages).
3. `03-MX8-1-Multiplexer.pdf` — TriAxis generic selector datasheet (3 pages).
4. `04-PX32-16-LCD.pdf` — PixelRiver generic pixel-display manual (5 pages).
5. `05-Motor-and-Temperature.pdf` — Vortek MD20 (4 pages) followed by ThermaSense AVT10 (3 pages),
   with separate vendor identities and local page numbers.
6. `06-BA-8-Buttons.pdf` — Kestrel's supplied electromechanical assembly specification (3 pages).

There is no seventh board document and no separate sensor handout. Seven LaTeX source units are
maintained to preserve independent supplier authorship, but they assemble into six recipient
documents. Clean alternatives are in `dist/recipient-clean/`. The two
`Blender8-Supplier-Manuals-*.pdf` files are convenience binders with six top-level bookmarks, not
additional assigned documents.

## The ownership rule

The product brief specifies the finished machine's speeds, interpretation of PULSE, firmware timing
budgets, temperature thresholds, diagnostic vocabulary, lockout/rearm policy, tests and specific
board wiring. Supplier manuals specify local inputs/outputs, registers, delays, physical transfer,
and the absence or presence of intrinsic behavior. The assembly manual specifies the mechanism
actually supplied, not how the firmware interprets it.

An MD20 with its obstruction removed responds to its current inputs. It does not secretly enforce
the firmware's restart policy. An AVT10 supplies voltage; it does not know B8 AN0 or a 65/85-degree
trip scheme. A BA-8 contact bounces within 5 ms; a 50-ms human-command acceptance rule belongs to
the product brief.

See `docs/DOCUMENT-OWNERSHIP-AUDIT.md` for findings, before/after responsibilities and requirement
traceability. `source_inputs/` remains the original hash-checked behavior baseline; it is not
student-facing material.

## Build locally

Python 3.12+, a TeX installation providing pdfLaTeX/latexmk, and the dependencies in
`pyproject.toml` are required for a full build. Rendering an already compiled PDF does not require
TeX.

```bash
python -m pip install -e '.[dev]'
b8-docs doctor --json
b8-docs audit --json
b8-docs packet --seed 4815 --json
python -m pytest -q
```

Without installation, from this checkout use `PYTHONPATH=src python -m b8docs ...` on a POSIX shell.

The `packet` command audits ownership, compiles the independent sources, applies their recorded copy
recipes, packages the six handouts and builds the convenience binders. Output folders
`dist/recipient/` and `dist/recipient-clean/` are regenerated as managed directories. Keep unrelated
files out of them.

```bash
b8-docs build --doc requirements --recipe config/manuals/requirements.toml
b8-docs build --doc motor --recipe config/manuals/motor.toml
b8-docs build --doc temperature --recipe config/manuals/temperature.toml
```

The last two commands build individual source papers for authoring; run `packet` for the combined
motor/sensor handout. No board catalog entry is published in this release. A historical board source
is under `owner/legacy/` solely for the audit trail.

## Reproduction and verification

Recipes retain independent copier passes, page transforms, asymmetric shadows, inherited streaks,
coffee and margin tears. Protected regions move with the paper. Every independently rendered source
paper has a sidecar manifest; `dist/packet-build.json` maps those papers into the six handouts with
hashes and page counts. See `docs/RECIPE-GUIDE.md` for parameters and replay.

The boundary audit is a declared lexical regression check plus packet-membership validation, not an
automated proof of semantic correctness. Visual review and ownership review remain required. The
latest executed checks and their limitations are in `docs/QA-RESULTS.md`; historical claims are not
automatically carried forward. This documentation release does not change or newly validate C++
behavior, HIL, electrical ratings or real appliance safety.

## Archive contents

This owner archive includes the source tree, six ready-to-issue PDFs in `dist/recipient/`, clean
source papers and clean handouts, the clean convenience binder, rendering manifests and verification
records. The combined G3 convenience binder and duplicated per-source field PDFs are supplied
separately or regenerated by `packet`; they are not repeated inside this archive. The separate
six-document handoff ZIP contains only the six recipient PDFs. Recorded absolute build paths in
evidence describe the executed environment; use the relative source/output layout and CLI commands
above on another machine.
