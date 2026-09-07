// Built-in font tests: glyph sanity, clipping, text advance, table size.
#include "../engine/font.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

int main() {
    // Table covers all 96 codes without out-of-bounds reads.
    for (unsigned c = 0x20; c <= 0x7F; ++c) {
        const uint8_t* g = font_glyph(c);
        (void)g;
    }
    L3D_REQUIRE(font_glyph('?')[0] == 0x3C);
    L3D_REQUIRE(font_glyph(0x00) == font_glyph('?')); // clamped
    L3D_REQUIRE(font_glyph(0xFF) == font_glyph('?'));
    L3D_REQUIRE(font_glyph(' ')[0] == 0x00); // space empty

    // 'A' has ink, text advance is exact.
    static uint8_t fb[16 * 16]{};
    L3D_REQUIRE(draw_glyph(fb, 16, 16, 16, 0, 0, 'A', 7) > 10);
    L3D_REQUIRE(fb[0 * 16 + 3] == 7); // apex pixel of 'A' row 0 (0x18)
    const size_t n = draw_text(fb, 16, 16, 16, 0, 8, "Hi", 5);
    L3D_REQUIRE(n > 0);
    L3D_REQUIRE(text_width(2) == 16);

    // Clipping: fully and partially off-screen never writes out of bounds.
    static uint8_t clip[8 * 8]{};
    L3D_REQUIRE(draw_text(clip, 8, 8, 8, 100, 100, "Hello", 1) == 0);
    L3D_REQUIRE(draw_text(clip, 8, 8, 8, -4, 0, "A", 1) > 0);
    L3D_REQUIRE(draw_text(nullptr, 8, 8, 8, 0, 0, "A", 1) == 0);
    L3D_REQUIRE(draw_text(clip, 8, 8, 8, 0, 0, nullptr, 1) == 0);
    // Empty string and space-only draw nothing but stay safe.
    L3D_REQUIRE(draw_text(clip, 8, 8, 8, 0, 0, "", 1) == 0);
    L3D_REQUIRE(draw_text(clip, 8, 8, 8, 0, 0, " ", 1) == 0);

    std::printf("font OK\n");
    return 0;
}
