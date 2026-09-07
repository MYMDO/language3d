#pragma once
// Roadmap Phase 8: data-driven dialogue foundation (no quests/inventory/
// language-profile evaluation yet).
//
// Dialogue CONTENT lives outside C++ (content/dialogues/*.dlg compiled by
// tools/build_dialogue.py into static tables). This header defines the
// zero-copy views, the runtime validator, and the session state machine.
// Language metadata (target language, CEFR, vocabulary/grammar tags) is
// carried on every node from day one but is INERT: nothing in the engine
// scores or branches on it yet — it is a clean extension point for the
// language phase, so that phase will not reshape this subsystem.
//
// DialogueSession is deliberately separate from InteractSession (active /
// completed / aborted vs active / ended): walking away aborts dialogue
// without destroying the interaction record.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t DIALOGUE_NONE = 0xFFFFu; // no dialogue / terminal choice
constexpr size_t DIALOGUE_MAX_NODES = 16;
constexpr size_t DIALOGUE_MAX_CHOICES = 4;
constexpr size_t DIALOGUE_MAX_TAGS = 4;
constexpr size_t DIALOGUE_MAX_TEXT = 192;

// Target language codes. Engine-agnostic: Ukrainian->X avaliable by adding
// rows here; gameplay never branches on the value (yet).
enum class DialogueLang : uint8_t {
    UNDEFINED = 0,
    EN,
    DE,
    PL,
    ES,
    FR,
    COUNT
};

// CEFR levels, 0 = undefined.
enum class DialogueCEFR : uint8_t {
    UNDEFINED = 0,
    A1,
    A2,
    B1,
    B2,
    C1,
    C2
};

enum class DialogueSpeaker : uint8_t { NPC = 0, PLAYER, NARRATOR, COUNT };

enum class DialogueState : uint8_t { EMPTY = 0, ACTIVE, COMPLETED, ABORTED };

// Condition kinds mirror QuestCondition layouts (p1/p2 meanings match),
// so the language/quest phases share one mental model.
enum class DialogueCondKind : uint8_t {
    NONE = 0,
    HAS_ITEM,  // p1 = item id, p2 = count
    FLAG_SET,  // p1 = bit
    COUNTER_GE, // p1 = index, p2 = threshold
    LEVEL_GE,  // p1 = level
    COUNT
};

// Effect kinds mirror QuestReward layouts. Applied by the orchestration
// layer (engine/dialogue_quest.h), never by the dialogue engine itself:
// the dialogue unit stays unaware of quests/inventory by construction.
enum class DialogueEffectKind : uint8_t {
    NONE = 0,
    GIVE_ITEM,  // p1 = item id, p2 = count (returns leftover)
    SET_FLAG,   // p1 = bit
    ADD_COUNTER, // p1 = index, p2 = amount
    ADD_XP,     // p1 = amount
    START_QUEST, // p1 = quest id
    COUNT
};

struct DialogueCond {
    uint8_t kind{uint8_t(DialogueCondKind::NONE)};
    uint16_t p1{0};
    uint16_t p2{0};
};

struct DialogueEffect {
    uint8_t kind{uint8_t(DialogueEffectKind::NONE)};
    uint16_t p1{0};
    uint16_t p2{0};
};

// Inert on this phase: carried, validated for range, never evaluated.
struct DialogueLangMeta {
    uint8_t lang{uint8_t(DialogueLang::UNDEFINED)};
    uint8_t cefr{uint8_t(DialogueCEFR::UNDEFINED)};
    uint16_t vocab[DIALOGUE_MAX_TAGS]{};
    uint8_t vocab_count{0};
    uint16_t grammar[DIALOGUE_MAX_TAGS]{};
    uint8_t grammar_count{0};
};

struct DialogueChoice {
    const char* text{nullptr};
    uint16_t next{DIALOGUE_NONE}; // node id, or NONE = dialogue ends here
    // Language semantics of the response (all opaque content ids, 0 = none).
    // intent: what the player means (e.g. request-ticket). The entry node
    // declares which intents it accepts; evaluation matches them instead of
    // trusting the choice position. vocab/grammar: what the response
    // practices (recorded on use + verdict).
    uint16_t intent{0};
    uint16_t vocab[DIALOGUE_MAX_TAGS]{};
    uint8_t vocab_count{0};
    uint16_t grammar[DIALOGUE_MAX_TAGS]{};
    uint8_t grammar_count{0};
};

struct DialogueNode {
    uint16_t id{0};
    uint8_t speaker{uint8_t(DialogueSpeaker::NPC)};
    uint8_t choice_count{0};
    DialogueChoice choices[DIALOGUE_MAX_CHOICES]{};
    const char* text{nullptr};
    DialogueLangMeta lang{};
    DialogueCond cond{};     // entry requirement (orchestration evaluates)
    DialogueEffect effect{}; // applied on entry (orchestration applies)
    // Expected response intents (0 = node declares none -> legacy USED-only
    // path, no verdict). primary match = CORRECT, secondary = PARTIAL,
    // otherwise INCORRECT.
    uint16_t expect_primary{0};
    uint16_t expect_secondary{0};
};

struct DialogueDef {
    uint16_t id{0};
    uint16_t npc_tag{0}; // opaque content id (NPCAgent::nameTag namespace)
    uint8_t node_count{0};
    const DialogueNode* nodes{nullptr};
    // Adaptive variant group (0 = standalone, legacy behavior). Dialogues
    // sharing a group are interchangeable presentations of one conversation;
    // selection picks the highest required mastery the profile meets, with
    // the requirement-free entry as the guaranteed fallback (see
    // dq_select_variant). require_word 0 = no requirement.
    uint16_t variant_group{0};
    uint16_t require_word{0};   // vocabulary id (shared namespace)
    uint8_t require_mastery{0}; // Mastery value required on that word
};

struct DialogueBank {
    const DialogueDef* defs{nullptr};
    size_t count{0};
};

struct DialogueSession {
    uint16_t dialogue{DIALOGUE_NONE};
    uint16_t node{DIALOGUE_NONE};
    uint16_t npc{DIALOGUE_NONE}; // runtime NPC id handoff (owner-validated)
    uint8_t state{uint8_t(DialogueState::EMPTY)};
};

// Structural validation of one definition (unique node ids, choice targets
// resolve or NONE, counts/text within caps, enums in range). Returns true
// when clean; optionally reports the first offending node id.
bool dialogue_validate(const DialogueDef& def, uint16_t* bad_node);
// Bank-level validation (unique dialogue ids + per-def validation).
bool dialogue_validate_bank(const DialogueBank& bank, uint16_t* bad_def);

const DialogueDef* dialogue_find(const DialogueBank& bank, uint16_t id);
const DialogueNode* dialogue_find_node(const DialogueDef& def, uint16_t node);

// Open a session on a dialogue for an NPC runtime id. Rejects unknown
// dialogues and sentinel NPC ids; NPC liveness stays the owner's job
// (same discipline as Entity/NPC pools).
bool dialogue_begin(DialogueSession& s, const DialogueBank& bank,
                    uint16_t dialogue, uint16_t npc);
// Advance by player choice. Terminal choice (NONE) or arrival at a
// choiceless node completes the session. Rejects bad indices and
// non-ACTIVE sessions.
bool dialogue_choose(DialogueSession& s, const DialogueBank& bank,
                     size_t choice);
// Explicit close (any state -> EMPTY) and abort (ACTIVE -> ABORTED).
void dialogue_end(DialogueSession& s);
bool dialogue_abort(DialogueSession& s);

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool dialogue_selfcheck();

} // namespace l3d
