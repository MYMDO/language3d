#include "game.h"
#include "resource_cache.h"
#include <array>
#include "test_common.h"
#include <cstdint>
#include <cstdio>

int main() {
    std::array<uint8_t, l3d::Config::WIDTH * l3d::Config::HEIGHT> pixels{};
    l3d::Framebuffer8 fb{pixels.data(), l3d::Config::WIDTH, l3d::Config::HEIGHT, l3d::Config::WIDTH};
    std::array<uint16_t, l3d::Config::WIDTH> depth{};
    fb.column_depth = depth.data();

    l3d::Game game;
    game.reset();
    l3d::SoftwareRaycaster renderer;
    game.render(renderer, fb);

    size_t nonzero=0;
    for (auto p : pixels) if (p) ++nonzero;
    L3D_REQUIRE(nonzero == pixels.size());

    l3d::ResourceCache<2> cache;
    const uint8_t a[]{1,2,3};
    const uint8_t b[]{4,5};
    L3D_REQUIRE(cache.put(1,a,sizeof(a)));
    L3D_REQUIRE(cache.put(2,b,sizeof(b)));
    L3D_REQUIRE(cache.size() == 2);
    L3D_REQUIRE(!cache.put(3,a,sizeof(a)));
    const auto* e = cache.get(2);
    L3D_REQUIRE(e && e->size == sizeof(b) && e->data[1] == 5);

    std::printf("v10 renderer/cache test OK nonzero=%zu cache=%zu\n", nonzero, cache.size());
    return 0;
}
