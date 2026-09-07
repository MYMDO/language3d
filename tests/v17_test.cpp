#include "api.h"
#include "assets.h"
#include "world_grid.h"
#include "renderer.h"
#include "test_common.h"
#include <cstdio>
#include <vector>

int main() {
    using namespace l3d;
    const AssetPackView p = builtin_assets();
    WorldGridView g{};
    L3D_REQUIRE(g.init(p));
    L3D_REQUIRE(g.valid());
    L3D_REQUIRE(g.chunks_x == 2 && g.chunks_y == 2);
    L3D_REQUIRE(g.cell(-1, 0) == 1);
    L3D_REQUIRE(g.cell(0, 0) != 0);
    L3D_REQUIRE(g.chunk_index(0, 0) == 0);
    L3D_REQUIRE(g.chunk_index(16, 16) == 3);

    const RendererCapabilities caps = software_capabilities();
    L3D_REQUIRE(caps.indexed8 && caps.sprites && caps.textured_walls && !caps.gpu_acceleration);
    static_assert(sizeof(RendererCapabilities) <= 16, "capability contract should remain tiny");

    std::vector<unsigned char> mem(l3d_engine_size() + 32);
    void* aligned = mem.data();
    while (reinterpret_cast<std::uintptr_t>(aligned) % l3d_engine_align()) ++reinterpret_cast<unsigned char*&>(aligned);
    L3D_REQUIRE(l3d_engine_init(aligned, mem.size() - (static_cast<unsigned char*>(aligned) - mem.data())));

    std::vector<u8> fb(MemoryBudget::FRAMEBUFFER8);
    l3d_framebuffer8 out{fb.data(), Config::WIDTH, Config::HEIGHT, Config::WIDTH};
    l3d_input in{};
    l3d_engine_update_ms(aligned, &in, 16);
    L3D_REQUIRE(l3d_engine_render(aligned, &out));

    std::printf("v17 world/capability test OK chunks=%u gpu=%d framebuffer=%zu\n",
                unsigned(g.chunks_x * g.chunks_y), caps.gpu_acceleration ? 1 : 0, fb.size());
    return 0;
}
