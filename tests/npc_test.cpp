// Roadmap Phase 4 tests: NPC lifecycle, entity/transform association,
// deterministic kinematic movement, blocked/arrived states.
#include "../engine/npc.h"
#include "../engine/entity.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static bool open_space(const Vec3&, const Vec3&) { return false; }

struct WallAtX {
    Fx x;
    bool operator()(const Vec3& from, const Vec3& to) const {
        // Vertical wall plane: deny any step crossing x.
        const int32_t a = from.x.raw - x.raw;
        const int32_t b = to.x.raw - x.raw;
        return (a < 0 && b >= 0) || (a > 0 && b <= 0);
    }
};

int main() {
    // --- uninitialized pool is safe ---
    {
        NPCPool<4> p{};
        L3D_REQUIRE(p.empty());
        L3D_REQUIRE(p.spawn(0x0100, NPCArchetype::RESIDENT) == L3D_NPC_INVALID);
    }

    EntityPool<8> entities{};
    entities.init();
    TransformPool<8> transforms{};
    NPCPool<8> npcs{};
    npcs.init();
    L3D_REQUIRE(npcs.empty() && !npcs.full());

    // --- spawn binds an existing entity, rejects INVALID ---
    const uint16_t e0 = entities.spawn(EntityKind::NPC);
    const uint16_t e1 = entities.spawn(EntityKind::NPC);
    L3D_REQUIRE(npcs.spawn(L3D_ENTITY_INVALID, NPCArchetype::RESIDENT) == L3D_NPC_INVALID);
    const uint16_t n0 = npcs.spawn(e0, NPCArchetype::SHOPKEEPER);
    const uint16_t n1 = npcs.spawn(e1, NPCArchetype::RESIDENT);
    L3D_REQUIRE(n0 != L3D_NPC_INVALID && n1 != L3D_NPC_INVALID && n0 != n1);
    L3D_REQUIRE(npcs.live_count() == 2);
    const NPCAgent* g0 = npcs.get(n0);
    L3D_REQUIRE(g0 && g0->entity == e0);
    L3D_REQUIRE(g0->archetype == uint8_t(NPCArchetype::SHOPKEEPER));
    L3D_REQUIRE(g0->state == uint8_t(NPCState::IDLE));
    L3D_REQUIRE(npcs.get(0x1234) == nullptr); // stale rejected

    // --- capacity ---
    uint16_t filler[6]{};
    uint16_t filler_e[6]{};
    for (int i = 0; i < 6; ++i) {
        filler_e[i] = entities.spawn(EntityKind::NPC);
        filler[i] = npcs.spawn(filler_e[i], NPCArchetype::CLERK);
        L3D_REQUIRE(filler[i] != L3D_NPC_INVALID);
    }
    L3D_REQUIRE(npcs.full());
    L3D_REQUIRE(npcs.spawn(entities.spawn(EntityKind::NPC), NPCArchetype::CLERK) == L3D_NPC_INVALID);

    // --- deterministic movement: 2 u/s, 250 ms ticks, X-only order ---
    Transform3 t0{};
    t0.pos = Vec3{Fx::from_int(0), Fx::from_int(0), Fx::from_int(2)};
    transforms.attach(e0, t0);
    Vec3 target{Fx::from_int(2), Fx::from_int(1), Fx{}};
    L3D_REQUIRE(npcs.command_move(n0, target, Fx::from_int(2)));
    L3D_REQUIRE(npcs.get(n0)->state == uint8_t(NPCState::MOVING));
    for (int i = 0; i < 2; ++i) // 2 x 0.5 u steps
        L3D_REQUIRE(npcs.update(transforms, n0, 250, open_space));
    const Transform3* m0 = transforms.get(e0);
    L3D_REQUIRE(m0 && m0->pos.x.raw == Fx::from_int(1).raw);
    L3D_REQUIRE(m0->pos.y.raw == Fx::from_int(1).raw); // Y arrived early
    L3D_REQUIRE(m0->pos.z.raw == Fx::from_int(2).raw); // Z preserved
    L3D_REQUIRE(npcs.get(n0)->state == uint8_t(NPCState::MOVING));
    for (int i = 0; i < 2; ++i)
        L3D_REQUIRE(npcs.update(transforms, n0, 250, open_space));
    L3D_REQUIRE(transforms.get(e0)->pos.x.raw == Fx::from_int(2).raw);
    L3D_REQUIRE(npcs.get(n0)->state == uint8_t(NPCState::ARRIVED));

    // --- blocked: wall plane at x=0.25, NPC at x=0 moving to x=3 ---
    Transform3 t1{};
    t1.pos = Vec3{Fx::from_int(0), Fx::from_int(5), Fx{}};
    transforms.attach(e1, t1);
    Vec3 far{Fx::from_int(3), Fx::from_int(5), Fx{}};
    L3D_REQUIRE(npcs.command_move(n1, far, Fx::from_int(1)));
    WallAtX wall{Fx::from_float(0.25f)};
    L3D_REQUIRE(npcs.update(transforms, n1, 500, wall));
    L3D_REQUIRE(npcs.get(n1)->state == uint8_t(NPCState::BLOCKED));
    L3D_REQUIRE(transforms.get(e1)->pos.x.raw == Fx::from_int(0).raw);
    // Stop order returns to IDLE.
    L3D_REQUIRE(npcs.command_stop(n1));
    L3D_REQUIRE(npcs.get(n1)->state == uint8_t(NPCState::IDLE));

    // --- update without bound transform waits (IDLE), update stale fails ---
    L3D_REQUIRE(npcs.destroy(filler[0])); // free one slot for n2
    L3D_REQUIRE(entities.destroy(filler_e[0]));
    const uint16_t e2 = entities.spawn(EntityKind::NPC);
    const uint16_t n2 = npcs.spawn(e2, NPCArchetype::RESIDENT);
    Vec3 anywhere{Fx::from_int(9), Fx::from_int(9), Fx{}};
    L3D_REQUIRE(npcs.command_move(n2, anywhere, Fx::from_int(1)));
    L3D_REQUIRE(npcs.update(transforms, n2, 100, open_space));
    L3D_REQUIRE(npcs.get(n2)->state == uint8_t(NPCState::IDLE));
    L3D_REQUIRE(!npcs.update(transforms, 0x1234, 100, open_space));
    L3D_REQUIRE(!npcs.command_move(0x1234, anywhere, Fx::from_int(1)));

    // --- destroy + generation reuse ---
    const size_t slot0 = entity_slot_index(n0);
    const uint8_t gen0 = entity_generation(n0);
    L3D_REQUIRE(npcs.destroy(n0));
    L3D_REQUIRE(!npcs.alive(n0) && npcs.get(n0) == nullptr);
    L3D_REQUIRE(!npcs.destroy(n0));
    const uint16_t rn = npcs.spawn(e0, NPCArchetype::CLERK);
    L3D_REQUIRE(entity_slot_index(rn) == slot0);
    L3D_REQUIRE(entity_generation(rn) != gen0 && entity_generation(rn) != 0);
    L3D_REQUIRE(npcs.get(rn)->archetype == uint8_t(NPCArchetype::CLERK));

    // --- drain ---
    npcs.destroy(n1);
    npcs.destroy(n2);
    npcs.destroy(rn);
    for (int i = 0; i < 6; ++i) npcs.destroy(filler[i]);
    L3D_REQUIRE(npcs.empty());

    L3D_REQUIRE(npc_selfcheck());

    std::printf("npc OK\n");
    return 0;
}
