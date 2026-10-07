# Provenance and inspiration

## Behavioral source

The engineering baseline is the user's existing **Blender8-v0.2.0.zip**, supplied in this
conversation, and its accompanying **Blender8-Design-Pack-v0.2.0.md**. The original archive and
consolidated document are not overwritten by this project. The standalone vendor sheets are a new
format/presentation deliverable.

The following unmodified archive entries are retained here for comparison:

| Archived entry                            | Local snapshot                         |
| ----------------------------------------- | -------------------------------------- |
| `blender8-v0.2.0/docs/datasheets/MUX8.md` | `source_inputs/MUX8-v0.2.0.md`         |
| `blender8-v0.2.0/docs/BOARD_WIRING.md`    | `source_inputs/BOARD_WIRING-v0.2.0.md` |

- `Blender8-v0.2.0.zip` SHA-256: `276ea731b887fafdcfd6d43a482667645fc1565ecf408474cc7af415663c847d`
- `Blender8-Design-Pack-v0.2.0.md` SHA-256:
  `1f194e45458e6fad200cccf7ca64b2e893c5bc47b1d7cbc4d442af91780be6bf`
- `MUX8-v0.2.0.md` SHA-256: `5ef1e7a5dba2d1f18930e39aa7335186b22e2d1e40771273c6890a290a25edca`
- `BOARD_WIRING-v0.2.0.md` SHA-256:
  `4aaaee590a97aa69e1a652e918f92d6d538cf6b5a4022c3b7a6e695a505659e3`

The presentation edition expands the short mux description into a function table, logical connection
drawing, explanatory timing diagrams and integration guidance. It adds no commercial supply/package
ratings, changes no firmware API and makes no physical safety claim.

## Creative boundary

The inspiration is SHENZHEN I/O's idea of learning from a packet of different manufacturers'
original component documentation. Zachtronics' official game page describes both the
multiple-manufacturer components and the included original datasheets/reference manual. This project
uses that *idea*, not the game's page layouts, logos, characters, text or artwork. The fictional
company identities and TikZ marks here were authored for Blender-8.

## Primary implementation references

Consulted 2026-10-06. These explain the tools; they are not sources for the fictional mux's
electrical or timing values.

- Zachtronics, **SHENZHEN I/O**: <https://www.zachtronics.com/shenzhen-io/>
- CTAN, **latexmk** package description: <https://ctan.org/pkg/latexmk>
- Pillow, **Image module** (rotation, resizing and image composition):
  <https://pillow.readthedocs.io/en/stable/reference/Image.html>
- pypdfium2, **Python API** (PDF rendering):
  <https://pypdfium2.readthedocs.io/en/stable/python_api.html>

## Asset provenance

Paper noise, stains, dust and handling marks are generated locally from deterministic numerical
recipes. No stock textures, manual scans, trademark artwork or external logo images are needed.
Typography references normal installed TeX font packages. Standalone font files are not included.

Starting with issue P2, supplier PDF metadata stays in-world. The source package and render
manifests retain provenance separately. Cosmetic stamps do not assert approval, compliance or
authenticity. No commercial-name clearance has been performed; the vendor lineup is a set of
teaching-design proposals, not a claim of available trademarks.

## Engine 0.2.0 additions

The reusable renderer, schemas, effects, tests and copy-generation proofs were authored as an
extension of the user's existing `Blender8-Document-Foundry-v0.1.0.zip`. The existing six vendor
style files and clean content were retained. The clean mux was recompiled and its bytes matched the
original master.

Additional primary references, consulted 2026-10-06:

- Pillow affine transform direction and coefficients:
  <https://pillow.readthedocs.io/en/stable/reference/ImageTransform.html>
- Pydantic models, extra-field rejection and nested validation:
  <https://docs.pydantic.dev/latest/concepts/models/>
- NumPy random-stream compatibility limits:
  <https://numpy.org/doc/stable/reference/random/compatibility.html>

These describe implementation libraries, not fictional hardware specifications. The package pins and
reports its tested versions rather than assuming that a newer documentation release was exercised.

## Engine 0.3.0 / publication P2

Extended the previously supplied foundry 0.2.0 into seven full manuals: MCU, LCD, mux, button
assembly, motor, temperature sensor and system integration. All eight current source extracts are
listed with SHA-256 hashes in `spec/manual-source-hashes.json`. MCU register tables are generated
from the checked snapshot. The original engineering archive remains unchanged.

Coffee rings/droplets, tears and their shadow/fiber edges are procedural numerical assets. Six
original project vendor styles are retained; content edits remove out-of-world notices from
participant-facing publications. No outside textures, stock stains, existing commercial manual
designs or fonts are redistributed. This release uses no new external technical claims or research
sources.

## Engine 0.4.0 / publication P3

Revised the saved 0.3.0 source archive following the user's six-document ownership correction. The
prior P2 binder and hash-checked engineering extracts were audited directly. Requirements, firmware
policy and specific chassis wiring now reside in an in-universe boss email with retained
attachments. The supplied component behaviors are unchanged. Motor and AVT10 papers retain their
authorship in one handout, making six documents totaling 30 pages.

The AVT10 remains a powered linear voltage-output sensor; no thermocouple interface is introduced.
The B8 remains the controller and BA-8 the button assembly. Mara Ellis and the Kestrel
example-domain email are authored correspondence for this packet; no actual email was sent. No
outside technical research, artwork or new component rating is used for this revision. The previous
board source survives in `owner/legacy/` for traceability, not as an additional handout.
