# Blender-8 Document Foundry 0.6.0

**P5: HalfALabs customer correspondence.** Component interfaces and reference platform retain the
versions printed in their manuals.

Document 1 is seven archived customer/contractor exchanges, Rick's handoff to Jordon and two
retained drawings. The seven-document handoff remains seven PDFs; the six supplier handouts are
unchanged.

```sh
python -m pip install -e ".[dev]"
b8-docs build --doc requirements --recipe config/manuals/requirements.toml --seed 4815
b8-docs packet --seed 4815
python -m pytest -q
```

Edit `correspondence/archive.json`. Its Pydantic model validates roles, reply chronology, unique
IDs, sketch references and final handoff. The build generates `generated/correspondence-body.tex`
and a Markdown transcript. Clarification sketches and circuit drawings are in
`latex/correspondence/`. The LaTeX master passes through the existing parameterized third-generation
renderer.

`dist/recipient/` is the immersive handoff. Owner notes, historical requirements and source code are
NOT extra recipient documents. Read `docs/CORRESPONDENCE-EDITORIAL-RECORD.md` before using old
acceptance materials: it maps all forty former clauses and explicitly delegates overprescribed
algorithms. Old reference firmware scenarios must not silently outrank the new correspondence.

This is a documentation revision, not new hardware or finished firmware. Current evidence is
`docs/P5-VALIDATION.md`; inherited logs are historical.

**Chassis-04 implementation gap:** The jar loop/dropout latch in the final correspondence and r4
drawings is not present in the bundled v0.3.0 reference simulator. Implement its model, GPIO
connections and pending conformance cases before using that executable for the revised assignment.
This package does not claim jar-interlock firmware or hardware acceptance.
