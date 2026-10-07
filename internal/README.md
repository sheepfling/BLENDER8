# Maintainer material

This directory is outside the New Employee assignment route. Its files support repeatable PDF
generation, source provenance, and owner review. The accepted recipient contract is the
correspondence under `platform/requirements/` and the finished handouts under `dist/recipient/`.

| Location                                          | Purpose                                                        |
| ------------------------------------------------- | -------------------------------------------------------------- |
| `authoring/config/`                               | Document catalog and copier recipes                            |
| `authoring/correspondence/`                       | Canonical source for the printed and Markdown mail file        |
| `authoring/schemas/`, `authoring/spec/`           | Document validation and source role declarations               |
| `authoring/source_inputs/`                        | Hash-checked original inputs; preserve their bytes             |
| [`owner/`](owner/README.md)                       | Semantic crosswalk, editorial record, and local interpretation |
| `history/docs/`, `history/evidence/`              | Earlier foundry releases and their recorded evidence           |
| `history/platform/`                               | Retired platform requirements, scenarios, and release records  |

The active LaTeX files remain at `../latex/` so the open document editor keeps its source path.
Python tooling under `../src/b8docs/` reads the authoring inputs through explicit paths relative
to the repository root supplied with `--root`.

Do not put new employee instructions or current customer requirements in owner notes. The
crosswalk explains how the authored correspondence maps to earlier clauses; it is not a private
grading contract. Owner-local `owner/private/` is Git-ignored and omitted from source archives.

The source ZIP produced by `tools/project.py bundle` excludes `owner/` and `history/`. It includes
the authoring inputs needed to regenerate PDFs. Distribute the seven finished PDFs and the intended
platform source when preparing a New Employee handoff; keep owner interpretation separate.
