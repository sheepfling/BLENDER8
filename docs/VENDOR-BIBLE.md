# Six suppliers, six documentation cultures

Design issue P1. These are fictional identities, not cleared commercial brand names. Use the
existing component identifiers; do not silently rename the interfaces while creating a
better-looking manual.

| Vendor                          | Component                                        | Layout and typography                                                                               | Voice                                                            | Default handling history                                                           |
| ------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Northstar Micro Devices         | Blender-8 MCU                                    | Blue-gray rules; formal serif body; compact sans headings; compass-star mark; dense register tables | “General description,” “register summary,” “interrupt sources”   | An older archive print in a three-ring binder; light skew and punch-hole shadows   |
| PixelRiver Display Technologies | PX32-16 LCD                                      | Teal nameplate; clean sans body; pixel-grid motif; generous timing and RAM-map diagrams             | “Module overview,” “display RAM,” “scan timing”                  | A comparatively fresh lab copy with a staple shadow                                |
| TriAxis Logic                   | MX8-1 mux                                        | Black-only Helvetica; angular three-stroke mark; compact truth tables; catalog metadata             | “Function,” “switching characteristics,” “connection discipline” | A commodity sheet copied once too often; repeated copier streak and a crooked feed |
| Kestrel Appliance Systems       | BA-8 button assembly and board integration notes | Framed drawing-style masthead; Courier body; olive-gray rules; issue/assembly blocks                | “Assembly notes,” “contact schedule,” “inspection points”        | A bench copy: warm paper, crease, staple, partial coffee ring                      |
| Vortek Motion                   | MD20 motor/driver                                | Bold italic wordmark; rotor mark; oxide-red headings; larger warning panels                         | “Operating profile,” “drive conditions,” “locked-rotor caution”  | An applications note kept beside a motor test fixture; modest shop wear            |
| ThermaSense Components          | AVT10 analog temperature sensor                  | Palatino body; plum-gray rules; two-point transfer-line mark; restrained equation layout            | “Transfer characteristics,” “response,” “application interface”  | A nearly clean, terse analog note; very light lab wear                             |

## What varies, and what stays controlled

Vary the header architecture, typography, figure priorities, document language, numbering
conventions and amount of wear. Merely changing the accent color does not create another supplier.

Keep part identifiers, behavioral revisions, units, logic polarity, bit significance, timing
guarantees and safety-related statements intact. Each component sheet stays generic; the application
company's assembly/wiring documents describe which MCU pads connect to which parts.

## Narrative details permitted now

Use cosmetic handling marks such as “BENCH COPY / RETURN,” “LAB FILE / COPY 02,” a staple shadow,
punch holes and a recurring scanner streak. These say something about the *paper*, not about the
component's correctness.

Do not add fake approvals, CE/UL marks, fabricated measurements, claimed electrical ratings,
crossed-out numeric limits, missing pages, contradictory revisions or an “OBSOLETE” stamp on the
current specification. A later errata exercise must explicitly identify the affected revision and
resolve the discrepancy before grading firmware against it.

## Seed story

One seed gives one repeatable copy of the packet. Changing a seed makes a different physical-looking
copy, not a different assignment. Device-coordinate copier streaks stay in the same place within a
recipe; sheet rotation, stain irregularity and dust vary by page. This is still an art-directed
approximation, not a measured copier model.

## Initial student handoff

The mux normally arrives as `bad_xerox`; the board extract arrives as `shop_copy`. Keep the clean
PDFs, TeX and provenance with the instructor, and provide a clean equivalent for accessibility or
legibility problems. Do not grade a student's ability to reconstruct a stained decimal point.
