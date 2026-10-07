# Validation — clock/reset/power release

B8 interface 03; reference platform 0.3.0; foundry 0.5.0 / P4.

**108 CTest checks passed under GCC and 108 under Clang.** These are the same suite on two
compilers, not 216 independent test cases. Of the 108 checks, 60 preserve the preceding
component/boundary/starter baseline and 48 cover clock, execution supervision, reset and qualified
power. The legacy component cases explicitly select legacy02. Public headers also compile
separately.

**177 Python tests passed, zero skipped; Python compileall passed.** Ruff and Pyright were not
installed and were not run. PDF compilation rejected overfull boxes and unexpected page counts.
Eight source-paper pairs passed source/output/hash, page-geometry and no-hidden-text checks. Seven
handouts and a 40-page binder have the expected source grouping and seven top-level bookmarks. The
field binder is image-only; clean sources remain searchable.

Both editions were rendered (80 page images). Field contact sheets were reviewed across all pages;
detailed review covered the new clock-register, DMT, power/reset and oscillator sheets. This is
visual/editorial QA, not proof of every engineering statement or OCR resistance.

The complete two-page Meridion oscillator field PDF replayed byte-for-byte with no engine/toolchain
override. This is one full-document replay; other papers received pair/hash/geometry checks, not
independent full-size replay on another machine.

The default deterministic bench released B8 at 32,979 us, qualified the external clock at 37,010 us,
and recorded an unserviced WDT reset at 237,779 us with POR+WDT cause and drive off. These are this
fixture's values, not universal hardware startup times.

The firmware starter intentionally does not implement the clock plan, WDT/DMT health policy or
product recovery. No claim that R-001..R-040 pass in completed application firmware is made. No
target hardware, physical fuse rating/coordination, PCB, actual crystal signal integrity or HIL
acceptance was tested. This native cooperative runtime cannot recover an arbitrary host loop that
never yields; host nontermination is a test failure, not a simulated device reset.

See release-validation.json and the included compiler/test logs for recorded evidence.
