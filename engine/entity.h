#pragma once
// Roadmap Phase 3: lightweight deterministic entity/component foundation.
//
// Anti-monolith rule: Entity carries ONLY identity (id + kind + lifecycle).
// Transform and Collider live in separate fixed pools keyed by entity id
// with per-slot generation copies, so a stale id can never address another
// entity's components. NPC/Item/Quest/Dialogue/Inventory pools arrive in
// later phases with exactly the same pattern.
//
// Rules (shared with world3):
// - No heap, no exceptions, no RTTI, no virtual dispatch, no STL.
// - ID semantics: u16 id = (generation << 8) | slot_index. Pool capacity
//   N must satisfy 1 <= N <= 256 (static_assert). Generation 0 is skipped
//   on reuse so id 0 is never a live-again alias of a destroyed entity.
// - Iteration is deterministic: slot-index order, active entities only.
// - Pools are plain aggregates; the caller owns them (static storage on
//   RP2040, any storage on host).
#include "transform3.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

// Entity kinds. Only identity tags — behavior lives in future systems.
enum class EntityKind : uint8_t {
    UNKNOWN = 0,
    PLAYER,
    NPC,
    ITEM,
    DOOR,
    TRIGGER,
    DECORATION,
    OBJECT,
    COUNT
};

constexpr uint16_t L3D_ENTITY_INVALID = 0xFFFFu;
// Recommended default world budget (fits RP2040 SRAM, see entity-system.md).
constexpr size_t L3D_ENTITY_BUDGET = 64;

constexpr size_t entity_slot_index(uint16_t id) { return size_t(id & 0xFFu); }
constexpr uint8_t entity_generation(uint16_t id) {
    return uint8_t((id >> 8) & 0xFFu);
}
constexpr uint16_t entity_make_id(uint8_t gen, size_t index) {
    return uint16_t((uint16_t(gen) << 8) | uint16_t(index & 0xFFu));
}

template <size_t N>
struct EntityPool {
    static_assert(N >= 1 && N <= 256, "entity pool capacity must fit id index bits");

    struct Slot {
        uint8_t gen{0};
        uint8_t kind{uint8_t(EntityKind::UNKNOWN)};
        bool alive{false};
        bool enabled{true};
        uint16_t free_next{0xFFFFu};
    };

    Slot slots[N]{};
    uint16_t free_head{0xFFFFu}; // INVALID until init(): safe by default
    size_t alive_count{0};

    void init() {
        for (size_t i = 0; i < N; ++i) {
            slots[i] = Slot{};
            slots[i].free_next = (i + 1 < N) ? uint16_t(i + 1) : uint16_t(0xFFFFu);
        }
        free_head = 0;
        alive_count = 0;
    }

    // Spawn with the given kind. Returns L3D_ENTITY_INVALID when full.
    // Reused slots always carry a new (non-zero, changed) generation.
    uint16_t spawn(EntityKind kind) {
        if (free_head == 0xFFFFu) return L3D_ENTITY_INVALID;
        const size_t i = free_head;
        Slot& s = slots[i];
        free_head = s.free_next;
        uint8_t g = s.gen;
        if (g == 0) g = 1; // first life never uses generation 0
        s.gen = g;
        s.kind = uint8_t(kind);
        s.alive = true;
        s.enabled = true;
        s.free_next = 0xFFFFu;
        ++alive_count;
        return entity_make_id(g, i);
    }

