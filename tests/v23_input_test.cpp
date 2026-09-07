#include "../engine/game.h"
#include "../engine/assets.h"
#include "test_common.h"
#include <cstdio>

int main() {
    l3d::Game g;
    g.set_assets(l3d::builtin_assets());
    g.reset();
    const auto p0 = g.player().pos;

    l3d::InputState strafe{};
    strafe.strafe_right = true;
    g.update(strafe, 100);
    const auto p1 = g.player().pos;
    L3D_REQUIRE(p1.x.raw != p0.x.raw || p1.y.raw != p0.y.raw);

    g.reset();
    const auto a0 = g.player().angle_turn;
    l3d::InputState mouse{};
    mouse.mouse_x = 10;
    g.update(mouse, 16);
    L3D_REQUIRE(g.player().angle_turn != a0);

    std::puts("v23 input model test OK");
}
