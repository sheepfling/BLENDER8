# Validation report — Blender-8 v0.2.0

**Date:** 2026-10-06 · **Environment:** Linux build container · **Status:** reference starter
verified; application acceptance not complete

## Executed validation

| Check                             | Result                                 |
| --------------------------------- | -------------------------------------- |
| GCC Release configure and build   | PASS                                   |
| GCC CTest                         | **60/60 PASS**                         |
| Clang Release configure and build | PASS                                   |
| Clang CTest                       | **60/60 PASS**                         |
| Standalone header compilation     | **16 headers compiled in both builds** |
| Headless bench smoke execution    | PASS                                   |
| Safe student stub smoke execution | PASS; motor RPM remained zero          |

The60 CTest entries comprise56 reference-model component cases, one safe-starter application smoke
check, and three local checks for register-map identity, firmware/SDK dependency boundaries, and
requirement/scenario/profile consistency. These are60 distinct checks run with each compiler, not120
distinct test cases.

The requirement/profile consistency check validates30 numbered requirements,38 planned acceptance
cases, register address uniqueness, reference-test inventory and selected mathematical invariants.
It does not prove every sentence of every datasheet or every source constant has been formally
verified.

## Tool versions actually used

```text
g++ (Debian 14.2.0-19) 14.2.0
clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)
cmake version 3.31.6
Python 3.13.5
```

Core simulator and student-firmware targets build with warning-as-error flags. The package has no
downloaded C++ dependency. Neither MSVC/Windows nor macOS nor an actual microcontroller/HIL adapter
was tested here.

## Observed bench output

```text
Selected channel 2 active-low read: 0
No-load RPM at code 160: 8222.8
[32 x 16 scanned pixel pattern printed]
350 ms after STOP: drive=0, coasting RPM=3025.0
Starter ran for 100000 us; motor RPM=0.
```

The bench confirms that drive-off and rotor-stop are different observations. It drives the model
directly and deliberately is not a completed application or a protection demonstration.

## Explicitly not claimed

No completed firmware protection implementation, no passing38-scenario customer-acceptance run, no
GUI/event-queue player, no hardware safety validation, no real motor parameter identification, no
instruction-set/cycle-accurate CPU emulation, and no physical HIL timing validation. See STATUS-001
for the delivered-versus-planned boundary. ASan/UBSan or exhaustive formal verification were not
part of this validation pass.
