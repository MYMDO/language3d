// Null backend for the Language3D thin Platform API.
//
// Headless, dependency-free (C++ standard library only): used by host unit
// tests and any headless tooling. Fixed 240x240 framebuffer contract, FIFO
// event queue with scripted injection (see platform_null.h).
#include "l3d_platform.h"
#include "platform_null.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr size_t NULL_QUEUE = 32;

struct NullBackend {
    bool ready{false};
    l3d_event_t queue[NULL_QUEUE]{};
    size_t head{0};
    size_t tail{0};
    size_t count{0};
    uint8_t held[L3D_KEY_COUNT]{};
    uint32_t ticks{0};
    int fullscreen{0};
    int relative_mouse{0};
    int cursor{1};
    uint8_t last_frame[240 * 240]{};
    uint16_t last_palette[256]{};
    uint32_t presents{0};
};

NullBackend g_null{};

} // namespace

extern "C" int l3d_pf_init(const char* /*title*/) {
    g_null = NullBackend{};
    g_null.ready = true;
    return 1;
}

extern "C" void l3d_pf_shutdown(void) { g_null.ready = false; }

extern "C" uint16_t l3d_pf_display_width(void) { return 240; }
extern "C" uint16_t l3d_pf_display_height(void) { return 240; }

extern "C" void l3d_pf_display_size(uint16_t* w, uint16_t* h) {
    if (w) *w = 240;
    if (h) *h = 240;
}

extern "C" void l3d_pf_set_fullscreen(int on) { g_null.fullscreen = on ? 1 : 0; }

extern "C" uint32_t l3d_pf_ticks_ms(void) { return g_null.ticks++; }

extern "C" int l3d_pf_poll(l3d_event_t* out) {
    if (!out || g_null.count == 0) return 0;
    *out = g_null.queue[g_null.head];
    g_null.head = (g_null.head + 1) % NULL_QUEUE;
    --g_null.count;
    return 1;
}

extern "C" int l3d_pf_key_down(uint8_t key) {
    if (key >= L3D_KEY_COUNT) return 0;
    return g_null.held[key] ? 1 : 0;
}

extern "C" void l3d_pf_set_relative_mouse(int on) {
    g_null.relative_mouse = on ? 1 : 0;
}

extern "C" void l3d_pf_show_cursor(int show) { g_null.cursor = show ? 1 : 0; }

extern "C" int l3d_pf_present_indexed8(const uint8_t* pixels, uint16_t width,
                                       uint16_t height, uint32_t stride,
                                       const uint16_t* palette_rgb565) {
    if (!g_null.ready || !pixels || !palette_rgb565) return 0;
    if (width != 240 || height != 240 || stride < 240) return 0;
    for (uint16_t y = 0; y < height; ++y)
        std::memcpy(&g_null.last_frame[size_t(y) * 240],
                    pixels + size_t(y) * stride, 240);
    std::memcpy(g_null.last_palette, palette_rgb565, sizeof(g_null.last_palette));
    ++g_null.presents;
    return 1;
}

// ---- test/support hooks (l3d_platform_null.h) ----

int l3d_pf_null_push(const l3d_event_t* ev) {
    if (!ev || g_null.count >= NULL_QUEUE) return 0;
    g_null.queue[g_null.tail] = *ev;
    g_null.tail = (g_null.tail + 1) % NULL_QUEUE;
    ++g_null.count;
    if ((ev->type == L3D_EVENT_KEY_DOWN || ev->type == L3D_EVENT_KEY_UP) &&
        ev->key < L3D_KEY_COUNT)
        g_null.held[ev->key] = ev->pressed ? uint8_t(1) : uint8_t(0);
    return 1;
}

uint32_t l3d_pf_null_presents(void) { return g_null.presents; }

const uint8_t* l3d_pf_null_last_frame(void) { return g_null.last_frame; }

int l3d_pf_null_fullscreen(void) { return g_null.fullscreen; }
