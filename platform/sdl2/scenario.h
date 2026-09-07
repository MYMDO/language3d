#pragma once
// Playable-scenario orchestration (desktop-only, no SDL includes:
// fully unit-testable). One generic runtime driven by a ScenarioDef;
// per-scenario differences (NPC roster, quest, regions, save file) are
// data, never branches. Owns the slice runtime — entities, NPCs, schedule,
// quest log, player state, inventory, language profile, clock, sessions —
// and exposes per-frame hooks for the game loop plus a save file.
//
// Slice contract (documented simplifications for playability):
// - NPCs with schedules dispatch on them (solid-aware); static NPCs stand.
//   Positions come from scenario transforms; the loop mirrors them into
//   Game sprites (no Game changes).
// - E near an NPC opens dialogue (consumed: the Game never sees it);
//   E near a GIVE recipient while holding the item reports directly.
// - REACH/COLLECT objectives auto-report from position/inventory each tick;
//   TALK completes through the dialogue fan-out (talk twice, as designed).
// - Completed quests auto-claim (rewards + message).
// - Variant groups pick harder dialogue once mastery qualifies. Save
//   persists progress; reload keeps it: the closed learning loop.
#include "../../engine/npc_dispatch.h"
#include "../../engine/interact.h"
#include "../../engine/dialogue_quest.h"
#include "../../engine/language.h"
#include "../../engine/save.h"
#include "../../engine/font.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

// One NPC bound by content tag. Schedules are optional: schedCount == 0
// means a static NPC (valid, exercised by the clerk).
struct ScenarioNPCDef {
    uint16_t tag{0};          // content NPC tag (TALK/GIVE matching, prompts)
    const char* name{nullptr}; // display name (prompts, speaker labels)
    uint16_t dialogue{0};     // fallback dialogue id
    uint16_t variantGroup{0}; // 0 = no variants
    uint8_t archetype{uint8_t(NPCArchetype::UNKNOWN)};
    int16_t cellX{0};
    int16_t cellY{0}; // spawn cell (centered at runtime)
    ScheduleEntry sched[2]{};
    uint8_t schedCount{0};
};

// Region box in map cells (built into Region3 at init).
struct ScenarioRegionDef {
    int16_t x0{0};
    int16_t y0{0};
    int16_t x1{0};
    int16_t y1{0};
    uint16_t tag{0};
};

struct ScenarioDef {
    const char* id{nullptr};   // "station" (CLI + save filename)
    const char* saveFile{nullptr};
    uint16_t quest{0}; // quest id (0 = none)
    ScenarioNPCDef npcs[4]{};
    uint8_t npcCount{0};
    ScenarioRegionDef regions[4]{};
    uint8_t regionCount{0};
};

// Generated content banks (implemented by the build-generated tables).
const DialogueBank content_dialogues();
const QuestBank content_quests();
const ItemBank content_items();
const VocabularyBank content_vocabulary();

// Built-in scenario definitions (defined in scenario.cpp).
const ScenarioDef* findScenarioDef(const char* id);
size_t scenarioDefCount();
const ScenarioDef* scenarioDefAt(size_t i);

class Scenario {
  public:
    bool init(const DialogueBank* db, const QuestBank* qb,
              const ItemBank* ib, const VocabularyBank* vb,
              const ScenarioDef* def);
    // Per-frame: clock, NPC dispatch (solid-blocked), quest pump, messages.
    // solid(ctx,x,y): true = blocked cell (e.g. Game::solid).
    void tick(uint32_t dtMs, uint32_t nowMs, bool (*solid)(void*, int32_t, int32_t),
              void* ctx);

    void setPlayer(const Transform3& tr) {
        playerTr_ = tr;
        refreshTexts();
    }
    // Sprite anchors for the loop to mirror into Game sprites.
    size_t npcCount() const { return npcCount_; }
    Vec2 npcPos(size_t i) const;
    uint16_t npcIdByTag(uint16_t tag) const;
    const char* saveFile() const {
        return (def_ && def_->saveFile) ? def_->saveFile : "language3d.save";
    }

    // E edge / answer keys. Return true when consumed (loop must hide the
    // key from the Game in that case).
    bool pressE(uint32_t nowMs);
    bool pressAnswer(int n);
    bool saveGame(const char* path);
    bool loadGame(const char* path);

    bool dialogueOpen() const;
    void renderPanel(uint8_t* px, uint16_t w, uint16_t h, uint32_t stride) const;
    const char* objectiveText() const;
    const char* promptText() const;
    const char* message() const;
    // Data-driven variant selection for an NPC tag (fallback when the tag
    // has no variant group).
    uint16_t dialogueFor(uint16_t tag) const;

    // Test access.
    const QuestLog<8>& quests() const { return log_; }
    const LanguageProfile<32>& language() const { return lang_; }
    const PlayerState<8>& player() const { return player_; }
    const NPCPool<8>& npcs() const { return npcs_; }

  private:
    const DialogueBank* dbank_{nullptr};
    const QuestBank* qbank_{nullptr};
    const ItemBank* ibank_{nullptr};
    const VocabularyBank* vbank_{nullptr};
    const ScenarioDef* def_{nullptr};
    EntityPool<16> entities_{};
    TransformPool<16> transforms_{};
    NPCPool<8> npcs_{};
    SchedulePool<8> sched_{};
    Region3 regions_[4]{};
    QuestLog<8> log_{};
    PlayerState<8> player_{};
    Inventory<8> inv_{};
    LanguageProfile<32> lang_{};
    GameClock clock_{};
    InteractSession isession_{};
    DialogueSession dsession_{};
    Transform3 playerTr_{};
    uint16_t npcIds_[4]{0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu};
    uint8_t npcCount_{0};
    bool showFinal_{false};
    char message_[96]{};
    uint32_t messageUntil_{0};
    uint32_t nowMs_{0};
    char objective_[128]{};
    char prompt_[64]{};

    void say(const char* text);
    void refreshTexts();
    void pumpQuests();
    const ScenarioNPCDef* npcDefByTag(uint16_t tag) const;
    const ScenarioNPCDef* npcDefByIndex(size_t i) const;
};
} // namespace l3d
