// Roadmap Phase 6A tests: schedule -> region -> movement integration.
#include "../engine/npc_dispatch.h"
#include "../engine/entity.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

constexpr uint16_t HOME = 1, SHOP = 2, CAFE = 3;

static bool open_space(const Vec3&, const Vec3&) { return false; }

struct WallAtX {
    Fx x;
    bool operator()(const Vec3& from, const Vec3& to) const {
        const int32_t a = from.x.raw - x.raw;
        const int32_t b = to.x.raw - x.raw;
        return (a < 0 && b >= 0) || (a > 0 && b <= 0);
    }
};

static Region3 make_region(uint16_t tag, int x0, int y0, int x1, int y1) {
    Region3 r{};
    r.box.mn = Vec3{Fx::from_int(x0), Fx::from_int(y0), Fx{}};
    r.box.mx = Vec3{Fx::from_int(x1), Fx::from_int(y1), Fx::from_int(2)};
    r.contentTag = tag;
    return r;
}

static void setup_world(EntityPool<8>& entities, TransformPool<8>& transforms,
                        NPCPool<8>& npcs, SchedulePool<8>& sched,
                        Region3* regions, uint16_t* out_npc) {
    entities.init();
    npcs.init();
    sched.init();
    regions[0] = make_region(HOME, 0, 0, 2, 2);    // center (1,1)
    regions[1] = make_region(SHOP, 10, 0, 12, 2);  // center (11,1)
    regions[2] = make_region(CAFE, 4, 8, 6, 10);   // center (5,9)
    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(1), Fx::from_int(1), Fx::from_int(1)};
    transforms.attach(e, t);
    const uint16_t n = npcs.spawn(e, NPCArchetype::RESIDENT);
    const ScheduleEntry day[] = {
        {0, 480, HOME, uint8_t(SchedBehavior::REST)},
        {480, 780, SHOP, uint8_t(SchedBehavior::WORK)},
        {780, 1440, CAFE, uint8_t(SchedBehavior::SOCIAL)},
    };
    L3D_REQUIRE(sched.set(n, day, 3));
    *out_npc = n;
}

static DispatchResult step(NPCPool<8>& npcs, TransformPool<8>& transforms,
                           SchedulePool<8>& sched, Region3* regions,
                           uint16_t n, GameClock& clock, uint32_t dt) {
    return npc_dispatch_step(npcs, transforms, sched, regions, 3, n, clock,
                             dt, Fx::from_int(2), open_space);
}

