// Phase 1 (True 3D world) regression tests: transform math, height-aware
// locomotion (stairs, walls, headroom, bridges), trigger/region volumes.
#include "../engine/world3.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static i32 abs32(i32 x) { return x < 0 ? -x : x; }

// 5x3 room, border walls, open interior. Heights in 1/8 units.
static constexpr int TW = 5, TH = 3;
static int8_t g_floor[TW * TH];
static int8_t g_ceil[TW * TH];
static bool g_useCeil = false;

static void flat_room() {
    for (int i = 0; i < TW * TH; ++i) { g_floor[i] = 0; g_ceil[i] = 0; }
    g_useCeil = false;
}
static bool room_solid(i32 cx, i32 cy) {
    if (cx < 0 || cy < 0 || cx >= TW || cy >= TH) return true;
    return false;
}
static HeightField room_field() {
    HeightField f;
    f.floorQ = g_floor;
    f.ceilQ = g_useCeil ? g_ceil : nullptr;
    f.w = TW; f.h = TH;
    return f;
}

int main() {
    // --- transform math ---
    {
        const Vec3 a{ Fx::from_int(1), Fx::from_int(2), Fx::from_int(3) };
        const Vec3 b{ Fx::from_float(0.5f), Fx::from_float(0.5f), Fx::from_float(0.5f) };
        const Vec3 c = a + b;
        L3D_REQUIRE(c.x.raw == Fx::from_float(1.5f).raw);
        const Vec3 d = a - a;
        L3D_REQUIRE(d.x.raw == 0 && d.y.raw == 0 && d.z.raw == 0);
        const Vec3 e = a * Fx::from_int(2);
        L3D_REQUIRE(e.z.raw == Fx::from_int(6).raw);
    }
    {
        const Vec2 f0 = yaw_forward(0);
        L3D_REQUIRE(abs32(f0.x.raw - Fx::ONE) < 4 && abs32(f0.y.raw) < 4);
        const Vec2 f90 = yaw_forward(0x4000);
        L3D_REQUIRE(abs32(f90.x.raw) < 4 && abs32(f90.y.raw - Fx::ONE) < 4);
    }
    {
        const AABB3 box{ Vec3{}, Vec3{ Fx::from_int(2), Fx::from_int(2), Fx::from_int(2) } };
        L3D_REQUIRE(aabb_contains(box, Vec3{ Fx::from_int(1), Fx::from_int(1), Fx::from_int(1) }));
        L3D_REQUIRE(!aabb_contains(box, Vec3{ Fx::from_int(2), Fx::from_int(1), Fx::from_int(1) })); // max exclusive
        const AABB3 far{ Vec3{ Fx::from_int(5), Fx::from_int(5), Fx::from_int(5) },
                         Vec3{ Fx::from_int(6), Fx::from_int(6), Fx::from_int(6) } };
        L3D_REQUIRE(!aabb_overlaps(box, far));
        L3D_REQUIRE(aabb_overlaps(box, box));
    }

    // --- flat walk keeps feet on floor 0 ---
    {
        flat_room();
        const MoveQuery q{};
        Vec3 p{ Fx::from_float(2.0f), Fx::from_float(1.0f), Fx{} };
        walk_move(room_field(), room_solid, p, q, Vec2{ Fx::from_float(0.5f), Fx{} });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(2.5f).raw) < 4);
        L3D_REQUIRE(p.z.raw == 0);
    }

    // --- wall slide: X blocked, Y passes ---
    {
        flat_room();
        const MoveQuery q{};
        auto wall = [](i32 cx, i32 cy) {
            if (cx == 2) return true; // full-height wall column
            return room_solid(cx, cy);
        };
        Vec3 p{ Fx::from_float(1.5f), Fx::from_float(0.5f), Fx{} };
        walk_move(room_field(), wall, p, q, Vec2{ Fx::from_float(0.5f), Fx::from_float(0.5f) });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(1.5f).raw) < 4); // X denied
        L3D_REQUIRE(abs32(p.y.raw - Fx::from_float(1.0f).raw) < 4); // Y slid
    }

    // --- stairs: ramp 0, .25, .5, .75, 1.0 is climbable, feet track floor ---
    {
        for (int y = 0; y < TH; ++y)
            for (int x = 0; x < TW; ++x)
                g_floor[y * TW + x] = int8_t(x * 2); // x/4 units
        g_useCeil = false;
        const MoveQuery q{};
        Vec3 p{ Fx::from_float(0.5f), Fx::from_float(1.0f), Fx{} };
        for (int i = 0; i < 4; ++i)
            walk_move(room_field(), room_solid, p, q, Vec2{ Fx::from_float(0.5f), Fx{} });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(2.5f).raw) < 4);
        L3D_REQUIRE(p.z.raw == Fx::from_float(0.5f).raw); // cell 2 -> 4/8
        // Eye follows the feet up the stairs.
        L3D_REQUIRE(abs32(eye_z(p).raw - (p.z + World3Limits::eye_height()).raw) == 0);
    }

    // --- too-tall step (1.0 > 0.45) is denied ---
    {
        flat_room();
        g_floor[1 * TW + 2] = 8; // +1.0 unit plateau in cell (2,1)
        const MoveQuery q{};
        Vec3 p{ Fx::from_float(1.5f), Fx::from_float(1.0f), Fx{} };
        walk_move(room_field(), room_solid, p, q, Vec2{ Fx::from_float(0.5f), Fx{} });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(1.5f).raw) < 4);
        L3D_REQUIRE(p.z.raw == 0);
    }

    // --- low ceiling (1.0 clear < 1.7 body) denies entry ---
    {
        flat_room();
        g_ceil[1 * TW + 2] = 8; // ceiling at +1.0 in cell (2,1)
        g_useCeil = true;
        const MoveQuery q{};
        Vec3 p{ Fx::from_float(1.5f), Fx::from_float(1.0f), Fx{} };
        walk_move(room_field(), room_solid, p, q, Vec2{ Fx::from_float(0.5f), Fx{} });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(1.5f).raw) < 4);
        g_useCeil = false;
    }

    // --- bridge: solid cell high above the head does not collide ---
    {
        flat_room();
        auto bridge = [](i32 cx, i32 cy) {
            if (cx == 2 && cy == 1) return true; // deck at +3.0..+4.0
            return room_solid(cx, cy);
        };
        static int8_t bf[TW * TH] = {};
        static int8_t bc[TW * TH];
        for (int i = 0; i < TW * TH; ++i) bc[i] = 16; // open sky (+2.0)
        bf[1 * TW + 2] = 24; bc[1 * TW + 2] = 32;
        HeightField f{ bf, bc, TW, TH };
        const MoveQuery q{};
        Vec3 p{ Fx::from_float(1.5f), Fx::from_float(1.0f), Fx{} };
        walk_move(f, bridge, p, q, Vec2{ Fx::from_float(0.5f), Fx{} });
        L3D_REQUIRE(abs32(p.x.raw - Fx::from_float(2.0f).raw) < 4); // passed under
        L3D_REQUIRE(p.z.raw == 0);
    }

    // --- trigger + region volumes ---
    {
        const TriggerVolume t{ AABB3{ Vec3{}, Vec3{ Fx::from_int(2), Fx::from_int(2), Fx::from_int(2) } }, 7 };
        L3D_REQUIRE(trigger_at(&t, 1, Vec3{ Fx::from_int(1), Fx::from_int(1), Fx{} }) == 7);
        L3D_REQUIRE(trigger_at(&t, 1, Vec3{ Fx::from_int(3), Fx::from_int(1), Fx{} }) == -1);
        const Region3 r{ t.box, 42 };
        L3D_REQUIRE(region_at(&r, 1, Vec3{ Fx::from_int(1), Fx::from_int(1), Fx{} }) == 42);
        L3D_REQUIRE(region_at(nullptr, 0, Vec3{}) == -1);
    }

    L3D_REQUIRE(world3_selfcheck());

    std::printf("world3 OK\n");
    return 0;
}
