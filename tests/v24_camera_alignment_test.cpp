#include "../engine/game.h"
#include "../engine/renderer.h"
#include "../engine/assets.h"
#include "../engine/l3d_math.h"
#include "test_common.h"
#include <array>
#include <cstdio>
#include <cstdint>

using namespace l3d;

static i32 abs32(i32 x) { return x < 0 ? -x : x; }

int main() {
    Game g;
    g.set_assets(builtin_assets());
    g.reset();
    SoftwareRaycaster r;

    // The renderer's center ray must be parallel to the simulation's forward
    // vector. A visible target exactly on that ray must therefore project to
    // the reticle center, regardless of the display width/profile.
    const u16 angles[] = {0u, 4096u, 8192u, 16384u, 24576u, 32768u, 49152u, 61440u};
    for (u16 angle : angles) {
        const Vec2 ray = r.center_ray(angle);
        const Fx fx = TrigLut::cos16(angle);
        const Fx fy = TrigLut::sin16(angle);
        const i64 cross = i64(ray.x.raw) * fy.raw - i64(ray.y.raw) * fx.raw;
        L3D_REQUIRE(abs32(static_cast<i32>(cross >> 16)) <= 256);

        Player p{};
        p.angle_turn = angle;
        const Vec2 target{
            Fx::from_raw(p.pos.x.raw + i32((i64(fx.raw) * 8))),
            Fx::from_raw(p.pos.y.raw + i32((i64(fy.raw) * 8)))
        };
        const i32 projected = r.project_world_x(p, target);
        const i32 expected = Config::WIDTH / 2;
        L3D_REQUIRE(abs32(projected - expected) <= 1);
    }

    std::printf("v24 camera alignment OK for %zu angles, center=%d\n",
                sizeof(angles) / sizeof(angles[0]), Config::WIDTH / 2);
    return 0;
}
