// Roadmap Phase 7 translation unit.
//
// Compile-proves the interaction module for every linked target's
// toolchain. interact_selfcheck() is deterministic and I/O-free;
// unreferenced, it is dropped by --gc-sections (firmware) or never pulled
// from the static lib.
#include "interact.h"
#include "entity.h"

namespace l3d {

bool interact_selfcheck() {
    EntityPool<4> entities{};
    entities.init();
    TransformPool<4> transforms{};
    NPCPool<4> npcs{};
    npcs.init();

    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(2), Fx::from_int(0), Fx{}};
    transforms.attach(e, t);
    const uint16_t n = npcs.spawn(e, NPCArchetype::RESIDENT);

    Transform3 player{};
    player.pos = Vec3{};
    player.yaw = 0; // facing +X
    const Fx radius = Fx::from_int(3);
    if (interact_target(npcs, transforms, player, radius) != n) return false;

    InteractSession s{};
    if (interact_try(s, n, 1000) != InteractResult::STARTED) return false;
    if (!s.active || s.npc != n) return false;
    if (interact_try(s, n, 1100) != InteractResult::ALREADY) return false;
    if (interact_end(s, 1200) != InteractResult::ENDED) return false;
    if (s.active) return false;
    if (interact_try(s, n, 1300) != InteractResult::COOLDOWN) return false;
    if (interact_try(s, n, 1701) != InteractResult::STARTED) return false;
    if (interact_end(s, 1800) != InteractResult::ENDED) return false;
    if (interact_try(s, L3D_INTERACT_NONE, 2400) != InteractResult::NO_TARGET)
        return false;
    return true;
}

} // namespace l3d
