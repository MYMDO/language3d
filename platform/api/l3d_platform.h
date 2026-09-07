#pragma once
// Language3D thin Platform API (brief Phase 1).
//
// This is the ONLY boundary between GameCore/game loops and the outside
// world. It abstracts exactly what the game needs today — nothing more:
// lifecycle, monotonic time, input events + key snapshot, mouse mode,
// display size/fullscreen, and indexed8 framebuffer presentation.
//
// Rules:
// - Pure C interface (usable from C and C++), fixed-size integer types.
// - No heap requirement on the caller; backends use static storage.
// - One backend is linked per binary (SDL2 / RP2040 native / null).
// - GameCore (engine/) must never include this header: the dependency
//   direction is platform -> core. Only platform mains and tests use it.
// - Single-threaded use. Audio is a reserved future extension, debugged
//   out of scope until a real need appears (see docs/architecture.md G-list).
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Physical-positional key codes, mapped by each backend to its own input
// hardware (SDL scancodes on desktop, GPIO buttons on RP2040). Only the
// keys the game actually uses are listed; extend deliberately, not eagerly.
typedef enum l3d_key {
    L3D_KEY_UNKNOWN = 0,
    L3D_KEY_W, L3D_KEY_A, L3D_KEY_S, L3D_KEY_D,
    L3D_KEY_Q, L3D_KEY_C,
    L3D_KEY_E, L3D_KEY_L, L3D_KEY_P, L3D_KEY_H,
    L3D_KEY_LEFT, L3D_KEY_RIGHT, L3D_KEY_UP, L3D_KEY_DOWN,
    L3D_KEY_ESCAPE,
    L3D_KEY_1, L3D_KEY_2, L3D_KEY_3,
    L3D_KEY_F3, L3D_KEY_F11,
    L3D_KEY_COUNT
} l3d_key_t;

typedef enum l3d_event_type {
    L3D_EVENT_NONE = 0,
    L3D_EVENT_QUIT,
    L3D_EVENT_KEY_DOWN,
    L3D_EVENT_KEY_UP,
    L3D_EVENT_MOUSE_MOTION, // dx/dy: relative pixels, clamped by backend
    L3D_EVENT_MOUSE_BUTTON, // key field carries L3D_MOUSE_* button id
    L3D_EVENT_FOCUS_LOST    // backend lost focus: caller should clear latches
} l3d_event_type_t;

#define L3D_MOUSE_LEFT 1

typedef struct l3d_event {
    uint8_t type;   // l3d_event_type_t
    uint8_t key;    // l3d_key_t for key events; button id for mouse button
    uint8_t pressed; // key/button edge state (1 = down/pressed)
    uint8_t repeat;  // 1 = auto-repeat (caller typically ignores for one-shots)
    int16_t dx;      // mouse motion X
    int16_t dy;      // mouse motion Y
} l3d_event_t;

// ---- lifecycle ------------------------------------------------------------
int l3d_pf_init(const char* title); // 1 = ok
void l3d_pf_shutdown(void);

// ---- display --------------------------------------------------------------
// Native framebuffer resolution of the backend (game render target size).
uint16_t l3d_pf_display_width(void);
uint16_t l3d_pf_display_height(void);
// Current drawable/window size in pixels (letterboxing math input).
void l3d_pf_display_size(uint16_t* w, uint16_t* h);
void l3d_pf_set_fullscreen(int on); // no-op where unsupported

// ---- time -----------------------------------------------------------------
uint32_t l3d_pf_ticks_ms(void); // monotonic milliseconds

// ---- input ----------------------------------------------------------------
// Drain one queued event; returns 0 when the queue is empty.
int l3d_pf_poll(l3d_event_t* out);
// Level snapshot (recovery path for backends that may drop edges).
int l3d_pf_key_down(uint8_t key); // l3d_key_t; 1 = held
// Relative mouse-look mode + cursor visibility (no-ops where unsupported).
void l3d_pf_set_relative_mouse(int on);
void l3d_pf_show_cursor(int show);

// ---- video ----------------------------------------------------------------
// Present an indexed8 frame through a 256-entry RGB565 palette.
// Dimensions must match l3d_pf_display_width/height; stride >= width.
// Returns 1 on success, 0 on failure (caller should stop the loop).
int l3d_pf_present_indexed8(const uint8_t* pixels, uint16_t width,
                            uint16_t height, uint32_t stride,
                            const uint16_t* palette_rgb565);

#ifdef __cplusplus
}
#endif
