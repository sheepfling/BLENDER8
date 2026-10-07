# Acceptance scenario specifications

`acceptance_cases.json` contains 38 customer-level firmware scenarios with requirement traceability.
These are **specified test cases**, not proof that student firmware implements them and not a CLI
format already supported by the bench executable.

A later acceptance driver should run `firmware::step()` continuously between timestamped fixture
events, with event processing at defined SDK/tick boundaries; observe motor-enable/PWM wires and
scanned LCD pixels. Do not advance `Board` for a long interval without running the firmware under
test, and do not inspect the student's internal state as the sole pass criterion.

The supplied executable reference-model tests use direct fixture access to isolate hardware
contracts. Their passing status does not establish application acceptance. See TEST-001 and the
validation report for the exact separation.
