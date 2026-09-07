#pragma once
// Roadmap Phase 7: NPC interaction (proximity + explicit action, no dialogue).
//
// Proves the player can walk up to a living NPC and interact: nearest
// in-front NPC within radius becomes the target; an explicit action edge
// (the E key, delivered by the caller from the Platform API) opens a
// session guarded by a cooldown. Result on this phase is only
// started/ended state — dialogue content arrives later and will consume
// the session's NPC id.
//
// Selection order (all in world coordinates, never rendering coordinates):
//   1. inside interaction radius (X/Y plane, squared fixed-point distance);
//   2. in the facing hemisphere (dot(forward, dir) > 0, exact, no sqrt);
//   3. lowest Entity id as the deterministic tie-break.
// Facing is a hemisphere gate rather than a continuous dot ranking: exact
// normalized-dot ordering would need a square root, and nearest+gated is
// the correct scope for "approach and press E". NPC iteration order never
// influences the result.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "npc.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t L3D_INTERACT_NONE = 0xFFFFu;

enum class InteractResult : uint8_t {
    NONE = 0,      // nothing happened (invalid target / idle end)
    STARTED,       // new session opened with the target NPC
    ALREADY,       // a session is already active
    COOLDOWN,      // action edge inside the debounce window
    NO_TARGET,     // action edge with no valid target
    ENDED          // active session closed
};

// Caller-owned session. now_ms comes from the caller's clock (platform
// ticks on device, scripted values in tests) — never from rendering.
struct InteractSession {
    uint16_t npc{L3D_INTERACT_NONE};
    uint32_t last_end_ms{0};
    uint32_t cooldown_ms{500};
    bool active{false};
    bool ever_started{false};
};

// Best interaction target for a player transform, or L3D_INTERACT_NONE.
// Scans live NPCs with bound transforms; see selection order above.
template <size_t NN, size_t MN>
uint16_t interact_target(const NPCPool<NN>& npcs, const TransformPool<MN>& tpool,
                         const Transform3& player, Fx radius) {
    const Vec2 fwd = yaw_forward(player.yaw);
    const int64_t r2 = int64_t(radius.raw) * int64_t(radius.raw);
    uint16_t best = L3D_INTERACT_NONE;
    int64_t best_d2 = 0;
    uint16_t best_entity = 0xFFFFu;
    npcs.for_each_active([&](uint16_t id, const NPCAgent& npc) {
        const Transform3* tr = tpool.get(npc.entity);
        if (!tr) return;
        const int64_t dx = int64_t(tr->pos.x.raw) - int64_t(player.pos.x.raw);
        const int64_t dy = int64_t(tr->pos.y.raw) - int64_t(player.pos.y.raw);
        const int64_t d2 = dx * dx + dy * dy;
        if (d2 > r2) return; // outside radius
        const int64_t dot = dx * int64_t(fwd.x.raw) + dy * int64_t(fwd.y.raw);
        if (dot <= 0) return; // behind the player
        if (best == L3D_INTERACT_NONE || d2 < best_d2 ||
            (d2 == best_d2 && npc.entity < best_entity)) {
            best = id;
            best_d2 = d2;
            best_entity = npc.entity;
        }
    });
    return best;
}

// Explicit action edge (E pressed). Opens a session or reports why not.
inline InteractResult interact_try(InteractSession& s, uint16_t target,
                                   uint32_t now_ms) {
    if (target == L3D_INTERACT_NONE) return InteractResult::NO_TARGET;
    if (s.active) return InteractResult::ALREADY;
    if (s.ever_started && now_ms - s.last_end_ms < s.cooldown_ms)
        return InteractResult::COOLDOWN;
    s.npc = target;
    s.active = true;
    s.ever_started = true;
    return InteractResult::STARTED;
}

// Close the active session.
inline InteractResult interact_end(InteractSession& s, uint32_t now_ms) {
    if (!s.active) return InteractResult::NONE;
    s.active = false;
    s.npc = L3D_INTERACT_NONE;
    s.last_end_ms = now_ms;
    return InteractResult::ENDED;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool interact_selfcheck();

} // namespace l3d
