#ifndef B8_VIEW_API_H
#define B8_VIEW_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Host-only scene ABI 1. Create the machine with b8_wasm_init before view_init.
uint32_t b8_view_abi(void);
int b8_view_init(void);
int b8_view_resize(uint32_t width,uint32_t height);
int b8_view_frame(double elapsed_ms);
// 0 pointer-down, 1 up, 2 move, 3 cancel; 4 ASCII key-down, 5 key-up; 6 release+pause.
int b8_view_event(uint32_t kind,int32_t identifier,double x,double y);
const uint8_t* b8_view_pixels(void);
uint32_t b8_view_width(void);
uint32_t b8_view_height(void);
const char* b8_view_status(void);
const char* b8_view_journal(void);
#ifdef __cplusplus
}
#endif
#endif
