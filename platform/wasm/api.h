#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Host bridge ABI 1. This is NOT the firmware-facing B8 peripheral interface.
uint32_t b8_wasm_abi(void);
// One session per module, modes 0=selected firmware, 1=chassis-04 bench, 2=legacy-02 bench (regression only).
// Call init once on a freshly instantiated module. Returns 0 on success.
int b8_wasm_init(uint32_t mode);
// JSON text is module-owned, copied by the caller before the next bridge call.
const char* b8_wasm_hello(void);
const char* b8_wasm_command(const char* utf8, uint32_t length);
// Inhibits simulated drive and releases the session. Restart uses a NEW module.
void b8_wasm_dispose(void);
#ifdef __cplusplus
}
#endif
