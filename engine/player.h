#pragma once
// Roadmap Phase 10: minimal deterministic PlayerState (no quests, no
// language profile, no save files yet).
//
// The player IS an entity: PlayerState stores a generational Entity
// reference and never duplicates Transform3/identity. The inventory stays
// a separate ownership domain: PlayerState holds a NON-OWNING association
// (nullable pointer, any capacity) — never a copy. Future dialogue effects
// (give_item) and quest conditions (has_item) will operate through this
// association without reshaping either side.
//
// Runtime vs persistent split is structural from day one:
//   - PlayerState  = runtime record (bindings + progression working set).
//   - PlayerPersistent = plain versioned data (everything worth saving;
//     pointers never persist; render/interaction caches live elsewhere
//     and are not represented here at all).
// snapshot()/restore() convert between them; file I/O arrives with the
// save phase.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "entity.h"
#include "item.h" // Inventory association (one-directional; item.h knows nothing of player)

#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr size_t PLAYER_COUNTERS = 8;
constexpr size_t PLAYER_FLAGS = 32;
constexpr uint16_t PLAYER_SAVE_VERSION = 1;

// Placeholder progression curve: level = 1 + xp/1000, capped at 99.
// Deliberately trivial; the progression phase will replace it.
constexpr uint16_t player_level_for_xp(uint32_t xp) {
    uint16_t level = uint16_t(1 + (xp / 1000u));
    return level > 99 ? 99 : level;
}

template <size_t INV>
struct PlayerState {
    uint16_t entity{L3D_ENTITY_INVALID}; // generational Entity ref (owner-validated)
    Inventory<INV>* inventory{nullptr}; // non-owning association, may be null
    uint32_t xp{0};
    uint16_t level{1};
    int16_t reputation{0};
    uint32_t flags{0}; // 32 world flags for future quest/dialogue gating
    uint16_t counters[PLAYER_COUNTERS]{};

    void init(uint16_t entity_id) {
        *this = PlayerState{};
        entity = entity_id;
    }
    void bind_inventory(Inventory<INV>* inv) { inventory = inv; }
    void unbind() {
        entity = L3D_ENTITY_INVALID;
        inventory = nullptr;
    }
    bool bound() const { return entity != L3D_ENTITY_INVALID; }

    void reset_progression() {
        xp = 0;
        level = 1;
        reputation = 0;
        flags = 0;
        for (size_t i = 0; i < PLAYER_COUNTERS; ++i) counters[i] = 0;
    }

    void add_xp(uint32_t amount) {
        uint64_t total = uint64_t(xp) + uint64_t(amount);
        xp = total > 0xFFFFFFFFu ? 0xFFFFFFFFu : uint32_t(total);
        level = player_level_for_xp(xp);
    }

    bool set_flag(size_t bit) {
        if (bit >= PLAYER_FLAGS) return false;
        flags |= (1u << bit);
        return true;
    }
    bool clear_flag(size_t bit) {
        if (bit >= PLAYER_FLAGS) return false;
        flags &= ~(1u << bit);
        return true;
    }
    bool has_flag(size_t bit) const {
        if (bit >= PLAYER_FLAGS) return false;
        return (flags & (1u << bit)) != 0;
    }

    bool add_counter(size_t idx, uint16_t amount) {
        if (idx >= PLAYER_COUNTERS) return false;
        uint32_t total = uint32_t(counters[idx]) + uint32_t(amount);
        counters[idx] = total > 0xFFFFu ? 0xFFFFu : uint16_t(total);
        return true;
    }
    uint16_t counter(size_t idx) const {
        if (idx >= PLAYER_COUNTERS) return 0;
        return counters[idx];
    }
};

// Versioned persistent candidate: everything below may be saved;
// bindings (pointers) and caches never are (inventory persists through
// its own future serializer; entity ids rebind on load).
struct PlayerPersistent {
    uint16_t version{PLAYER_SAVE_VERSION};
    uint16_t entity{L3D_ENTITY_INVALID};
    uint32_t xp{0};
    uint16_t level{1};
    int16_t reputation{0};
    uint32_t flags{0};
    uint16_t counters[PLAYER_COUNTERS]{};
};

template <size_t INV>
PlayerPersistent player_snapshot(const PlayerState<INV>& p) {
    PlayerPersistent s{};
    s.entity = p.entity;
    s.xp = p.xp;
    s.level = p.level;
    s.reputation = p.reputation;
    s.flags = p.flags;
    for (size_t i = 0; i < PLAYER_COUNTERS; ++i) s.counters[i] = p.counters[i];
    return s;
}

template <size_t INV>
bool player_restore(PlayerState<INV>& p, const PlayerPersistent& s) {
    if (s.version != PLAYER_SAVE_VERSION) return false;
    // Load flow: pools are restored first, so the saved entity id is
    // meaningful again. The inventory pointer cannot persist — the
    // orchestrator rebinds it right after restore (preserved here so a
    // mid-session checkpoint round-trips without losing the association).
    Inventory<INV>* inv = p.inventory;
    p.reset_progression();
    p.entity = s.entity;
    p.inventory = inv;
    p.xp = s.xp;
    p.level = s.level;
    p.reputation = s.reputation;
    p.flags = s.flags;
    for (size_t i = 0; i < PLAYER_COUNTERS; ++i) p.counters[i] = s.counters[i];
    return true;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool player_selfcheck();

} // namespace l3d
