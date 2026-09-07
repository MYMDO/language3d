#pragma once
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l3d_input {
    uint8_t up, down, left, right;
    uint8_t strafe_left, strafe_right;
    int16_t mouse_x;
    uint8_t interact_pressed, language_pressed;
    uint8_t progress_pressed, help_pressed, escape_pressed;
    uint8_t answer;
} l3d_input;

typedef struct l3d_framebuffer8 {
    uint8_t* pixels;
    uint16_t width;
    uint16_t height;
    uint32_t stride;
} l3d_framebuffer8;

size_t l3d_engine_size(void);
size_t l3d_engine_align(void);
int l3d_engine_init(void* memory, size_t memory_size);
void l3d_engine_reset(void* memory);
int l3d_engine_set_assets(void* memory, const void* data, size_t size);
void l3d_engine_update_ms(void* memory, const l3d_input* input, uint32_t dt_ms);
int l3d_engine_render(void* memory, l3d_framebuffer8* framebuffer);

uint16_t l3d_width(void);
uint16_t l3d_height(void);
size_t l3d_framebuffer_bytes(void);
size_t l3d_reserved_sram_bytes(void);
size_t l3d_sram_headroom_bytes(void);
size_t l3d_resource_cache_slots(void);
size_t l3d_render_stats_size(void);
int l3d_get_render_stats(void* memory, void* out_stats, size_t out_size);

#ifdef __cplusplus
}
#endif

