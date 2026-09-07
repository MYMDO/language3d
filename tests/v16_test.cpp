#include "api.h"
#include "renderer.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>

int main() {
    std::vector<std::uint8_t> storage(l3d_engine_size() + l3d_engine_align());
    void* p = storage.data();
    const std::size_t a = l3d_engine_align();
    const std::uintptr_t u = reinterpret_cast<std::uintptr_t>(p);
    if (u % a) p = storage.data() + (a - (u % a));
    L3D_REQUIRE(l3d_engine_init(p, l3d_engine_size()));

    std::vector<std::uint8_t> pixels(l3d_framebuffer_bytes());
    l3d_framebuffer8 fb{pixels.data(), l3d_width(), l3d_height(), l3d_width()};
    l3d_input in{};
    for (int i = 0; i < 8; ++i) {
        l3d_engine_update_ms(p, &in, 16);
        L3D_REQUIRE(l3d_engine_render(p, &fb));
    }
    l3d::RenderStats warm{};
    L3D_REQUIRE(l3d_get_render_stats(p, &warm, sizeof(warm)));
    L3D_REQUIRE(warm.rays == l3d_width());
    L3D_REQUIRE(warm.wall_pixels > 0);

    constexpr int FRAMES = 240;
    const auto t0 = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (int i = 0; i < FRAMES; ++i) {
        l3d_engine_update_ms(p, &in, 16);
        L3D_REQUIRE(l3d_engine_render(p, &fb));
        checksum += pixels[static_cast<std::size_t>((i * 97) % pixels.size())];
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    l3d::RenderStats st{};
    L3D_REQUIRE(l3d_get_render_stats(p, &st, sizeof(st)));
    std::printf("v16 hotpath benchmark OK frames=%d elapsed_ms=%.3f avg_ms=%.4f dda=%u wall=%u checksum=%llu\n",
                FRAMES, ms, ms / FRAMES, st.dda_steps, st.wall_pixels,
                static_cast<unsigned long long>(checksum));
    return 0;
}
