# Current implementation status

Selectable **B8 interface 03** and **B16 interface 04**, default chassis **04**.

Implemented: firmware SDK/backend, typed component factories, jar loop/latch, board
physics/peripherals, persistent cooperative runtime, timestamped fixture events, browser workbench,
JSON-lines native emulator, trace client, scenario runner, stress runner and customer-acceptance
harness. B16 adds 1 KiB SRAM, paced DMA, pin selection and a sixth interrupt. The browser can
load a matching compiled Wasm/loader pair and save an execution smoke report with its hash.
Binary32 arithmetic is compiler-backed; no instruction ISA or FPU instruction timing is assigned.
See [the device workflow](DEVICE-WORKFLOW.md). Pixel probe is a peripheral bring-up
demonstration, not a firmware solution.

A lockstep motor-link adapter is implemented and tested with a loopback. A physical HWIL
transport/scheduler is **not implemented or validated**. Finished product firmware is **not
supplied**. Acceptance reviews are explicit and an unfinished starter is rejected. CPU instruction
emulation, guard locking, braking, physical safety certification and real fuse selection remain
outside the delivered implementation.

See README.md and the new architecture/testing/interface documents for current behavior.
`CLOCK_RESET_CONTRACT.md` and `internal/history/platform/` record earlier
revisions. Their old document counts and selected algorithm prescriptions are not new acceptance
requirements.
