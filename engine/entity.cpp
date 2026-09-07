// Roadmap Phase 3 translation unit.
//
// Compile-proves the entity/component pools for every linked target's
// toolchain (host game/tests, RP2040 firmware). entity_selfcheck() is a
// deterministic no-I/O scenario; unreferenced, it is dropped by
// --gc-sections (firmware) or never pulled from the static core lib.
#include "entity.h"

namespace l3d {

bool entity_selfcheck() {
    EntityPool<4> pool{};
    if (!pool.empty()) return false;
    if (pool.spawn(EntityKind::PLAYER) != L3D_ENTITY_INVALID) return false; // not init: safe

    pool.init();
    if (!pool.empty() || pool.full()) return false;

    const uint16_t a = pool.spawn(EntityKind::PLAYER);
    const uint16_t b = pool.spawn(EntityKind::NPC);
    if (a == L3D_ENTITY_INVALID || b == L3D_ENTITY_INVALID) return false;
    if (entity_slot_index(a) == entity_slot_index(b)) return false;
    if (!pool.alive(a) || !pool.active(a)) return false;

    TransformPool<4> tp{};
    ColliderPool<4> cp{};
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(3), Fx::from_int(4), Fx::from_int(1)};
    tp.attach(a, t);
    const Transform3* got = tp.get(a);
    if (!got || got->pos.x.raw != Fx::from_int(3).raw) return false;
    Collider c{Fx::from_float(0.2f), Fx::from_float(0.2f), Fx::from_float(1.7f), true};
    cp.attach(a, c);
    AABB3 box{};
    if (!collider_world_box(cp, a, *got, &box)) return false;
    if (!aabb_contains(box, Vec3{Fx::from_int(3), Fx::from_int(4), Fx::from_int(2)})) return false;

    if (!pool.destroy(a)) return false;
    if (pool.alive(a)) return false;
    if (pool.destroy(a)) return false; // double-destroy rejected

    // Components detach explicitly at destroy time (owner discipline);
    // after detach the stale id addresses nothing.
    if (!tp.detach(a) || !cp.detach(a)) return false;
    if (tp.get(a) != nullptr) return false;
    if (cp.get(a) != nullptr) return false;

    const uint16_t a2 = pool.spawn(EntityKind::ITEM);
    if (a2 == L3D_ENTITY_INVALID) return false;
    if (entity_slot_index(a2) != entity_slot_index(a)) return false; // slot reuse
    if (entity_generation(a2) == entity_generation(a)) return false; // new generation
    if (entity_generation(a2) == 0) return false;
    bool ok = false;
    if (pool.kind_of(a2, &ok) != EntityKind::ITEM || !ok) return false;

    if (!pool.destroy(a2) || !pool.destroy(b)) return false;
    return pool.empty();
}

} // namespace l3d
