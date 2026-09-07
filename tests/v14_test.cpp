#include "api.h"
#include "renderer.h"
#include "game.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <vector>

int main(){
    L3D_REQUIRE(l3d_engine_size() >= sizeof(std::uint16_t) * l3d_width());
    std::vector<std::uint8_t> storage(l3d_engine_size() + l3d_engine_align());
    void* p = storage.data();
    std::uintptr_t u = reinterpret_cast<std::uintptr_t>(p);
    const std::size_t a = l3d_engine_align();
    if (u % a) p = storage.data() + (a - (u % a));
    L3D_REQUIRE(l3d_engine_init(p, l3d_engine_size()));
    std::vector<std::uint8_t> pixels(l3d_framebuffer_bytes());
    l3d_framebuffer8 fb{pixels.data(), l3d_width(), l3d_height(), l3d_width()};
    l3d_input in{};
    l3d_engine_update_ms(p, &in, 16);
    L3D_REQUIRE(l3d_engine_render(p, &fb));
    L3D_REQUIRE(l3d_render_stats_size() == sizeof(l3d::RenderStats));
    l3d::RenderStats st{};
    L3D_REQUIRE(l3d_get_render_stats(p, &st, sizeof(st)));
    L3D_REQUIRE(st.frame_id >= 1);
    L3D_REQUIRE(st.rays == l3d_width());
    L3D_REQUIRE(st.wall_pixels > 0);
    std::printf("v14 telemetry test OK frame=%u rays=%u dda=%u walls=%u sprites=%u sprite_pixels=%u floor=%u\n",
        st.frame_id, st.rays, st.dda_steps, st.wall_pixels, st.sprites_drawn, st.sprite_pixels, st.floor_pixels);
    return 0;
}
