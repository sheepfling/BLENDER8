# Authoring and revision rules

1. Identify the existing behavioral document and archive its unmodified text. Keep the behavior
   revision separate from the presentation issue.
2. Define the vendor's house style in `latex/vendors/`; do not copy/paste the whole common style
   into each component.
3. Put repeated identifiers, truth-table inputs and key constants in the validated spec. Generate
   TeX fragments; do not hand-edit `generated/`.
4. Write a real component document: function, signal meanings, applicable conditions, timing,
   limitations and document control. Do not invent commercial electrical ratings to make it look
   authentic.
5. Keep board-specific wiring out of generic vendor sheets. Use Attachment B of the Kestrel
   requirements email for those connections, not the button-assembly contract.
6. Build the clean edition and review every page, including units, negative signs, bit numbers,
   truth-table ordering and diagrams. A passing compiler is not a technical review.
7. Build a field copy with a recorded recipe and seed. Inspect the smallest important text and all
   diagrams after degradation. When legibility is doubtful, reduce wear; never make the reader infer
   the intended value.
8. Release the source, clean PDF, field PDF and manifest in the owner release set. The student
   distribution is a deliberate subset, not the whole instructor archive.

## Existing baseline vs. this edition

MX8-1 remains eight digital inputs, three binary selects, one output and a two-microsecond settling
interval that restarts on a relevant transition. Active-low meaning comes from the button wiring.
The output is not a high-level button index. STOP remains outside the mux on the Blender-8 board.

The mux symbol is a **logical connection drawing**. Its positions are not pin numbers. The core
design did not specify a purchasable package, supply range or absolute maximum ratings; none has
been invented here. A hardware adapter needs a real electrical design, not fictional values inferred
from the page's vendor styling.

The timing diagrams use declared initial conditions. They do not turn the reference implementation's
startup behavior into an additional undocumented guarantee.

## Adding the next component

Add the source excerpt and its provenance, extend the spec model only as needed, author
`latex/<component>/<part>.tex`, add a catalog entry and expected page count, and reuse the
corresponding vendor package. Build it with `b8-docs build --doc <id> --preset <recipe>`.

The `generate` command retains the MX8-1 spec generator. Component compilation additionally runs
hash-checked generation of the B8 register tables. The remaining manuals use reviewed LaTeX prose
and figures; adding a TOML entry alone does not author or verify them. The `packet` command
assembles seven independent source papers into exactly six handouts. The requirements email leads
the packet; the motor and sensor papers share one handout. Run `b8-docs audit --json` before
publication.

## Revisions and challenge material

Do not create contradictory old/new spec copies merely for atmosphere. A true revision exercise can
be valuable later, but needs a controlled obsolete revision, a clear errata note, a current
resolution and test expectations tied to the effective revision. Cosmetic coffee is independent of
the assignment's technical difficulty.

## Participant-facing language

Supplier PDFs use company names, part identifiers, interface revision, issue number, technical notes
and factual interface limits. Do not add teaching/fictional/hypothetical labels, grading notes,
instructor APIs or meta-commentary to these pages or their metadata. Keep provenance,
system-fidelity limitations and implementation history in this source package. This separation does
not authorize inventing physical ratings or falsifying external certifications.

## Authority of a statement

Ask whether the statement remains true after installing the component in unrelated equipment.
Register side effects, signal timing, pixel packing, mechanical latching and the absence of a
protective latch belong with the part. Speed presets, fault names, trip thresholds, firmware
response deadlines and board-pin choices belong in the product brief. Generic instructions needed to
use an interface correctly may remain in its manual.

Do not make the component perform a firmware requirement on paper when the supplied hardware does
not perform it. In particular, removing an obstruction while MD20 ENABLE/PWM remain asserted allows
motion to resume; product lockout is firmware behavior. Preserve local contact timing in BA-8, but
keep operator hold/recognition requirements in the brief.
