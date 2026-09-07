// Roadmap Phase 3 tests: entity lifecycle, generational ids, fixed pools,
// deterministic iteration, Transform/Collider association, stale rejection.
#include "../engine/entity.h"
#include "../engine/world3.h" // TriggerVolume overlap check (inline, no link dep)
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static int g_visit[8];
static int g_visits = 0;

int main() {
    // --- uninitialized pool is safe (spawn refused) ---
    {
        EntityPool<4> p{};
        L3D_REQUIRE(p.empty());
        L3D_REQUIRE(p.spawn(EntityKind::PLAYER) == L3D_ENTITY_INVALID);
    }

    EntityPool<8> pool{};
    pool.init();
    L3D_REQUIRE(pool.empty() && !pool.full() && pool.live_count() == 0);

    // --- create: unique ids, kinds, counts ---
    uint16_t ids[8]{};
    for (int i = 0; i < 8; ++i) {
        ids[i] = pool.spawn(EntityKind(i % int(EntityKind::COUNT)));
        L3D_REQUIRE(ids[i] != L3D_ENTITY_INVALID);
        for (int j = 0; j < i; ++j) L3D_REQUIRE(ids[j] != ids[i]);
    }
    L3D_REQUIRE(pool.full() && pool.live_count() == 8);
    L3D_REQUIRE(pool.spawn(EntityKind::NPC) == L3D_ENTITY_INVALID); // capacity
    for (int i = 0; i < 8; ++i) {
        L3D_REQUIRE(pool.alive(ids[i]) && pool.active(ids[i]));
        bool ok = false;
        L3D_REQUIRE(pool.kind_of(ids[i], &ok) == EntityKind(i % int(EntityKind::COUNT)) && ok);
    }
    bool ok = true;
    L3D_REQUIRE(pool.kind_of(0x1234, &ok) == EntityKind::UNKNOWN && !ok);

    // --- deterministic iteration: slot-index order, active only ---
    pool.set_enabled(ids[3], false);
    pool.set_enabled(ids[6], false);
    g_visits = 0;
    pool.for_each_active([](uint16_t id, EntityKind) {
        if (g_visits < 8) g_visit[g_visits++] = int(entity_slot_index(id));
    });
    L3D_REQUIRE(g_visits == 6);
    for (int i = 1; i < g_visits; ++i) L3D_REQUIRE(g_visit[i - 1] < g_visit[i]);
    pool.set_enabled(ids[3], true);
    pool.set_enabled(ids[6], true);

    // --- destroy + reuse: same slot, new generation, stale rejected ---
    const size_t slot1 = entity_slot_index(ids[1]);
    const uint8_t gen1 = entity_generation(ids[1]);
    L3D_REQUIRE(pool.destroy(ids[1]));
    L3D_REQUIRE(!pool.alive(ids[1]) && pool.live_count() == 7);
    L3D_REQUIRE(!pool.destroy(ids[1])); // double-destroy
    const uint16_t r1 = pool.spawn(EntityKind::DOOR);
    L3D_REQUIRE(r1 != L3D_ENTITY_INVALID);
    L3D_REQUIRE(entity_slot_index(r1) == slot1); // most-recent-free reuse
    L3D_REQUIRE(entity_generation(r1) != gen1 && entity_generation(r1) != 0);

    // --- Transform association follows the live id, not the stale one ---
    TransformPool<8> tp{};
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(9), Fx::from_int(9), Fx{}};
    t.yaw = 1234;
    tp.attach(ids[2], t);
    const Transform3* g2 = tp.get(ids[2]);
    L3D_REQUIRE(g2 && g2->pos.x.raw == Fx::from_int(9).raw && g2->yaw == 1234);
    L3D_REQUIRE(tp.get(ids[5]) == nullptr); // never attached
    // Owner discipline: detach components before destroy; the stale id
    // then addresses nothing.
    L3D_REQUIRE(tp.detach(ids[2]));
    L3D_REQUIRE(pool.destroy(ids[2]));
    L3D_REQUIRE(tp.get(ids[2]) == nullptr);
    const uint16_t r2 = pool.spawn(EntityKind::TRIGGER);
    if (entity_slot_index(r2) == entity_slot_index(ids[2])) {
        // Slot reused: old generation's component invisible to the new id
        // until explicitly re-attached.
        L3D_REQUIRE(tp.get(r2) == nullptr);
        Transform3 nt{};
        nt.pos = Vec3{Fx::from_int(1), Fx::from_int(1), Fx{}};
        tp.attach(r2, nt);
        L3D_REQUIRE(tp.get(r2)->pos.x.raw == Fx::from_int(1).raw);
        L3D_REQUIRE(tp.detach(r2));
        L3D_REQUIRE(tp.get(r2) == nullptr);
        L3D_REQUIRE(!tp.detach(r2));
    }

    // --- Collider: feet-anchored box + World3 trigger overlap ---
    ColliderPool<8> cp{};
    Collider c{Fx::from_float(0.5f), Fx::from_float(0.5f), Fx::from_float(1.7f), true};
    cp.attach(ids[0], c);
    Transform3 et{};
    et.pos = Vec3{Fx::from_int(4), Fx::from_int(4), Fx{}};
    AABB3 box{};
    L3D_REQUIRE(collider_world_box(cp, ids[0], et, &box));
    L3D_REQUIRE(aabb_contains(box, Vec3{Fx::from_int(4), Fx::from_int(4), Fx::from_int(1)}));
    L3D_REQUIRE(!aabb_contains(box, Vec3{Fx::from_int(5), Fx::from_int(4), Fx{}}));
    // A World3 trigger volume sees the entity through its collider box.
    const TriggerVolume trig{box, 3};
    L3D_REQUIRE(trigger_at(&trig, 1, Vec3{Fx::from_int(4), Fx::from_int(4), Fx{}}) == 3);
    L3D_REQUIRE(!collider_world_box(cp, ids[4], et, &box)); // no collider
    L3D_REQUIRE(cp.detach(ids[0]));
    L3D_REQUIRE(pool.destroy(ids[0]));
    L3D_REQUIRE(!collider_world_box(cp, ids[0], et, &box)); // detached

    // --- drain to empty ---
    for (int i = 0; i < 8; ++i) {
        // ids[0..2] already destroyed; destroy() is stale-safe.
        pool.destroy(ids[i]);
    }
    pool.destroy(r1);
    pool.destroy(r2);
    L3D_REQUIRE(pool.empty() && pool.live_count() == 0 && !pool.full());

    L3D_REQUIRE(entity_selfcheck());

    // --- id bit layout contract ---
    L3D_REQUIRE(entity_make_id(0x12, 0x34) == 0x1234u);
    L3D_REQUIRE(entity_slot_index(0x1234u) == 0x34u);
    L3D_REQUIRE(entity_generation(0x1234u) == 0x12u);

    std::printf("entity OK\n");
    return 0;
}