    // Destroy. Returns false for stale/unknown ids (double-destroy safe).
    bool destroy(uint16_t id) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
        // Bump generation, skipping 0 so a wrapped id never aliases gen 0.
        uint8_t g = uint8_t(s->gen + 1u);
        if (g == 0) g = 1;
        const size_t i = entity_slot_index(id);
        *s = Slot{};
        s->gen = g;
        s->free_next = free_head;
        free_head = uint16_t(i);
        --alive_count;
        return true;
    }

    bool alive(uint16_t id) const { return slot(id) != nullptr; }

    bool active(uint16_t id) const {
        const Slot* s = slot(id);
        return s && s->enabled;
    }

    bool set_enabled(uint16_t id, bool on) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
        s->enabled = on;
        return true;
    }

    // Kind of a live entity; *ok=false for stale ids.
    EntityKind kind_of(uint16_t id, bool* ok) const {
        const Slot* s = slot(id);
        if (ok) *ok = (s != nullptr);
        return s ? EntityKind(s->kind) : EntityKind::UNKNOWN;
    }

    size_t live_count() const { return alive_count; }
    bool full() const { return free_head == 0xFFFFu; }
    bool empty() const { return alive_count == 0; }

    // Deterministic iteration: slot-index order, active entities only.
    template <typename Fn>
    void for_each_active(Fn&& fn) const {
        for (size_t i = 0; i < N; ++i) {
            const Slot& s = slots[i];
            if (s.alive && s.enabled) fn(entity_make_id(s.gen, i), EntityKind(s.kind));
        }
    }

  private:
    const Slot* slot(uint16_t id) const {
        const size_t i = entity_slot_index(id);
        if (i >= N) return nullptr;
        const Slot& s = slots[i];
        if (!s.alive || s.gen != entity_generation(id)) return nullptr;
        return &s;
    }
    Slot* mutable_slot(uint16_t id) {
        return const_cast<Slot*>(static_cast<const EntityPool*>(this)->slot(id));
    }
};

// Transform component pool: one Transform3 per entity slot, keyed by id
// with a generation copy, so a REUSED slot (new generation) never exposes
// the previous occupant's component. Ownership rule: the owner (future
// World registry) must detach() an entity's components before destroy();
// the generation copy guards reuse-aliasing, not owner discipline.
template <size_t N>
struct TransformPool {
    static_assert(N >= 1 && N <= 256, "transform pool capacity must fit id index bits");
    Transform3 items[N]{};
    uint8_t gens[N]{}; // 0 = never attached
    bool has[N]{};

    void attach(uint16_t id, const Transform3& t) {
        const size_t i = entity_slot_index(id);
        if (i >= N) return;
        items[i] = t;
        gens[i] = entity_generation(id);
        has[i] = true;
    }
    const Transform3* get(uint16_t id) const {
        const size_t i = entity_slot_index(id);
        if (i >= N || !has[i] || gens[i] != entity_generation(id)) return nullptr;
        return &items[i];
    }
    bool detach(uint16_t id) {
        const size_t i = entity_slot_index(id);
        if (i >= N || !has[i] || gens[i] != entity_generation(id)) return false;
        has[i] = false;
        gens[i] = 0;
        return true;
    }
};

// Collider component pool. The box is feet-anchored: horizontal half
// extents hx/hy plus full body height h above the feet point, i.e.
// box = {pos - (hx,hy,0), pos + (hx,hy,h)}. `solid` marks blocking
// volumes; non-solid volumes are sensor/trigger shapes.
struct Collider {
    Fx hx{};
    Fx hy{};
    Fx h{};
    bool solid{true};
};

template <size_t N>
struct ColliderPool {
    static_assert(N >= 1 && N <= 256, "collider pool capacity must fit id index bits");
    Collider items[N]{};
    uint8_t gens[N]{};
    bool has[N]{};

    void attach(uint16_t id, const Collider& c) {
        const size_t i = entity_slot_index(id);
        if (i >= N) return;
        items[i] = c;
        gens[i] = entity_generation(id);
        has[i] = true;
    }
    const Collider* get(uint16_t id) const {
        const size_t i = entity_slot_index(id);
        if (i >= N || !has[i] || gens[i] != entity_generation(id)) return nullptr;
        return &items[i];
    }
    bool detach(uint16_t id) {
        const size_t i = entity_slot_index(id);
        if (i >= N || !has[i] || gens[i] != entity_generation(id)) return false;
        has[i] = false;
        gens[i] = 0;
        return true;
    }
};

// Feet-anchored world box of an entity's collider. Returns false for stale
// ids or missing colliders.
template <size_t N>
bool collider_world_box(const ColliderPool<N>& pool, uint16_t id,
                        const Transform3& tr, AABB3* out) {
    const Collider* c = pool.get(id);
    if (!c || !out) return false;
    out->mn = Vec3{tr.pos.x - c->hx, tr.pos.y - c->hy, tr.pos.z};
    out->mx = Vec3{tr.pos.x + c->hx, tr.pos.y + c->hy, tr.pos.z + c->h};
    return true;
}

// Deterministic self-check (no I/O, no heap). See world3_selfcheck().
bool entity_selfcheck();

} // namespace l3d
