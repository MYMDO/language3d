#pragma once
// Phase 1 (True 3D world): fixed-point 3D math primitives.
//
// Design rules for this layer:
// - 16.16 Fx everywhere, matching the existing simulation representation.
// - No heap, no exceptions, no virtual dispatch, no STL containers.
// - Header-only inline functions; safe to include from any target
//   (host, RP2040 firmware, tests).
// - World convention: X = east/west, Y = north/south, Z = height (up).
//   Yaw uses the same 65536-units-per-turn convention as the 2D Player.
#include "core.h"
#include "l3d_math.h"

namespace l3d {

struct Vec3 {
    Fx x{};
    Fx y{};
    Fx z{};
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3 operator-(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 v, Fx s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

// Position (feet point) + facing. Pitch/roll are intentionally absent:
// Phase 1 needs yaw-only locomotion; camera pitch arrives with the 3D
// renderer work, not with the world model.
struct Transform3 {
    Vec3 pos{};
    u16 yaw{0};
};

// Forward direction on the XY plane for a yaw turn value.
inline Vec2 yaw_forward(u16 yaw) {
    return Vec2{TrigLut::cos16(yaw), TrigLut::sin16(yaw)};
}

// Axis-aligned bounding box, inclusive on min, exclusive on max.
struct AABB3 {
    Vec3 mn{};
    Vec3 mx{};
};

constexpr bool aabb_contains(const AABB3& box, const Vec3& p) {
    return p.x >= box.mn.x && p.x < box.mx.x &&
           p.y >= box.mn.y && p.y < box.mx.y &&
           p.z >= box.mn.z && p.z < box.mx.z;
}

constexpr bool aabb_overlaps(const AABB3& a, const AABB3& b) {
    return a.mn.x < b.mx.x && a.mx.x > b.mn.x &&
           a.mn.y < b.mx.y && a.mx.y > b.mn.y &&
           a.mn.z < b.mx.z && a.mx.z > b.mn.z;
}

} // namespace l3d
