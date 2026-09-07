// Roadmap Phase 6A translation unit.
//
// Compile-proves the dispatch glue for every linked target's toolchain.
// npc_dispatch_selfcheck() runs a deterministic home->shop day fragment;
// unreferenced, it is dropped by --gc-sections (firmware) or never pulled
// from the static lib.
#include "npc_dispatch.h"
#include "entity.h"

namespace l3d {

namespace {
bool open_space(const Vec3&, const Vec3&) { return false; }

Region3 tag_region(uint16_t tag, int x0, int y0, int x1, int y1) {
    Region3 r{};
    r.box.mn = Vec3{Fx::from_int(x0), Fx::from_int(y0), Fx{}};
    r.box.mx = Vec3{Fx::from_int(x1), Fx::from_int(y1), Fx::from_int(2)};
    r.contentTag = tag;
    return r;
}
} // namespace

bool npc_dispatch_selfcheck() {
    EntityPool<4> entities{};
    entities.init();
    TransformPool<4> transforms{};
    NPCPool<4> npcs{};
    npcs.init();
    SchedulePool<4> sched{};
    sched.init();
    GameClock clock{};
    clock.minute = 475; // 07:55, before the shop opens

    constexpr uint16_t HOME = 1, SHOP = 2;
    Region3 regions[2] = {tag_region(HOME, 0, 0, 2, 2),
                          tag_region(SHOP, 10, 0, 12, 2)};
    const ScheduleEntry day[] = {
        {0, 480, HOME, uint8_t(SchedBehavior::REST)},
        {480, 1440, SHOP, uint8_t(SchedBehavior::WORK)},
    };

    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(1), Fx::from_int(1), Fx{}};
    transforms.attach(e, t);
    const uint16_t n = npcs.spawn(e, NPCArchetype::RESIDENT);
    if (!sched.set(n, day, 2)) return false;

    // 07:55 -> target HOME center (1,1): already there -> ARRIVED.
    if (npc_dispatch_step(npcs, transforms, sched, regions, 2, n, clock, 500,
                          Fx::from_int(2), open_space) != DispatchResult::OK)
        return false;
    const NPCAgent* npc = npcs.get(n);
    if (!npc || npc->state != uint8_t(NPCState::ARRIVED)) return false;

    // 08:00 -> schedule flips to SHOP center (11,1): must re-command.
    clock.minute = 480;
    if (npc_dispatch_step(npcs, transforms, sched, regions, 2, n, clock, 500,
                          Fx::from_int(2), open_space) != DispatchResult::OK)
        return false;
    npc = npcs.get(n);
    if (!npc || npc->state != uint8_t(NPCState::MOVING)) return false;
    if (npc->target.x.raw != Fx::from_int(11).raw) return false;

    // Walk until arrival: 10 units at 2 u/s, 500 ms ticks.
    for (int i = 0; i < 10; ++i) {
        if (npc_dispatch_step(npcs, transforms, sched, regions, 2, n, clock,
                              500, Fx::from_int(2), open_space) != DispatchResult::OK)
            return false;
    }
    npc = npcs.get(n);
    if (!npc || npc->state != uint8_t(NPCState::ARRIVED)) return false;
    const Transform3* m = transforms.get(e);
    if (!m || m->pos.x.raw != Fx::from_int(11).raw) return false;
    if (m->pos.y.raw != Fx::from_int(1).raw) return false;
    return true;
}

} // namespace l3d
