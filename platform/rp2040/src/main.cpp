#include "api.h"
#include "assets.h"
#include "renderer.h"
#include "rp2040_display.h"
#include <pico/stdlib.h>
#include <cstdio>
#include <cstdint>

namespace {
alignas(16) static uint8_t g_engine[16 * 1024];
static uint8_t g_framebuffer[240 * 240];
static uint16_t g_palette[256];

constexpr uint P_W = 2;
constexpr uint P_A = 3;
constexpr uint P_S = 4;
constexpr uint P_D = 5;
constexpr uint P_E = 6;
constexpr uint P_L = 7;
constexpr uint P_P = 8;
constexpr uint P_H = 9;
}

static bool button_down(uint pin) { return gpio_get(pin) == 0; }
static void init_button(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

int main() {
    stdio_init_all();
    sleep_ms(1200);

    for (const uint pin : {P_W, P_A, P_S, P_D, P_E, P_L, P_P, P_H}) init_button(pin);

    if (!rp2040_display_init()) {
        printf("[ERR] ST7789 init failed\n");
        return 1;
    }

    if (!l3d_engine_init(g_engine, sizeof(g_engine))) {
        printf("[ERR] engine init failed\n");
        return 2;
    }

    const l3d::AssetPackView assets = l3d::generated_assets();
    if (!assets.valid()) {
        printf("[ERR] generated asset pack invalid\n");
        return 3;
    }

    if (!l3d_engine_set_assets(g_engine, assets.data, assets.size)) {
        printf("[ERR] set assets failed\n");
        return 4;
    }

    l3d::Palette palette;
    palette.make_default();
    for (size_t i = 0; i < 256; ++i) g_palette[i] = palette.rgb565[i];

    // Hardware sanity pattern sent through the exact same game display backend.
    // This separates panel/transport bring-up from renderer output.
    l3d_framebuffer8 out{g_framebuffer, 240, 240, 240};
    for (size_t i = 0; i < sizeof(g_framebuffer); ++i) {
        const size_t x = i % 240u;
        const size_t y = i / 240u;
        g_framebuffer[i] = (x / 30u + y / 30u) & 1u ? 1u : 0u;
    }
    static uint16_t probe_palette[256]{};
    probe_palette[0] = 0xF800; // red
    probe_palette[1] = 0x001F; // blue
    if (!rp2040_display_present_indexed8(out.pixels, out.width, out.height, out.stride, probe_palette)) {
        printf("[ERR] ST7789 startup probe transfer failed\n");
        return 5;
    }
    sleep_ms(800);
    l3d_input held{};
    bool prev_e = false;
    bool prev_l = false;
    bool prev_p = false;
    bool prev_h = false;

    uint32_t last_ms = to_ms_since_boot(get_absolute_time());
    uint32_t stat_ms = last_ms;
    uint32_t frames = 0;

    printf("[ARCH] profile=RP2040 | display=ST7789 240x240 SPI | SPI=MODE0/40MHz/MSB | CS=GP15 | framebuffer=indexed8 single | SRAM=264 KiB\n");
    printf("[ASSET] map=%ux%u | textures=%u | bytes=%u\n",
           assets.map_width(), assets.map_height(), assets.texture_count(), (unsigned)assets.size);

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        uint32_t dt_ms = now_ms - last_ms;
        last_ms = now_ms;
        if (dt_ms == 0) dt_ms = 1;
        if (dt_ms > 50) dt_ms = 50;

        held.up = button_down(P_W);
        held.strafe_left = button_down(P_A);
        held.down = button_down(P_S);
        held.strafe_right = button_down(P_D);

        const bool e = button_down(P_E);
        const bool l = button_down(P_L);
        const bool p = button_down(P_P);
        const bool h = button_down(P_H);
        held.interact_pressed = e && !prev_e;
        held.language_pressed = l && !prev_l;
        held.progress_pressed = p && !prev_p;
        held.help_pressed = h && !prev_h;
        prev_e = e; prev_l = l; prev_p = p; prev_h = h;

        l3d_engine_update_ms(g_engine, &held, dt_ms);
        if (!l3d_engine_render(g_engine, &out)) {
            printf("[ERR] engine render failed\n");
            break;
        }
        if (!rp2040_display_present_indexed8(out.pixels, out.width, out.height, out.stride, g_palette)) {
            printf("[ERR] display transfer failed\n");
            break;
        }

        ++frames;
        if (now_ms - stat_ms >= 1000) {
            printf("[STATS] FPS=%u | GAME-SRAM-RESERVED=%u KiB | FB8=%u KiB | ASSET=%u bytes\n",
                   static_cast<unsigned>(frames),
                   static_cast<unsigned>(l3d_reserved_sram_bytes() / 1024u),
                   (unsigned)(l3d_framebuffer_bytes() / 1024u),
                   (unsigned)assets.size);
            frames = 0;
            stat_ms = now_ms;
        }
        tight_loop_contents();
    }

    rp2040_display_shutdown();
    return 0;
}
