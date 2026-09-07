#pragma once
// Phase 14 vertical slice orchestration (desktop-only, no SDL includes:
// fully unit-testable). Owns the slice runtime — entities, NPCs, schedule,
// quest log, player state, inventory, language profile, clock, sessions —
// and exposes per-frame hooks for the game loop plus a save file.
//
// Slice contract (documented simplifications for playability):
// - Anna is anchored at the Game NPC cell; her schedule/dispatch machinery
//   runs (living state), the clerk is static. Positions come from scenario
//   transforms; the loop mirrors them into Game sprites (no Game changes).
// - E near an NPC opens dialogue (consumed: the Game never sees it);
//   E near the clerk while holding the ticket for GIVE reports directly.
// - REACH/COLLECT objectives auto-report from position/inventory each tick;
//   TALK completes through the dialogue fan-out (talk twice, as designed).
// - Completed quests auto-claim (rewards + message).
// - Anna greets with dialogue 3 once station mastery reaches FAMILIAR,
//   else dialogue 1. Save persists progress; reload flips the greeting:
//   the closed learning loop, playable.
#include "../../engine/npc_dispatch.h"
#include "../../engine/interact.h"
#include "../../engine/dialogue_quest.h"
#include "../../engine/language.h"
#include "../../engine/save.h"
#include "../../engine/font.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

// Generated content banks (implemented by the build-generated tables).
const DialogueBank content_dialogues();
const QuestBank content_quests();
const ItemBank content_items();
const VocabularyBank content_vocabulary();

// Slice layout (map cells, all verified open in assets/map.txt):
//   Anna anchor / MARKET plaza (18,18), HOME nook (16..17,17..18),
//   clerk booth (19,17), station district (16..20,16..20) tag 3.
constexpr uint16_t SLICE_ANNA_TAG = 1;
constexpr uint16_t SLICE_CLERK_TAG = 2;
constexpr uint16_t SLICE_STATION_TAG = 3;
constexpr uint16_t SLICE_MARKET_TAG = 10;
constexpr uint16_t SLICE_HOME_TAG = 11;
constexpr uint16_t SLICE_ANNA_DIALOGUE = 1;
constexpr uint16_t SLICE_ANNA_VARIANT_GROUP = 1;
constexpr uint16_t SLICE_CLERK_DIALOGUE = 2;
constexpr uint16_t SLICE_QUEST = 1;
constexpr uint16_t SLICE_TICKET = 2;
constexpr uint32_t SLICE_SAVE_VERSION_GUARD = 1;

class Scenario {
  public:
    bool init(const DialogueBank* db, const QuestBank* qb,
              const ItemBank* ib, const VocabularyBank* vb);
    // Per-frame: clock, Anna dispatch (solid-blocked), quest pump, messages.
    // solid(ctx,x,y): true = blocked cell (e.g. Game::solid).
    void tick(uint32_t dtMs, uint32_t nowMs, bool (*solid)(void*, int32_t, int32_t),
              void* ctx);

    void setPlayer(const Transform3& tr) {
        playerTr_ = tr;
        refreshTexts();
    }
    // Sprite anchors for the loop to mirror into Game sprites.
    Vec2 annaPos() const;
    Vec2 clerkPos() const;

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
    uint16_t annaDialogue() const;

    // Test access.
    const QuestLog<8>& quests() const { return log_; }
    const LanguageProfile<32>& language() const { return lang_; }
    const PlayerState<8>& player() const { return player_; }
    const NPCPool<8>& npcs() const { return npcs_; }
    uint16_t annaId() const { return anna_; }
    uint16_t clerkId() const { return clerk_; }

  private:
    const DialogueBank* dbank_{nullptr};
    const QuestBank* qbank_{nullptr};
    const ItemBank* ibank_{nullptr};
    const VocabularyBank* vbank_{nullptr};
    EntityPool<16> entities_{};
    TransformPool<16> transforms_{};
    NPCPool<8> npcs_{};
    SchedulePool<8> sched_{};
    Region3 regions_[3]{};
    QuestLog<8> log_{};
    PlayerState<8> player_{};
    Inventory<8> inv_{};
    LanguageProfile<32> lang_{};
    GameClock clock_{};
    InteractSession isession_{};
    DialogueSession dsession_{};
    Transform3 playerTr_{};
    uint16_t anna_{0xFFFFu};
    uint16_t clerk_{0xFFFFu};
    bool showFinal_{false};
    char message_[96]{};
    uint32_t messageUntil_{0};
    uint32_t nowMs_{0};
    char objective_[128]{};
    char prompt_[64]{};

    void say(const char* text);
    void refreshTexts();
    void pumpQuests();
    const NPCAgent* anna() const;
};

} // namespace l3d
