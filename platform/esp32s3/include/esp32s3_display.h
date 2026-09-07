#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int esp32s3_display_init(void);
uint8_t* esp32s3_display_acquire_framebuffer(void);
int esp32s3_display_present(const uint8_t* pixels, uint16_t width, uint16_t height, uint32_t stride);
int esp32s3_display_is_busy(void);
void esp32s3_display_wait_idle(void);
void esp32s3_display_shutdown(void);
#ifdef __cplusplus
}
#endif
