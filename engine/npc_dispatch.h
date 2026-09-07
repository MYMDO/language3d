#pragma once
// Roadmap Phase 6A: schedule-driven NPC movement.
//
// This module is the ONLY place where GameClock + SchedulePool + NPCPool +
// TransformPool + World3 regions meet. The four subsystems stay decoupled:
// dispatch reads the active schedule entry, resolves its locationTag to a
// Region3, steers the NPC toward the region center with the existing
// kinematic update(), and reports arrival — all in fixed-point, driven
// purely by dt_ms (never by render framerate).
//
// Split responsibilities (never mixed into NPCAgent):
//   schedule resolution  -> npc_active_entry()
//   target selection     -> resolve_region_target()
//   movement             -> NPCPool::update() (unchanged)
//   arrival detection    -> NPCPool state machine (ARRIVED/IDLE)
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "schedule.h"
#include "world3.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

enum class DispatchResult : uint8_t {
    OK = 0,       // en route or already arrived (see NPC state)
    NO_SCHEDULE,  // NPC has no entry covering now -> commanded to IDLE
    NO_REGION,    // locationTag matches no region -> commanded to IDLE
    INVALID_NPC   // stale/unknown NPC id
};

// Active schedule entry for an NPC id at the clock's minute, or nullptr.
template <size_t SN, size_t SE>
const ScheduleEntry* npc_active_entry(const SchedulePool<SN, SE>& sched,
                                      uint16_t npc, const GameClock& clock) {
    return sched.lookup(npc, clock.minute);
}

// Region center (X/Y) for a location tag; Z is taken from the given feet
// point so the target is always reachable by X/Y kinematic movement and
// height is never lost. First region with a matching contentTag wins.
inline bool resolve_region_target(const Region3* regions, size_t count,
                                  uint16_t tag, const Vec3& feet, Vec3* out) {
    if (!regions || !out) return false;
    for (size_t i = 0; i < count; ++i) {
        if (regions[i].contentTag != tag) continue;
        const AABB3& b = regions[i].box;
        // Exact raw halving: no float, no division routine needed.
        out->x = Fx::from_raw((b.mn.x.raw + b.mx.x.raw) / 2);
        out->y = Fx::from_raw((b.mn.y.raw + b.mx.y.raw) / 2);
        out->z = feet.z;
        return true;
    }
    return false;
}

inline bool same_xy(const Vec3& a, const Vec3& b) {
    return a.x.raw == b.x.raw && a.y.raw == b.y.raw;
}

// One dispatch step for a single NPC: resolve schedule -> region target ->
// (re)command on transition -> advance. cruiseSpeed applies to newly
// commanded legs. Stateless across calls except through the NPC record
// itself (target comparison detects schedule transitions).
template <size_t NN, size_t MN, size_t SN, size_t SE, typename BlockedFn>
DispatchResult npc_dispatch_step(NPCPool<NN>& npcs, TransformPool<MN>& tpool,
                                 const SchedulePool<SN, SE>& sched,
                                 const Region3* regions, size_t regionCount,
                                 uint16_t npc, const GameClock& clock,
                                 uint32_t dt_ms, Fx cruiseSpeed,
                                 BlockedFn blocked) {
    const NPCAgent* cur = npcs.get(npc);
    if (!cur) return DispatchResult::INVALID_NPC;
    const ScheduleEntry* entry = npc_active_entry(sched, npc, clock);
    if (!entry) {
        npcs.command_stop(npc);
        return DispatchResult::NO_SCHEDULE;
    }
    const Transform3* tr = tpool.get(cur->entity);
    Vec3 feet{};
    if (tr) feet = tr->pos;
    Vec3 target{};
    if (!resolve_region_target(regions, regionCount, entry->locationTag, feet, &target)) {
        npcs.command_stop(npc);
        return DispatchResult::NO_REGION;
    }
    // Already arrived and schedule unchanged: hold ARRIVED, skip update
    // (update() with no target would fold back to IDLE).
    if (!cur->hasTarget && same_xy(cur->target, target))
        return DispatchResult::OK;
    if (!cur->hasTarget || !same_xy(cur->target, target)) {
        if (!npcs.command_move(npc, target, cruiseSpeed))
            return DispatchResult::INVALID_NPC;
    }
    npcs.update(tpool, npc, dt_ms, blocked);
    return DispatchResult::OK;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool npc_dispatch_selfcheck();

} // namespace l3d
