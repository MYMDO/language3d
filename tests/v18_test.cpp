#include "api.h"
#include "core.h"
#include "renderer.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    using namespace l3d;
    SimulationClock clock{};
    unsigned ticks = 0;
    clock.advance(1000, [&](u32 ms) { L3D_REQUIRE(ms == 16u || ms == 17u); ++ticks; });
    L3D_REQUIRE(ticks == Config::MAX_CATCHUP_TICKS);
    L3D_REQUIRE(clock.accumulator_ms < clock.next_tick_ms());

    clock.accumulator_ms = 0;
    ticks = 0;
    for (int i = 0; i < 10; ++i)
        clock.advance(16, [&](u32 ms) { L3D_REQUIRE(ms == 16u || ms == 17u); ++ticks; });
    L3D_REQUIRE(ticks == 9);
    L3D_REQUIRE(clock.accumulator_ms == 10);


    SimulationClock exact{};
    unsigned exact_ticks = 0;
    unsigned exact_elapsed = 0;
    for (unsigned i = 0; i < 1000; ++i)
        exact.advance(1, [&](u32 ms) { ++exact_ticks; exact_elapsed += ms; });
    L3D_REQUIRE(exact_ticks == 60);
    L3D_REQUIRE(exact_elapsed == 1000);

    std::vector<std::uint8_t> storage(l3d_engine_size() + l3d_engine_align());
    void* p = storage.data();
    const std::size_t a = l3d_engine_align();
    const std::uintptr_t u = reinterpret_cast<std::uintptr_t>(p);
    if (u % a) p = storage.data() + (a - (u % a));
    L3D_REQUIRE(l3d_engine_init(p, l3d_engine_size()));
    std::vector<std::uint8_t> fb(l3d_framebuffer_bytes());
    l3d_framebuffer8 out{fb.data(), l3d_width(), l3d_height(), l3d_width()};
    l3d_input in{};
    for (int i = 0; i < 60; ++i) {
        l3d_engine_update_ms(p, &in, 16);
        L3D_REQUIRE(l3d_engine_render(p, &out));
    }
    l3d::RenderStats st{};
    L3D_REQUIRE(l3d_get_render_stats(p, &st, sizeof(st)));
    L3D_REQUIRE(st.rays == l3d_width());
    std::printf("v18 60hz-clock test OK ticks=%u frame_id=%u next_tick_ms=%u\n", ticks, unsigned(st.frame_id), unsigned(clock.next_tick_ms()));
    return 0;
}
