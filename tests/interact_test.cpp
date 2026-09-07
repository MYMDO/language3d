// Roadmap Phase 7 tests: proximity, facing gate, deterministic selection,
// session lifecycle with cooldown.
#include "../engine/interact.h"
#include "../engine/entity.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static uint16_t spawn_npc(EntityPool<8>& entities, TransformPool<8>& transforms,
                          NPCPool<8>& npcs, int x, int y) {
    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(x), Fx::from_int(y), Fx{}};
    transforms.attach(e, t);
    return npcs.spawn(e, NPCArchetype::RESIDENT);
}

static Transform3 player_at(int x, int y, uint16_t yaw) {
    Transform3 p{};
    p.pos = Vec3{Fx::from_int(x), Fx::from_int(y), Fx{}};
    p.yaw = yaw;
    return p;
}

int main() {
    EntityPool<8> entities{};
    entities.init();
    TransformPool<8> transforms{};
    NPCPool<8> npcs{};
    npcs.init();
    const Fx radius = Fx::from_int(3);

    // Player at origin facing +X (yaw 0).
    const Transform3 p0 = player_at(0, 0, 0);

    // --- proximity boundary: 3 in, 4 out ---
    const uint16_t near = spawn_npc(entities, transforms, npcs, 3, 0);
    const uint16_t far = spawn_npc(entities, transforms, npcs, 4, 0);
    (void)far;
    L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) == near);

    // --- nearest wins among several in radius ---
    const uint16_t mid = spawn_npc(entities, transforms, npcs, 2, 0);
    L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) == mid);

    // --- facing gate: nearer NPC behind is ignored ---
    const uint16_t behind = spawn_npc(entities, transforms, npcs, -1, 0);
    (void)behind;
    L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) == mid);

    // --- tie at equal distance: lowest Entity id wins, not slot order ---
    // mid is at (2,0) d2=4; add symmetric (0,2)? dot with +X fwd = 0 -> gated.
    // Use (2,0) duplicate distance via second NPC spawned later at same spot:
    // equal d2=4, higher entity id must lose to mid.
    const uint16_t dup = spawn_npc(entities, transforms, npcs, 2, 0);
    L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) == mid);
    (void)dup;

    // --- NPC without bound transform is skipped ---
    {
        const uint16_t e = entities.spawn(EntityKind::NPC);
        const uint16_t n = npcs.spawn(e, NPCArchetype::RESIDENT);
        (void)n;
        L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) == mid);
    }

    // --- session lifecycle with cooldown ---
    InteractSession s{};
    L3D_REQUIRE(interact_try(s, L3D_INTERACT_NONE, 1000) == InteractResult::NO_TARGET);
    L3D_REQUIRE(interact_try(s, mid, 1000) == InteractResult::STARTED);
    L3D_REQUIRE(s.active && s.npc == mid);
    L3D_REQUIRE(interact_try(s, near, 1100) == InteractResult::ALREADY);
    L3D_REQUIRE(s.npc == mid); // session keeps its NPC
    L3D_REQUIRE(interact_end(s, 1200) == InteractResult::ENDED);
    L3D_REQUIRE(!s.active);
    L3D_REQUIRE(interact_end(s, 1300) == InteractResult::NONE);
    L3D_REQUIRE(interact_try(s, near, 1400) == InteractResult::COOLDOWN);
    L3D_REQUIRE(!s.active);
    L3D_REQUIRE(interact_try(s, near, 1700) == InteractResult::STARTED); // window passed
    L3D_REQUIRE(interact_end(s, 1800) == InteractResult::ENDED);

    // --- determinism: same world, same target twice ---
    L3D_REQUIRE(interact_target(npcs, transforms, p0, radius) ==
                interact_target(npcs, transforms, p0, radius));

    // --- tiebreak uses Entity id, independent of NPC slot order ---
    {
        EntityPool<8> ee{};
        ee.init();
        TransformPool<8> tt{};
        NPCPool<8> nn{};
        nn.init();
        const uint16_t ea = ee.spawn(EntityKind::NPC); // slot 0
        const uint16_t eb = ee.spawn(EntityKind::NPC); // slot 1
        Transform3 t{};
        t.pos = Vec3{Fx::from_int(2), Fx::from_int(0), Fx{}};
        tt.attach(ea, t);
        tt.attach(eb, t);
        const uint16_t nb = nn.spawn(eb, NPCArchetype::RESIDENT); // npc slot 0
        const uint16_t na = nn.spawn(ea, NPCArchetype::RESIDENT); // npc slot 1
        (void)nb;
        // Same spot, same distance: lower Entity id (na) wins despite the
        // later NPC slot.
        L3D_REQUIRE(interact_target(nn, tt, p0, radius) == na);
    }

    // --- empty world: no target ---
    {
        EntityPool<8> ee{};
        ee.init();
        TransformPool<8> tt{};
        NPCPool<8> nn{};
        nn.init();
        L3D_REQUIRE(interact_target(nn, tt, p0, radius) == L3D_INTERACT_NONE);
    }

    L3D_REQUIRE(interact_selfcheck());

    std::printf("interact OK\n");
    return 0;
}
