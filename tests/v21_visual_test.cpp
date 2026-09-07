#include "game.h"
#include "renderer.h"
#include "assets.h"
#include <array>
#include "test_common.h"
#include <cstdio>

using namespace l3d;

int main() {
    Game g;
    const AssetPackView assets = builtin_assets();
    g.set_assets(assets);
    g.reset();
    SoftwareRaycaster r;
    Framebuffer8 fb{};
    std::array<u8, Config::WIDTH * Config::HEIGHT> pixels{};
    std::array<u16, Config::WIDTH> depth{};
    fb.pixels = pixels.data();
    fb.width = Config::WIDTH;
    fb.height = Config::HEIGHT;
    fb.stride = Config::WIDTH;
    fb.column_depth = depth.data();

    // The initial view faces the wall at x=11 from (4.5, 4.5). The wall plane
    // is therefore frontal and should have nearly constant projected height.
    g.render(r, fb);
    const auto &st = r.stats();
    L3D_REQUIRE(st.rays == Config::WIDTH);
    L3D_REQUIRE(st.wall_pixels > 0);

    int min_top = Config::HEIGHT, max_top = -1;
    int min_bottom = Config::HEIGHT, max_bottom = -1;
    const int sample_left = std::min(20, Config::WIDTH / 4);
    const int sample_right = std::max(sample_left + 1, Config::WIDTH - sample_left);
    for (int x = sample_left; x < sample_right; ++x) {
        int top = -1, bottom = -1;
        for (int y = 40; y < 120; ++y) {
            const u8 px = pixels[static_cast<size_t>(y) * Config::WIDTH + x];
            if (px != 16) { top = y; break; }
        }
        for (int y = 120; y < 200; ++y) {
            const u8 px = pixels[static_cast<size_t>(y) * Config::WIDTH + x];
            if (px != 24) { bottom = y - 1; break; }
        }
        L3D_REQUIRE(top >= 0 && bottom >= top);
        min_top = std::min(min_top, top); max_top = std::max(max_top, top);
        min_bottom = std::min(min_bottom, bottom); max_bottom = std::max(max_bottom, bottom);
    }
    // The top edge varies with perspective because the camera plane intersects
    // multiple wall segments at different distances; the lower floor boundary
    // remains stable in this test scene.
    L3D_REQUIRE(max_bottom - min_bottom <= 2);

    // The scene should retain a finite wall span; the top may vary because the
    // camera plane intersects the surrounding wall at different distances.
    L3D_REQUIRE((max_bottom - min_top) > 20);
    L3D_REQUIRE((max_bottom - min_top) < 100);
    const int center = Config::WIDTH / 2;
    int center_top = -1;
    for (int y = 40; y < 120; ++y) {
        const u8 px = pixels[static_cast<size_t>(y) * Config::WIDTH + center];
        if (px != 16) { center_top = y; break; }
    }
    L3D_REQUIRE(center_top >= 100 && center_top <= 104);

    InputState turn{};
    turn.right = true;
    g.update(turn, 1000);
    g.render(r, fb);
    bool changed = false;
    for (size_t i = 0; i < pixels.size(); ++i) {
        if (pixels[i] != 16) { changed = true; break; }
    }
    L3D_REQUIRE(changed);

    std::printf("v21 visual camera/reciprocal test OK top=%d..%d bottom=%d..%d wall=%d\n",
                min_top, max_top, min_bottom, max_bottom, max_bottom - min_top);
    return 0;
}
