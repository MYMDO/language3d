#pragma once
// Phase 1 (True 3D world): height-aware world model + locomotion.
//
// This header deliberately models the WORLD (X/Y/Z geometry, floors,
// ceilings, stairs, trigger/region volumes) without any rendering code.
// The existing 2.5D raycaster is untouched; a 3D-capable renderer can be
// introduced later against exactly this model.
//
// Memory model (RP2040-safe):
// - HeightField is a zero-copy view over caller-owned (flash-resident)
//   packed bytes: 1 byte/cell/layer. No parsing, no copies, no heap.
// - walk_move / trigger_at / region_at operate on caller-owned structs.
// - No exceptions, no RTTI, no virtual dispatch.
#include "transform3.h"

namespace l3d {

// Height quantum: cell values are int8 in units of 1/8 world unit
// (range -16 .. +15.875), enough for houses, stairs, bridges, dungeons.
struct HeightField {
    const int8_t* floorQ{nullptr}; // row-major w*h; nullptr => flat 0
    const int8_t* ceilQ{nullptr};  // optional; nullptr => floor + default
    u16 w{0};
    u16 h{0};
};

// Phase-1 world conventions (world units, 16.16 Fx).
struct World3Limits {
    // Maximum floor rise the player can step onto without jumping.
    // Stairs are authored as ramps with per-cell rise below this.
    static Fx step_up_max() { return Fx::from_float(0.45f); }
    // Default clear height above a floor when no ceiling layer exists.
    static Fx default_clear() { return Fx::from_float(2.0f); }
    // Player capsule height and eye height above the feet point.
    static Fx player_height() { return Fx::from_float(1.7f); }
    static Fx eye_height() { return Fx::from_float(1.6f); }
    // Horizontal collision radius of the player capsule.
    static Fx body_radius() { return Fx::from_float(0.2f); }
};

inline Fx floor_at(const HeightField& f, i32 cx, i32 cy) {
    if (!f.floorQ || cx < 0 || cy < 0 || cx >= i32(f.w) || cy >= i32(f.h))
        return Fx{};
    const int8_t q = f.floorQ[size_t(cy) * f.w + size_t(cx)];
    return Fx::from_raw(i32(q) * (Fx::ONE / 8));
}

inline Fx ceil_at(const HeightField& f, i32 cx, i32 cy) {
    if (f.ceilQ && cx >= 0 && cy >= 0 && cx < i32(f.w) && cy < i32(f.h)) {
        const int8_t q = f.ceilQ[size_t(cy) * f.w + size_t(cx)];
        return Fx::from_raw(i32(q) * (Fx::ONE / 8));
    }
    return floor_at(f, cx, cy) + World3Limits::default_clear();
}

struct MoveQuery {
    Fx radius{World3Limits::body_radius()};
    Fx height{World3Limits::player_height()};
    Fx stepUp{World3Limits::step_up_max()};
};

// Axis-separated 2.5D slide with step-up and headroom checks.
// - pos is the FEET point; on success pos.z tracks the supporting floor
//   (stairs up AND down; free falling arrives with the physics phase).
// - solid(cx, cy) reports full-height blockers (walls, closed doors);
//   height variation is resolved from the HeightField. Out-of-range cells
//   are reported through solid() by the caller (typically solid = wall).
template <typename SolidFn>
void walk_move(const HeightField& f, SolidFn solid, Vec3& pos,
               const MoveQuery& q, Vec2 delta) {
    for (int axis = 0; axis < 2; ++axis) {
        Vec3 probe = pos;
        if (axis == 0) probe.x = probe.x + delta.x;
        else probe.y = probe.y + delta.y;

        const Fx r = q.radius;
        const i32 x0 = (probe.x - r).to_int();
        const i32 x1 = (probe.x + r).to_int();
        const i32 y0 = (probe.y - r).to_int();
        const i32 y1 = (probe.y + r).to_int();

        bool blocked = false;
        Fx topFloor = Fx::from_raw(INT32_MIN / 2);
        Fx lowCeil = Fx::from_raw(INT32_MAX / 2);
        for (i32 cy = y0; cy <= y1 && !blocked; ++cy) {
            for (i32 cx = x0; cx <= x1; ++cx) {
                if (solid(cx, cy)) {
                    // A solid cell blocks only if it actually overlaps the
                    // body vertically: floors far below or ceilings far above
                    // the player do not collide (bridges/tunnels work).
                    const Fx fl = floor_at(f, cx, cy);
                    const Fx ce = ceil_at(f, cx, cy);
                    const Fx feet = pos.z;
                    if (fl < feet + q.height && ce > feet + Fx::from_raw(Fx::ONE / 8))
                        blocked = true;
                    continue;
                }
                const Fx fl = floor_at(f, cx, cy);
                const Fx ce = ceil_at(f, cx, cy);
                if (fl.raw > topFloor.raw) topFloor = fl;
                if (ce.raw < lowCeil.raw) lowCeil = ce;
            }
        }
        if (blocked) continue; // slide: keep the other axis
        if (topFloor.raw == INT32_MIN / 2) continue; // fully outside: deny
        // Outside the authored field there is no supporting data; a zero
        // HeightField ({}) means "no world", so deny rather than walking
        // on an implicit infinite plane.
        if (f.w == 0 || f.h == 0) continue;
        if (topFloor - pos.z > q.stepUp) continue;   // too tall to step onto
        if (lowCeil - topFloor < q.height) continue; // no headroom
        pos = probe;
        pos.z = topFloor;
    }
}

inline Fx eye_z(const Vec3& feetPos) {
    return feetPos.z + World3Limits::eye_height();
}

// Opaque content-binding volumes. The world model never interprets `tag`
// (quest ids, vocabulary pack ids, ...); content layers defined in later
// phases assign meaning. -1 (as int) means "none".
struct TriggerVolume {
    AABB3 box{};
    u16 id{0};
};

inline int trigger_at(const TriggerVolume* vols, size_t count, const Vec3& p) {
    for (size_t i = 0; i < count; ++i) {
        if (aabb_contains(vols[i].box, p)) return int(vols[i].id);
    }
    return -1;
}

struct Region3 {
    AABB3 box{};
    u16 contentTag{0}; // opaque: vocabulary pack / district / biome id...
};

inline int region_at(const Region3* regions, size_t count, const Vec3& p) {
    for (size_t i = 0; i < count; ++i) {
        if (aabb_contains(regions[i].box, p)) return int(regions[i].contentTag);
    }
    return -1;
}

// Deterministic self-check (no I/O, no heap). Implemented in world3.cpp so
// every linked target — including the RP2040 firmware — compile-proves this
// translation unit for its toolchain. Unused symbols are dropped by
// --gc-sections (firmware) or never pulled from the static core lib (tests).
bool world3_selfcheck();

} // namespace l3d
