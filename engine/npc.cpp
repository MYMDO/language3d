// Roadmap Phase 4 translation unit.
//
// Compile-proves the NPC pool for every linked target's toolchain.
// npc_selfcheck() is deterministic and I/O-free; unreferenced, it is
// dropped by --gc-sections (firmware) or never pulled from the static lib.
#include "npc.h"
#include "entity.h"

namespace l3d {

namespace {
bool never_blocked(const Vec3&, const Vec3&) { return false; }
} // namespace

bool npc_selfcheck() {
    EntityPool<4> entities{};
    entities.init();
    TransformPool<4> transforms{};
    NPCPool<4> npcs{};
    npcs.init();
    if (!npcs.empty()) return false;

    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(1), Fx::from_int(1), Fx{}};
    transforms.attach(e, t);

    const uint16_t n = npcs.spawn(e, NPCArchetype::SHOPKEEPER);
    if (n == L3D_NPC_INVALID) return false;
    const NPCAgent* npc = npcs.get(n);
    if (!npc || npc->entity != e) return false;
    if (npc->archetype != uint8_t(NPCArchetype::SHOPKEEPER)) return false;
    if (npc->state != uint8_t(NPCState::IDLE)) return false;

    Vec3 target{Fx::from_int(3), Fx::from_int(1), Fx{}};
    if (!npcs.command_move(n, target, Fx::from_int(1))) return false; // 1 u/s
    for (int i = 0; i < 4; ++i) { // 4 x 500 ms = 2.0 units
        if (!npcs.update(transforms, n, 500, never_blocked)) return false;
    }
    npc = npcs.get(n);
    if (!npc || npc->state != uint8_t(NPCState::ARRIVED)) return false;
    const Transform3* moved = transforms.get(e);
    if (!moved || moved->pos.x.raw != Fx::from_int(3).raw) return false;
    if (moved->pos.y.raw != Fx::from_int(1).raw) return false;

    if (!npcs.destroy(n)) return false;
    if (npcs.alive(n) || npcs.get(n) != nullptr) return false;
    if (npcs.destroy(n)) return false;
    return npcs.empty();
}

} // namespace l3d
