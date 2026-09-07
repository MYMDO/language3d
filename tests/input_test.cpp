#include "../engine/game.h"
#include "../engine/assets.h"
#include "test_common.h"
#include <cstdio>

int main() {
    l3d::Game g;
    g.set_assets(l3d::builtin_assets());
    g.reset();

    // The desktop bug was passing ~0.016f to an integer-millisecond API, producing 0.
    // Verify one real 16 ms simulation tick moves and turns the player.
    const auto p0 = g.player().pos;
    l3d::InputState w{}; w.up=true; g.update(w,16);
    const auto p1=g.player().pos;
    L3D_REQUIRE(p1.x.raw!=p0.x.raw || p1.y.raw!=p0.y.raw);

    const auto a0 = g.player().angle_turn;
    l3d::InputState d{}; d.right=true; g.update(d,16);
    L3D_REQUIRE(g.player().angle_turn!=a0);

    // Turning should be responsive: 60 deterministic 16/17 ms ticks
    // should rotate by ~12000 turn units (~66 degrees).
    g.reset();
    l3d::InputState turn{}; turn.right=true;
    unsigned turn_ms = 0;
    for(int i=0;i<60;++i){
        const l3d::u32 dt = (i % 3 == 0) ? 16u : 17u;
        turn_ms += dt;
        g.update(turn, dt);
    }
    const unsigned turned = g.player().angle_turn;
    L3D_REQUIRE(turn_ms == 1000u);
    L3D_REQUIRE(turned > 11500u && turned < 12500u);

    // Exact deterministic 60 Hz clock: 20×16ms + 40×17ms = 1000ms.
    l3d::SimulationClock clock{};
    unsigned total=0, ticks=0;
    while (ticks < 60) {
        const auto dt = clock.next_tick_ms();
        total += dt;
        ++ticks;
        // Advance through a synthetic frame containing exactly this tick.
        clock.advance(dt, [](l3d::u8){});
    }
    L3D_REQUIRE(total == 1000);

    std::puts("input/clock test OK");
}
