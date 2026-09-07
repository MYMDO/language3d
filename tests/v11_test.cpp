#include "game.h"
#include <array>
#include "test_common.h"
#include <cstdio>

int main() {
    std::array<uint8_t, l3d::Config::WIDTH * l3d::Config::HEIGHT> pixels{};
    std::array<uint16_t, l3d::Config::WIDTH> depth{};
    l3d::Framebuffer8 fb{pixels.data(), l3d::Config::WIDTH, l3d::Config::HEIGHT, l3d::Config::WIDTH, depth.data()};
    l3d::Game game;
    game.reset();
    L3D_REQUIRE(game.sprites().size() == 1);
    auto& pool = game.sprites();
    L3D_REQUIRE(pool.add(l3d::SpriteEntity{l3d::Vec2{l3d::Fx::from_int(8), l3d::Fx::from_int(6)}, 1, 0, 24, 48}));
    L3D_REQUIRE(pool.size() == 2);
    l3d::SoftwareRaycaster renderer;
    game.render(renderer, fb);
    size_t nonzero = 0;
    for (uint8_t p : pixels) if (p) ++nonzero;
    L3D_REQUIRE(nonzero == pixels.size());
    bool wrote_depth = false;
    for (auto d : depth) if (d != 0xFFFFu) { wrote_depth = true; break; }
    L3D_REQUIRE(wrote_depth);
    std::printf("v11 sprite/entity test OK sprites=%zu nonzero=%zu\n", pool.size(), nonzero);
    return 0;
}
