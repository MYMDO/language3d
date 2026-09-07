#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int rp2040_display_init(void);
uint8_t* rp2040_display_framebuffer(void);
int rp2040_display_present_indexed8(const uint8_t* pixels, uint16_t width, uint16_t height, uint32_t stride, const uint16_t* palette_rgb565);
void rp2040_display_shutdown(void);
#ifdef __cplusplus
}
#endif
