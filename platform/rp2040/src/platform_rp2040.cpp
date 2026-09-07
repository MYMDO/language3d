// RP2040 native backend for the Language3D thin Platform API.
//
// Maps the production ST7789 display path and the GPIO button panel onto
// the platform contract. The low-level driver (rp2040_display_*) is used
// as-is; only the addressing changed (backend instead of game loop).
//
// Button wiring (active-low, pull-ups) — unchanged hardware profile:
//   GP2=W(up) GP3=A(strafe L) GP4=S(down) GP5=D(strafe R)
//   GP6=E GP7=L GP8=P GP9=H (one-shot actions, delivered as key edges)
#include "l3d_platform.h"

#include "rp2040_display.h"

#include <pico/stdlib.h>

#include <cstdint>

namespace {

constexpr uint PIN_W = 2;
constexpr uint PIN_A = 3;
constexpr uint PIN_S = 4;
constexpr uint PIN_D = 5;
constexpr uint PIN_E = 6;
constexpr uint PIN_L = 7;
constexpr uint PIN_P = 8;
constexpr uint PIN_H = 9;

constexpr uint16_t DISP_W = 240;
constexpr uint16_t DISP_H = 240;

constexpr size_t RP_QUEUE = 16;

struct RpBackend {
    bool ready{false};
    l3d_event_t queue[RP_QUEUE]{};
    size_t head{0};
    size_t tail{0};
    size_t count{0};
    uint8_t level[L3D_KEY_COUNT]{}; // last sampled GPIO levels per key
};

RpBackend g_rp{};

uint pin_for_key(uint8_t key) {
    switch (key) {
        case L3D_KEY_W: return PIN_W;
        case L3D_KEY_A: return PIN_A;
        case L3D_KEY_S: return PIN_S;
        case L3D_KEY_D: return PIN_D;
        case L3D_KEY_E: return PIN_E;
        case L3D_KEY_L: return PIN_L;
        case L3D_KEY_P: return PIN_P;
        case L3D_KEY_H: return PIN_H;
        default: break;
    }
    return UINT32_MAX;
}

int button_down(uint pin) { return gpio_get(pin) == 0 ? 1 : 0; }

void push_edge(uint8_t key, int down) {
    if (g_rp.count >= RP_QUEUE) return; // drop on overflow, never block
    l3d_event_t ev{};
    ev.type = down ? L3D_EVENT_KEY_DOWN : L3D_EVENT_KEY_UP;
    ev.key = key;
    ev.pressed = down ? uint8_t(1) : uint8_t(0);
    g_rp.queue[g_rp.tail] = ev;
    g_rp.tail = (g_rp.tail + 1) % RP_QUEUE;
    ++g_rp.count;
    g_rp.level[key] = down ? uint8_t(1) : uint8_t(0);
}

void init_button(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

} // namespace

extern "C" int l3d_pf_init(const char* /*title*/) {
    if (g_rp.ready) return 1;
    stdio_init_all();
    sleep_ms(1200);
    init_button(PIN_W);
    init_button(PIN_A);
    init_button(PIN_S);
    init_button(PIN_D);
    init_button(PIN_E);
    init_button(PIN_L);
    init_button(PIN_P);
    init_button(PIN_H);
    if (!rp2040_display_init()) return 0;
    g_rp = RpBackend{};
    g_rp.ready = true;
    // Baseline levels so the first poll does not emit spurious edges.
    for (uint8_t k = 0; k < L3D_KEY_COUNT; ++k) {
        const uint pin = pin_for_key(k);
        if (pin != UINT32_MAX) g_rp.level[k] = uint8_t(button_down(pin));
    }
    return 1;
}

extern "C" void l3d_pf_shutdown(void) {
    if (!g_rp.ready) return;
    rp2040_display_shutdown();
    g_rp.ready = false;
}

extern "C" uint16_t l3d_pf_display_width(void) { return DISP_W; }
extern "C" uint16_t l3d_pf_display_height(void) { return DISP_H; }

extern "C" void l3d_pf_display_size(uint16_t* w, uint16_t* h) {
    if (w) *w = DISP_W;
    if (h) *h = DISP_H;
}

extern "C" void l3d_pf_set_fullscreen(int /*on*/) {} // no-op: fixed panel

extern "C" uint32_t l3d_pf_ticks_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

extern "C" int l3d_pf_poll(l3d_event_t* out) {
    if (!out) return 0;
    // Sample hardware on every drain: GPIO transitions become key edges.
    for (uint8_t k = 0; k < L3D_KEY_COUNT; ++k) {
        const uint pin = pin_for_key(k);
        if (pin == UINT32_MAX) continue;
        const int down = button_down(pin);
        if (down != g_rp.level[k]) push_edge(k, down);
    }
    if (g_rp.count == 0) return 0;
    *out = g_rp.queue[g_rp.head];
    g_rp.head = (g_rp.head + 1) % RP_QUEUE;
    --g_rp.count;
    return 1;
}

extern "C" int l3d_pf_key_down(uint8_t key) {
    const uint pin = pin_for_key(key);
    if (pin == UINT32_MAX) return 0;
    return button_down(pin);
}

extern "C" void l3d_pf_set_relative_mouse(int /*on*/) {} // no mouse on device
extern "C" void l3d_pf_show_cursor(int /*show*/) {}

extern "C" int l3d_pf_present_indexed8(const uint8_t* pixels, uint16_t width,
                                       uint16_t height, uint32_t stride,
                                       const uint16_t* palette_rgb565) {
    if (!g_rp.ready) return 0;
    if (!rp2040_display_present_indexed8(pixels, width, height, stride,
                                         palette_rgb565))
        return 0;
    return 1;
}
