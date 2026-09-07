// RP2040 production game loop, written against the thin Platform API.
//
// Hardware profile (unchanged): ST7789 240x240, buttons on GP2..GP9
// (active-low, pull-ups). Display init, boot probe pattern and stats output
// preserve the proven bring-up sequence; only the addressing changed
// (platform backend instead of direct GPIO/display calls).
#include "api.h"
#include "assets.h"
#include "renderer.h"
#include "l3d_platform.h"

#include <pico/stdlib.h>

#include <cstdio>
#include <cstdint>

namespace {
alignas(16) static uint8_t g_engine[16 * 1024];
static uint8_t g_framebuffer[240 * 240];
static uint16_t g_palette[256];
}

int main() {
    if (!l3d_pf_init("Language3D")) {
        std::printf("[ERR] platform init failed\n");
        return 1;
    }

    if (!l3d_engine_init(g_engine, sizeof(g_engine))) {
        std::printf("[ERR] engine init failed\n");
        return 2;
    }

    const l3d::AssetPackView assets = l3d::generated_assets();
    if (!assets.valid()) {
        std::printf("[ERR] generated asset pack invalid\n");
        return 3;
    }

    if (!l3d_engine_set_assets(g_engine, assets.data, assets.size)) {
        std::printf("[ERR] set assets failed\n");
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
    if (!l3d_pf_present_indexed8(out.pixels, out.width, out.height, out.stride, probe_palette)) {
        std::printf("[ERR] ST7789 startup probe transfer failed\n");
        return 5;
    }

    // Board bring-up delay preserved from the proven main loop: lets USB
    // stdio attach and the panel settle after the probe pattern.
    sleep_ms(800);

    l3d_input held{};
    uint32_t last_ms = l3d_pf_ticks_ms();
    uint32_t stat_ms = last_ms;
    uint32_t frames = 0;

    std::printf("[ARCH] profile=RP2040 | display=ST7789 240x240 SPI | SPI=MODE0/40MHz/MSB | CS=GP15 | framebuffer=indexed8 single | SRAM=264 KiB\n");
    std::printf("[ASSET] map=%ux%u | textures=%u | bytes=%u\n",
           assets.map_width(), assets.map_height(), assets.texture_count(), (unsigned)assets.size);

    while (true) {
        const uint32_t now_ms = l3d_pf_ticks_ms();
        uint32_t dt_ms = now_ms - last_ms;
        last_ms = now_ms;
        if (dt_ms == 0) dt_ms = 1;
        if (dt_ms > 50) dt_ms = 50;

        held.up = l3d_pf_key_down(L3D_KEY_W);
        held.strafe_left = l3d_pf_key_down(L3D_KEY_A);
        held.down = l3d_pf_key_down(L3D_KEY_S);
        held.strafe_right = l3d_pf_key_down(L3D_KEY_D);
        held.interact_pressed = false;
        held.language_pressed = false;
        held.progress_pressed = false;
        held.help_pressed = false;

        l3d_event ev{};
        while (l3d_pf_poll(&ev)) {
            if (ev.type != L3D_EVENT_KEY_DOWN || ev.repeat) continue;
            switch (ev.key) {
                case L3D_KEY_E: held.interact_pressed = true; break;
                case L3D_KEY_L: held.language_pressed = true; break;
                case L3D_KEY_P: held.progress_pressed = true; break;
                case L3D_KEY_H: held.help_pressed = true; break;
                default: break;
            }
        }

        l3d_engine_update_ms(g_engine, &held, dt_ms);
        if (!l3d_engine_render(g_engine, &out)) {
            std::printf("[ERR] engine render failed\n");
            break;
        }
        if (!l3d_pf_present_indexed8(out.pixels, out.width, out.height, out.stride, g_palette)) {
            std::printf("[ERR] display transfer failed\n");
            break;
        }

        ++frames;
        if (now_ms - stat_ms >= 1000) {
            std::printf("[STATS] FPS=%u | GAME-SRAM-RESERVED=%u KiB | FB8=%u KiB | ASSET=%u bytes\n",
                   static_cast<unsigned>(frames),
                   static_cast<unsigned>(l3d_reserved_sram_bytes() / 1024u),
                   (unsigned)(l3d_framebuffer_bytes() / 1024u),
                   (unsigned)assets.size);
            frames = 0;
            stat_ms = now_ms;
        }
    }

    l3d_pf_shutdown();
    return 0;
}
