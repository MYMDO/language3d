#include "api.h"
#include "renderer.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>

int main(){
    std::vector<std::uint8_t> storage(l3d_engine_size() + l3d_engine_align());
    void* p = storage.data();
    const std::size_t a = l3d_engine_align();
    const std::uintptr_t u = reinterpret_cast<std::uintptr_t>(p);
    if (u % a) p = storage.data() + (a - (u % a));
    L3D_REQUIRE(l3d_engine_init(p, l3d_engine_size()));

    std::vector<std::uint8_t> pixels(l3d_framebuffer_bytes());
    l3d_framebuffer8 fb{pixels.data(), l3d_width(), l3d_height(), l3d_width()};
    l3d_input in{};

    // Warm up and correctness check.
    for (int i = 0; i < 4; ++i) {
        l3d_engine_update_ms(p, &in, 16);
        L3D_REQUIRE(l3d_engine_render(p, &fb));
    }
    l3d::RenderStats st{};
    L3D_REQUIRE(l3d_get_render_stats(p, &st, sizeof(st)));
    L3D_REQUIRE(st.rays == l3d_width());
    L3D_REQUIRE(st.wall_pixels > 0);
    L3D_REQUIRE(st.floor_pixels == (l3d_height()/2u) * l3d_width());

    constexpr int FRAMES = 120;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < FRAMES; ++i) {
        l3d_engine_update_ms(p, &in, 16);
        L3D_REQUIRE(l3d_engine_render(p, &fb));
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("v15 baseline benchmark OK frames=%d elapsed_ms=%.3f avg_ms=%.4f rays=%u dda=%u\\n",
                FRAMES, ms, ms / FRAMES, st.rays, st.dda_steps);
    return 0;
}
