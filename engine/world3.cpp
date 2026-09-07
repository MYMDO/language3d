// Phase 1 world-model translation unit.
//
// Exists so every linked target (host game, host tests, RP2040 firmware)
// compile-proves this code for its own toolchain. world3_selfcheck() runs
// a tiny deterministic scenario with zero I/O; callers that never reference
// it pay nothing (--gc-sections on firmware, on-demand static-lib members
// on host).
#include "world3.h"

namespace l3d {

namespace {
bool open_solid(i32, i32) { return false; }
} // namespace

bool world3_selfcheck() {
    // Empty field means "no world": movement must be denied.
    const HeightField flat{};
    Vec3 p{ Fx::from_int(1), Fx::from_int(1), Fx{} };
    const MoveQuery q{};
    walk_move(flat, open_solid, p, q, Vec2{ Fx::from_float(0.3f), Fx{} });
    if (p.x.raw != Fx::from_int(1).raw) return false;
    if (p.z.raw != 0) return false;

    // Single authored cell at floor 0: movement inside it is allowed.
    static const int8_t floor1[1] = {0};
    const HeightField one{ floor1, nullptr, 1, 1 };
    Vec3 p2{ Fx::from_float(0.5f), Fx::from_float(0.5f), Fx{} };
    auto boxed = [](i32 cx, i32 cy) { return cx != 0 || cy != 0; };
    walk_move(one, boxed, p2, q, Vec2{ Fx::from_float(0.1f), Fx{} });
    if (p2.z.raw != 0) return false;

    // Trigger + region lookup sanity.
    const TriggerVolume t{ AABB3{ Vec3{}, Vec3{ Fx::from_int(2), Fx::from_int(2), Fx::from_int(2)} }, 7 };
    if (trigger_at(&t, 1, Vec3{ Fx::from_int(1), Fx::from_int(1), Fx{} }) != 7) return false;
    if (trigger_at(&t, 1, Vec3{ Fx::from_int(5), Fx::from_int(5), Fx{} }) != -1) return false;
    const Region3 r{ t.box, 11 };
    if (region_at(&r, 1, Vec3{ Fx::from_int(1), Fx::from_int(1), Fx{} }) != 11) return false;

    // Eye height convention.
    if (eye_z(Vec3{}).raw != World3Limits::eye_height().raw) return false;
    return true;
}

} // namespace l3d
