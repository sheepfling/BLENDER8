# Source migration contents

This checkout combines the document foundry source from `HalfALabs-Blender8-Complete-v0.7.0.zip` and
the active C++ source from the unpacked `HalfALabs-B8` 0.6.0 intake. The earlier 0.4.0
`reference-platform/` snapshot inside the document package was superseded. Its historical records
now live under `internal/history/`. The unpacked inputs remain under ignored
`.local/source-archives/original-intake/`.

The parts-manual G3 ZIP contains seven published field PDFs already represented by the foundry's
LaTeX sources, recipes, and Python renderer. The separate preview PNG is a published output. Both
are retained with the source archives, outside the tracked source tree.

Run `python tools/project.py docs` to save pristine and G1/G2 PDFs under ignored `build/docs/`
and the final G3 packet under ignored `dist/`;
`native` builds the active C++ platform; `wasm` builds the C++ WebAssembly target and page;
`wasm-test` checks real compiled modules against the native reference. See
[README.md](../README.md) for setup and evidence boundaries.
