# Fixed safe-starter fixture

These files pin the incomplete starter used by platform self-tests. The learner edits
`platform/firmware/`; CMake builds this copy for `firmware.safe_starter` and the fixed
protocol fixtures. Do not sync learner edits into this directory. Change it only when
the platform intentionally revises the baseline safe-starter contract and its tests.
