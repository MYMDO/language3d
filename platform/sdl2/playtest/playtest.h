#pragma once
// Phase 16B: integrated playtest & QA core (desktop-only).
//
// Layering: this module drives PUBLIC gameplay/API surfaces (Scenario,
// engines, save codec) and never the reverse — GameCore, renderer and
// platform backends include nothing from here. Production builds work
// identically with this translation unit unlinked.
//
// All storage is fixed-capacity (no heap, no STL containers); all runs
// are deterministic functions of (fixture, ops) with no wall-clock input.
#include "../../../engine/core.h"

#include <cstddef>
#include <cstdint>

namespace l3d {
namespace playtest {

constexpr size_t LOG_CAP = 256;
constexpr size_t REPORT_PATH_MAX = 128;
constexpr size_t STATE_TEXT_MAX = 512;
constexpr size_t SCENARIO_ID_MAX = 32;
constexpr uint32_t REPLAY_VERSION = 1;

// Structured event types. Logged by the runner/scenarios around real
// gameplay calls — engines themselves stay log-free.
enum class EventType : uint8_t {
    NONE = 0,
    GAME_START,
    GAME_RESET,
    PLAYER_MOVE,
    NPC_INTERACT,
    DIALOGUE_START,
    DIALOGUE_CHOICE,
    RESPONSE_EVALUATED,
    LANGUAGE_EVENT,
    VARIANT_SELECTED,
    QUEST_STATE_CHANGED,
    QUEST_OBJECTIVE_CHANGED,
    ITEM_ADDED,
    ITEM_REMOVED,
    SAVE,
    LOAD,
    SCENARIO_START,
    SCENARIO_END,
    ASSERTION_PASS,
    ASSERTION_FAIL,
    COUNT
};

const char* event_name(EventType t);

struct Event {
    uint8_t type{uint8_t(EventType::NONE)};
    uint16_t a{0};
    uint16_t b{0};
    uint16_t c{0};
};

// Fixed-capacity ring buffer. Oldest entries are overwritten past capacity
// (bounded by construction); hash() covers logical content order.
struct EventLog {
    Event events[LOG_CAP]{};
    size_t head{0}; // next write slot
    size_t count{0}; // entries present, <= LOG_CAP
    uint32_t seq{0}; // total events ever logged

    void clear();
    void log(EventType t, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0);
    // i-th oldest entry (0 = oldest). Returns false when out of range.
    bool at(size_t i, Event& out) const;
    uint32_t hash() const; // FNV-1a over logical content, deterministic
};

// Assertion outcome. Every check logs ASSERTION_PASS/FAIL with its order.
struct AssertCtx {
    EventLog* log{nullptr};
    uint32_t order{0};
    uint32_t failed{0};
    char last_message[128]{};
};

void assert_begin(AssertCtx& ctx, EventLog& log);
bool assert_true(AssertCtx& ctx, bool cond, const char* what);
bool assert_eq_u32(AssertCtx& ctx, uint32_t actual, uint32_t expected,
                   const char* what);
// Gameplay-level helpers (thin wrappers over the generic core).
bool assert_dialogue_state(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                           const char* what);
bool assert_verdict(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                    const char* what);
bool assert_quest_state(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                        const char* what);

// One deterministic scenario: fixed id, human name, entry point.
// Returns true on full pass; fills state_text with the final key state.
struct ScenarioDesc {
    char id[SCENARIO_ID_MAX]{};
    char name[64]{};
    bool (*run)(AssertCtx& ctx, EventLog& log, char* state_text,
                size_t state_cap, uint32_t seed){nullptr};
};

constexpr size_t MAX_SCENARIOS = 12;

struct Registry {
    ScenarioDesc items[MAX_SCENARIOS]{};
    size_t count{0};
    bool add(const ScenarioDesc& d);
    const ScenarioDesc* find(const char* id) const;
};

Registry& registry();
bool register_scenario(const char* id, const char* name,
                       bool (*run)(AssertCtx&, EventLog&, char*, size_t,
                                   uint32_t));

// Registers the 8 built-in gameplay scenarios (defined in scenarios.cpp).
bool register_all_scenarios();

// Run one scenario fresh (clears log first). On success, out_hash holds
// the event-log hash and state_text the final state. fail_msg_out (if
// non-null) receives the last failing assertion message, if any.
bool run_scenario(const ScenarioDesc& d, EventLog& log, uint32_t seed,
                  uint32_t& out_hash, char* state_text, size_t state_cap,
                  uint32_t& out_failed, char* fail_msg_out = nullptr,
                  size_t fail_cap = 0);
// Run twice and require identical hashes (determinism proof).
bool run_scenario_twice(const ScenarioDesc& d, uint32_t seed,
                        uint32_t& out_hash, char* state_text,
                        size_t state_cap, uint32_t& out_failed,
                        char* fail_msg_out = nullptr, size_t fail_cap = 0);

// Replay file (.l3dr): versioned, bounded, portable text.
struct ReplayFile {
    uint32_t version{REPLAY_VERSION};
    char scenario[SCENARIO_ID_MAX]{};
    uint32_t seed{0};
    uint32_t expected_hash{0};
};

bool replay_save(const char* path, const ReplayFile& rf);
bool replay_load(const char* path, ReplayFile& rf);
// Re-execute and compare against the recorded hash.
bool replay_verify(const ReplayFile& rf, uint32_t& actual_hash,
                   uint32_t& out_failed);

// Bug-report bundle: report.txt + events.log + scenario.txt + state.txt +
// replay.l3dr, all deterministic text. Returns false on I/O failure.
bool write_report(const char* dir, const char* scenario_id, uint32_t seed,
                  bool passed, uint32_t failed, const char* fail_message,
                  const EventLog& log, const char* state_text);

} // namespace playtest
} // namespace l3d
