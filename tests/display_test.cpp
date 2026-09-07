#include "core.h"
#include "rp2350_display.h"
#include "rp2350_transport.h"
#include "test_common.h"
#include <cstdint>
#include <array>
#include <iostream>

int main() {
    L3D_REQUIRE(rp2350_display_init());
    auto* a = rp2350_display_acquire_framebuffer();
    L3D_REQUIRE(a);
    a[0] = 7;
    rp2350_indexed_framebuffer fb{a, l3d::Config::WIDTH, l3d::Config::HEIGHT, l3d::Config::WIDTH};
    std::array<uint16_t,256> palette{};
    palette[7] = 0x1234;
    std::array<uint16_t,l3d::Config::WIDTH*l3d::Config::HEIGHT> out{};
    L3D_REQUIRE(rp2350_display_convert_indexed_to_rgb565(&fb, palette.data(), out.data(), out.size()));
    L3D_REQUIRE(out[0] == 0x1234);
    L3D_REQUIRE(!rp2350_display_is_busy());
    L3D_REQUIRE(rp2350_display_present(&fb));
    auto* b = rp2350_display_acquire_framebuffer();
    L3D_REQUIRE(b && b != a);
    rp2350_display_shutdown();
    std::cout << "RP2350 display conversion/ownership test OK\n";
}