int main() {
    // --- active schedule resolution + region target + arrival at HOME ---
    {
        EntityPool<8> entities{};
        TransformPool<8> transforms{};
        NPCPool<8> npcs{};
        SchedulePool<8> sched{};
        Region3 regions[3]{};
        uint16_t n = 0;
        setup_world(entities, transforms, npcs, sched, regions, &n);
        GameClock clock{};
        clock.minute = 100;
        L3D_REQUIRE(npc_active_entry(sched, n, clock)->locationTag == HOME);
        L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 500) == DispatchResult::OK);
        // HOME center is (1,1): already there (z preserved at 1).
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::ARRIVED));
        L3D_REQUIRE(transforms.get(npcs.get(n)->entity)->pos.z.raw == Fx::from_int(1).raw);
    }

    // --- movement toward SHOP after 08:00 transition ---
    {
        EntityPool<8> entities{};
        TransformPool<8> transforms{};
        NPCPool<8> npcs{};
        SchedulePool<8> sched{};
        Region3 regions[3]{};
        uint16_t n = 0;
        setup_world(entities, transforms, npcs, sched, regions, &n);
        GameClock clock{};
        clock.minute = 479;
        L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 500) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::ARRIVED)); // still HOME
        clock.minute = 480; // schedule flips
        L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 500) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::MOVING));
        L3D_REQUIRE(npcs.get(n)->target.x.raw == Fx::from_int(11).raw);
        for (int i = 0; i < 12; ++i)
            L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 500) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::ARRIVED));
        const Transform3* m = transforms.get(npcs.get(n)->entity);
        L3D_REQUIRE(m->pos.x.raw == Fx::from_int(11).raw && m->pos.y.raw == Fx::from_int(1).raw);
    }

    // --- overnight transition: 23:59 CAFE -> 00:00 HOME ---
    {
        EntityPool<8> entities{};
        TransformPool<8> transforms{};
        NPCPool<8> npcs{};
        SchedulePool<8> sched{};
        Region3 regions[3]{};
        uint16_t n = 0;
        setup_world(entities, transforms, npcs, sched, regions, &n);
        GameClock clock{};
        clock.minute = 1439;
        // Park the NPC at CAFE center first.
        Transform3 t{};
        t.pos = Vec3{Fx::from_int(5), Fx::from_int(9), Fx{}};
        transforms.attach(npcs.get(n)->entity, t);
        L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 100) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::ARRIVED)); // CAFE
        clock.advance(60000); // +60 min -> 00:59 day+1, HOME schedule
        L3D_REQUIRE(clock.minute == 59);
        L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, 100) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::MOVING));
        L3D_REQUIRE(npcs.get(n)->target.x.raw == Fx::from_int(1).raw);
    }

    // --- blocked movement reports BLOCKED through dispatch ---
    {
        EntityPool<8> entities{};
        TransformPool<8> transforms{};
        NPCPool<8> npcs{};
        SchedulePool<8> sched{};
        Region3 regions[3]{};
        uint16_t n = 0;
        setup_world(entities, transforms, npcs, sched, regions, &n);
        GameClock clock{};
        clock.minute = 500; // SHOP leg
        WallAtX wall{Fx::from_float(1.5f)};
        L3D_REQUIRE(npc_dispatch_step(npcs, transforms, sched, regions, 3, n,
                                      clock, 500, Fx::from_int(2), wall) == DispatchResult::OK);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::BLOCKED));
    }

    // --- determinism: two identical runs, identical states ---
    // --- framerate independence: 10x100ms vs 100x10ms, same arrival ---
    {
        Transform3 a_end{}, b_end{};
        uint8_t a_state = 0, b_state = 0;
        for (int run = 0; run < 2; ++run) {
            EntityPool<8> entities{};
            TransformPool<8> transforms{};
            NPCPool<8> npcs{};
            SchedulePool<8> sched{};
            Region3 regions[3]{};
            uint16_t n = 0;
            setup_world(entities, transforms, npcs, sched, regions, &n);
            GameClock clock{};
            clock.minute = 480;
            const uint32_t dt = (run == 0) ? 100 : 10;
            const int steps = (run == 0) ? 120 : 1200; // both = 12 s
            for (int i = 0; i < steps; ++i) {
                clock.advance(dt);
                L3D_REQUIRE(step(npcs, transforms, sched, regions, n, clock, dt) == DispatchResult::OK);
            }
            // 12 s: clock advanced 12 min (480 -> 492), still SHOP leg.
            const Transform3* m = transforms.get(npcs.get(n)->entity);
            if (run == 0) {
                a_end = *m;
                a_state = npcs.get(n)->state;
            } else {
                b_end = *m;
                b_state = npcs.get(n)->state;
            }
        }
        L3D_REQUIRE(a_state == uint8_t(NPCState::ARRIVED) && b_state == a_state);
        L3D_REQUIRE(a_end.pos.x.raw == b_end.pos.x.raw);
        L3D_REQUIRE(a_end.pos.y.raw == b_end.pos.y.raw);
        L3D_REQUIRE(a_end.pos.x.raw == Fx::from_int(11).raw);
    }

    // --- stale NPC + invalid locationTag ---
    {
        EntityPool<8> entities{};
        TransformPool<8> transforms{};
        NPCPool<8> npcs{};
        SchedulePool<8> sched{};
        Region3 regions[3]{};
        uint16_t n = 0;
        setup_world(entities, transforms, npcs, sched, regions, &n);
        GameClock clock{};
        clock.minute = 500;
        L3D_REQUIRE(npc_dispatch_step(npcs, transforms, sched, regions, 3, 0x1234,
                                      clock, 100, Fx::from_int(1), open_space) ==
                      DispatchResult::INVALID_NPC);
        // Schedule pointing at an undefined tag: IDLE, no crash.
        const ScheduleEntry lost[] = {{0, 1440, 999, uint8_t(SchedBehavior::STAY)}};
        L3D_REQUIRE(sched.set(n, lost, 1));
        L3D_REQUIRE(npc_dispatch_step(npcs, transforms, sched, regions, 3, n,
                                      clock, 100, Fx::from_int(1), open_space) ==
                      DispatchResult::NO_REGION);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::IDLE));
        // NPC without any schedule: IDLE.
        L3D_REQUIRE(sched.clear(n));
        L3D_REQUIRE(npc_dispatch_step(npcs, transforms, sched, regions, 3, n,
                                      clock, 100, Fx::from_int(1), open_space) ==
                      DispatchResult::NO_SCHEDULE);
        L3D_REQUIRE(npcs.get(n)->state == uint8_t(NPCState::IDLE));
    }

    L3D_REQUIRE(npc_dispatch_selfcheck());

    std::printf("npc-dispatch OK\n");
    return 0;
}
