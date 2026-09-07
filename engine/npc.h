#pragma once
// Roadmap Phase 4: NPC foundation (no dialogue/quests/AI/schedules yet).
//
// An NPC agent is a thin record bound to an EXISTING Entity: it never
// duplicates entity identity or Transform3 storage. (Named NPCAgent — not
// NPC — because core.h already owns the legacy 2D single-NPC game state;
// the two will converge when gameplay migrates to entities.) Movement is a minimal deterministic
// kinematic model (IDLE/MOVING/BLOCKED/ARRIVED) over the entity's transform;
// collision input arrives as an owner-provided predicate, so this unit stays
// decoupled from any particular world representation (a future phase will
// bind it to walk_move/HeightField).
//
// Future extension points (reserved, not implemented): dialogue trees,
// quest-giver flags, inventory handles, schedules, language profiles — each
// becomes its own fixed pool referencing the NPC id, mirroring how NPC
// references the Entity id today.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "transform3.h"
#include "entity.h" // id helpers + TransformPool (no EntityPool dereference)

#include <cstddef>
#include <cstdint>

namespace l3d {

// Small archetype tag for content binding (behavior lives in later systems).
enum class NPCArchetype : uint8_t {
    UNKNOWN = 0,
    RESIDENT,
    SHOPKEEPER,
    CLERK,
    COUNT
};

enum class NPCState : uint8_t {
    INACTIVE = 0, // slot free
    IDLE,         // alive, no active order
    MOVING,       // advancing toward target
    BLOCKED,      // owner predicate denied the step
    ARRIVED       // reached the commanded target
};

constexpr uint16_t L3D_NPC_INVALID = 0xFFFFu;
// Recommended default budget (fits RP2040 SRAM, see npc-system.md).
constexpr size_t L3D_NPC_BUDGET = 16;

struct NPCAgent {
    uint16_t entity{L3D_ENTITY_INVALID}; // generational Entity id (owner-validated)
    uint8_t archetype{uint8_t(NPCArchetype::UNKNOWN)};
    uint8_t state{uint8_t(NPCState::INACTIVE)};
    Vec3 target{};    // commanded destination (X/Y used; Z untouched)
    bool hasTarget{false};
    Fx speed{};       // world units per second
    uint16_t nameTag{0}; // opaque content id (future name/dialogue tables)
};

template <size_t N>
struct NPCPool {
    static_assert(N >= 1 && N <= 256, "npc pool capacity must fit id index bits");

    struct Slot {
        NPCAgent npc{};
        uint8_t gen{0};
        bool alive{false};
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

    // Bind to an already-spawned Entity id. The pool stores the id but never
    // dereferences the EntityPool: liveness of the entity stays the owner's
    // responsibility (detach NPC before destroying its entity).
    uint16_t spawn(uint16_t entity, NPCArchetype archetype) {
        if (free_head == 0xFFFFu || entity == L3D_ENTITY_INVALID) return L3D_NPC_INVALID;
        const size_t i = free_head;
        Slot& s = slots[i];
        free_head = s.free_next;
        uint8_t g = s.gen;
        if (g == 0) g = 1;
        s = Slot{};
        s.gen = g;
        s.alive = true;
        s.npc.entity = entity;
        s.npc.archetype = uint8_t(archetype);
        s.npc.state = uint8_t(NPCState::IDLE);
        ++alive_count;
        return entity_make_id(g, i);
    }

    bool destroy(uint16_t id) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
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

    const NPCAgent* get(uint16_t id) const {
        const Slot* s = slot(id);
        return s ? &s->npc : nullptr;
    }

    size_t live_count() const { return alive_count; }
    bool full() const { return free_head == 0xFFFFu; }
    bool empty() const { return alive_count == 0; }

    // Order a live NPC toward a destination at speed units/sec.
    bool command_move(uint16_t id, const Vec3& target, Fx speed) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
        s->npc.target = target;
        s->npc.hasTarget = true;
        s->npc.speed = speed;
        s->npc.state = uint8_t(NPCState::MOVING);
        return true;
    }

    bool command_stop(uint16_t id) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
        s->npc.hasTarget = false;
        s->npc.state = uint8_t(NPCState::IDLE);
        return true;
    }

    // Advance a live NPC by dt_ms using its entity's transform from tpool.
    // blocked(from, to) is consulted per axis step; when it denies motion
    // the NPC keeps its position and reports BLOCKED. Z is preserved
    // (height integration arrives with the physics phase).
    template <typename BlockedFn, size_t M>
    bool update(TransformPool<M>& tpool, uint16_t id, uint32_t dt_ms,
                BlockedFn blocked) {
        Slot* s = mutable_slot(id);
        if (!s) return false;
        const Transform3* cur = tpool.get(s->npc.entity);
        if (!cur) {
            s->npc.state = uint8_t(NPCState::IDLE);
            return true; // no transform bound yet: wait, don't fail
        }
        if (!s->npc.hasTarget) {
            s->npc.state = uint8_t(NPCState::IDLE);
            return true;
        }
        // Per-axis clamped step: exact arrival, direction-independent.
        const Fx step = Fx::from_raw(
            int32_t((int64_t(s->npc.speed.raw) * int64_t(dt_ms)) / 1000));
        bool moved_any = false;
        bool denied = false;
        Vec3 pos = cur->pos;
        for (int axis = 0; axis < 2; ++axis) {
            const Fx want = (axis == 0) ? s->npc.target.x : s->npc.target.y;
            const Fx at = (axis == 0) ? pos.x : pos.y;
            if (want.raw == at.raw) continue;
            Fx next = want;
            const int32_t d = want.raw - at.raw;
            if (d > step.raw) next = Fx::from_raw(at.raw + step.raw);
            else if (d < -step.raw) next = Fx::from_raw(at.raw - step.raw);
            Vec3 probe = pos;
            if (axis == 0) probe.x = next;
            else probe.y = next;
            if (blocked(pos, probe)) {
                denied = true;
                continue;
            }
            pos = probe;
            moved_any = true;
        }
        Transform3 moved = *cur;
        moved.pos = pos;
        tpool.attach(s->npc.entity, moved);
        if (pos.x.raw == s->npc.target.x.raw && pos.y.raw == s->npc.target.y.raw) {
            s->npc.state = uint8_t(NPCState::ARRIVED);
            s->npc.hasTarget = false;
        } else if (denied && !moved_any) {
            s->npc.state = uint8_t(NPCState::BLOCKED);
        } else {
            s->npc.state = uint8_t(NPCState::MOVING);
        }
        return true;
    }

    template <typename Fn>
    void for_each_active(Fn&& fn) const {
        for (size_t i = 0; i < N; ++i) {
            const Slot& s = slots[i];
            if (s.alive) fn(entity_make_id(s.gen, i), s.npc);
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
        return const_cast<Slot*>(static_cast<const NPCPool*>(this)->slot(id));
    }
};

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool npc_selfcheck();

} // namespace l3d
