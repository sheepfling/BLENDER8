# Platform 0.5.0: native and WebAssembly source routes

The shared C++ Session now fronts both the native JSON-lines executable and a narrow C ABI intended
for Emscripten. The workbench uses interchangeable NativeTransport and WasmTransport hosts; the
latter owns a dedicated module worker and serializes all stateful calls. Component factories,
firmware register interface 03 and chassis 04 are retained.

`tools/b8.py wasm-build`, `wasm-serve`, and `wasm-test` are the new entry points. A successful
Emscripten build produces a static site containing selected firmware, pixel probe, and fixed starter
modules. The intentionally nonreturning compiler test image is excluded from publication. Actual
compiled parity checks never use the worker unit-test doubles.

**This release contains source, not compiled Wasm. The compiler was unavailable and the actual
Emscripten/browser path remains unverified.** See `WASM.md` for reproducible local commands and
`VALIDATION-0.5.0.md` for the 192-test native results and the remaining gate.

## Compatibility and maintenance

- Same firmware headers, register addresses, pin behavior, mechanics, plant equations and current
  correspondence. No MCU manual regeneration is required.
- Existing native commands continue using the same wire protocol through the extracted Session.
- Trace sample limits now use a ceiling count so a partial last sample cannot exceed the documented
  1,000-sample limit.
- Native page reload observes current state instead of displaying the process's original handshake
  snapshot.
- Front-panel run/pause uses a generation token to prevent duplicate stepping loops after rapid
  toggles. Hidden tabs stop automatic stepping; no elapsed-wall-time catch-up is introduced.
- The native launcher explicitly resets selected firmware/sanitizer configuration when omitted,
  avoiding stale CMake selections. Native and Wasm caches are separate.
- Old validation records refer to earlier versions and do not certify the new target.
