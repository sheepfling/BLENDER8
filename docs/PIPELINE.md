# Current document pipeline

`python tools/project.py docs --root .` validates the controlled inputs and correspondence chain,
generates the shared TeX fragments, and compiles the eight assignment sources and the optional
[B16 addendum](../platform/docs/B16-FOLLOW-ON.md) without shell escape.
It checks page counts, text presence and layout overflow before copying pristine PDFs to
`build/docs/clean/`.

The validated TOML recipes then run three cumulative copier passes with the same seed. G1 and G2
source-unit PDFs, seven-handout previews and binders are retained under `build/docs/`. The pass
manifests show that G2 extends G1 and G3 extends G2. These are stages of one pixel-copy chain;
the finished G3 source-unit PDFs are saved under `dist/field/`.

Only G3 handouts go into `dist/recipient/`. The final convenience binder is
`dist/Blender8-Supplier-Manuals-G3.pdf`. The motor and sensor retain separate supplier source
documents but share one recipient handout. `build/docs/packet-build.json` records the paths and
hashes.
The owner crosswalk and retired teaching requirements are outside the recipient packet.
The B16 G3 copy remains a separate field PDF; it is not added to the seven-handout binder.

`python tools/assemble_existing.py --root .` verifies the pristine and three copy stages, then
reassembles handouts and binders without recompiling LaTeX. Both `build/` and `dist/` are ignored by
Git; only source and recipes are tracked. The single-document `b8-docs build --doc NAME` route uses
that document's `internal/authoring/config/manuals/NAME.toml` recipe and the same output locations.
