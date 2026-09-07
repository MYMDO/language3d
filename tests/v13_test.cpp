#include "assets.h"
#include "entities.h"
#include "l3d_math.h"
#include "renderer.h"
#include "test_common.h"
#include <cstdio>
#include <vector>

int main() {
    const auto pack = l3d::builtin_assets();
    L3D_REQUIRE(pack.valid());
    L3D_REQUIRE(pack.texture_count() == 2);
    L3D_REQUIRE(pack.texture8(0) != pack.texture8(1));
    L3D_REQUIRE(l3d::texture_sample64(pack, 0, 1, 1) != l3d::texture_sample64(pack, 1, 1, 1));
    L3D_REQUIRE(l3d::material_texture(pack, 1) == 0);
    L3D_REQUIRE(l3d::material_texture(pack, 15) == 0);

    l3d::EntityPool<8> pool;
    L3D_REQUIRE(pool.add(l3d::SpriteEntity{l3d::Vec2{l3d::Fx::from_raw(400000), l3d::Fx::from_raw(400000)}, 1, 0, 32, 64}));
    L3D_REQUIRE(pool.add(l3d::SpriteEntity{l3d::Vec2{l3d::Fx::from_raw(500000), l3d::Fx::from_raw(500000)}, 1, 0, 32, 64}));
    const auto before0 = pool.data()[0].pos.x.raw;
    const auto before1 = pool.data()[1].pos.x.raw;
    std::vector<l3d::u8> pixels(l3d::Config::WIDTH*l3d::Config::HEIGHT);
    std::vector<l3d::u16> depth(l3d::Config::WIDTH, 0xFFFFu);
    l3d::Framebuffer8 fb{pixels.data(), l3d::Config::WIDTH, l3d::Config::HEIGHT, l3d::Config::WIDTH, depth.data()};
    l3d::SoftwareRaycaster r; r.set_assets(pack); r.begin(fb); r.draw_sprites(fb, l3d::Player{}, pool.data(), pool.size());
    L3D_REQUIRE(pool.data()[0].pos.x.raw == before0 && pool.data()[1].pos.x.raw == before1);
    std::printf("v13 regression OK textures=2 entity_order_preserved=1\n");
    return 0;
}
