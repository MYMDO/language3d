// Platform API contract tests (null backend): lifecycle, display caps,
// present validation, FIFO event queue, key snapshot, launch smoke.
#include "l3d_platform.h"
#include "platform_null.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

static l3d_event key_ev(uint8_t type, uint8_t key, uint8_t pressed) {
    l3d_event e{};
    e.type = type;
    e.key = key;
    e.pressed = pressed;
    return e;
}

int main() {
    L3D_REQUIRE(l3d_pf_init("test"));
    L3D_REQUIRE(l3d_pf_display_width() == 240);
    L3D_REQUIRE(l3d_pf_display_height() == 240);
    uint16_t w = 0, h = 0;
    l3d_pf_display_size(&w, &h);
    L3D_REQUIRE(w == 240 && h == 240);

    // Ticks are monotonic.
    const uint32_t t0 = l3d_pf_ticks_ms();
    const uint32_t t1 = l3d_pf_ticks_ms();
    L3D_REQUIRE(t1 >= t0);

    // Empty queue drains as false; null out is rejected.
    l3d_event e{};
    L3D_REQUIRE(l3d_pf_poll(&e) == 0);
    L3D_REQUIRE(l3d_pf_poll(nullptr) == 0);

    // FIFO order across mixed event types.
    const l3d_event kd = key_ev(L3D_EVENT_KEY_DOWN, L3D_KEY_W, 1);
    l3d_event mm{};
    mm.type = L3D_EVENT_MOUSE_MOTION;
    mm.dx = 10;
    mm.dy = -4;
    l3d_event mb{};
    mb.type = L3D_EVENT_MOUSE_BUTTON;
    mb.key = L3D_MOUSE_LEFT;
    mb.pressed = 1;
    L3D_REQUIRE(l3d_pf_null_push(&kd) == 1);
    L3D_REQUIRE(l3d_pf_null_push(&mm) == 1);
    L3D_REQUIRE(l3d_pf_null_push(&mb) == 1);
    // Key snapshot follows pushed edges.
    L3D_REQUIRE(l3d_pf_key_down(L3D_KEY_W) == 1);
    L3D_REQUIRE(l3d_pf_key_down(L3D_KEY_S) == 0);
    L3D_REQUIRE(l3d_pf_key_down(L3D_KEY_COUNT) == 0);

    L3D_REQUIRE(l3d_pf_poll(&e) == 1 && e.type == L3D_EVENT_KEY_DOWN && e.key == L3D_KEY_W);
    L3D_REQUIRE(l3d_pf_poll(&e) == 1 && e.type == L3D_EVENT_MOUSE_MOTION && e.dx == 10 && e.dy == -4);
    L3D_REQUIRE(l3d_pf_poll(&e) == 1 && e.type == L3D_EVENT_MOUSE_BUTTON && e.key == L3D_MOUSE_LEFT);
    L3D_REQUIRE(l3d_pf_poll(&e) == 0);

    const l3d_event ku = key_ev(L3D_EVENT_KEY_UP, L3D_KEY_W, 0);
    L3D_REQUIRE(l3d_pf_null_push(&ku) == 1);
    L3D_REQUIRE(l3d_pf_key_down(L3D_KEY_W) == 0);
    L3D_REQUIRE(l3d_pf_poll(&e) == 1 && e.type == L3D_EVENT_KEY_UP);

    // Fullscreen/mouse modes are accepted (no-ops on null).
    l3d_pf_set_fullscreen(1);
    L3D_REQUIRE(l3d_pf_null_fullscreen() == 1);
    l3d_pf_set_relative_mouse(1);
    l3d_pf_show_cursor(0);

    // Present validation mirrors the contract: exact size required.
    static uint8_t frame[240 * 240];
    static uint16_t palette[256];
    for (size_t i = 0; i < sizeof(frame); ++i) frame[i] = uint8_t(i & 0xFFu);
    for (size_t i = 0; i < 256; ++i) palette[i] = uint16_t(i);
    L3D_REQUIRE(l3d_pf_present_indexed8(nullptr, 240, 240, 240, palette) == 0);
    L3D_REQUIRE(l3d_pf_present_indexed8(frame, 240, 240, 240, nullptr) == 0);
    L3D_REQUIRE(l3d_pf_present_indexed8(frame, 320, 240, 320, palette) == 0);
    L3D_REQUIRE(l3d_pf_present_indexed8(frame, 240, 240, 200, palette) == 0);
    L3D_REQUIRE(l3d_pf_present_indexed8(frame, 240, 240, 240, palette) == 1);
    L3D_REQUIRE(l3d_pf_null_presents() == 1);
    L3D_REQUIRE(std::memcmp(l3d_pf_null_last_frame(), frame, sizeof(frame)) == 0);

    l3d_pf_shutdown();
    std::printf("platform OK\n");
    return 0;
}
