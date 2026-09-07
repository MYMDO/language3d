// Playable-scenario orchestration (desktop-only, stdio for the save
// file; no SDL dependency so host tests drive the same code as the game).
// One generic runtime shared by every scenario definition (see scenario.h);
// scenario differences live in ScenarioDef tables below, never in branches.
#include "scenario.h"

#include <cstdio>
#include <cstring>

namespace l3d {

namespace {

// Shared market geography (cells verified open in assets/map.txt).
// Both built-in scenarios play in this district.
constexpr ScenarioRegionDef kMarketRegions[] = {
    {17, 17, 19, 19, 10}, // MARKET plaza
    {16, 17, 17, 18, 11}, // HOME nook
    {16, 16, 20, 20, 3},  // station district
};

// Anna's working day, shared wherever she appears.
constexpr ScheduleEntry kAnnaDay[] = {
    {360, 1080, 10, uint8_t(SchedBehavior::STAY)},
    {1080, 360, 11, uint8_t(SchedBehavior::REST)},
};

constexpr ScenarioDef kStationDef = {
    "station",
    "language3d.save",
    1, // quest: Getting to the Station
    {
        {1, "Anna", 1, 1, uint8_t(NPCArchetype::RESIDENT), 18, 18,
         {kAnnaDay[0], kAnnaDay[1]},
         2},
        {2, "Clerk", 2, 0, uint8_t(NPCArchetype::CLERK), 19, 17, {}, 0},
    },
    2,
    {
        kMarketRegions[0],
        kMarketRegions[1],
        kMarketRegions[2],
    },
    3,
};

constexpr ScenarioDef kShopDef = {
    "shop",
    "language3d-shop.save",
    2, // quest: An Apple for Anna
    {
        {1, "Anna", 1, 1, uint8_t(NPCArchetype::RESIDENT), 18, 18,
         {kAnnaDay[0], kAnnaDay[1]},
         2},
        {3, "Shopkeeper", 4, 2, uint8_t(NPCArchetype::SHOPKEEPER), 17, 19, {}, 0},
    },
    2,
    {
        kMarketRegions[0],
        kMarketRegions[1],
        kMarketRegions[2],
    },
    3,
};

constexpr const ScenarioDef* kDefs[] = {&kStationDef, &kShopDef};

} // namespace

const ScenarioDef* findScenarioDef(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < 2; ++i) {
        const char* a = kDefs[i]->id;
        const char* b = id;
        while (*a && *a == *b) {
            ++a;
            ++b;
        }
        if (*a == *b) return kDefs[i];
    }
    return nullptr;
}

size_t scenarioDefCount() { return 2; }

const ScenarioDef* scenarioDefAt(size_t i) {
    return i < 2 ? kDefs[i] : nullptr;
}

bool Scenario::init(const DialogueBank* db, const QuestBank* qb,
                    const ItemBank* ib, const VocabularyBank* vb,
                    const ScenarioDef* def) {
    if (!db || !qb || !ib || !vb || !def) return false;
    if (def->npcCount == 0 || def->npcCount > 4) return false;
    if (def->regionCount > 4) return false;
    dbank_ = db;
    qbank_ = qb;
    ibank_ = ib;
    vbank_ = vb;
    def_ = def;
    entities_.init();
    npcs_.init();
    sched_.init();
    log_.init();
    inv_.init();
    player_.init(0xFFFFu);
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    lang_.init(pair);
    clock_ = GameClock{};
    clock_.minute = 480; // 08:00, market hours
    isession_ = InteractSession{};
    dsession_ = DialogueSession{};
    showFinal_ = false;
    message_[0] = '\0';
    npcCount_ = 0;
    // Fixed spawn order (load relies on it: same order -> same ids).
    const uint16_t ePlayer = entities_.spawn(EntityKind::PLAYER);
    if (ePlayer == L3D_ENTITY_INVALID) return false;
    for (size_t i = 0; i < def->npcCount; ++i) {
        const ScenarioNPCDef& nd = def->npcs[i];
        const uint16_t e = entities_.spawn(EntityKind::NPC);
        if (e == L3D_ENTITY_INVALID) return false;
        Transform3 t{};
        t.pos = Vec3{Fx::from_int(nd.cellX) + Fx::from_float(0.5f),
                     Fx::from_int(nd.cellY) + Fx::from_float(0.5f), Fx{}};
        transforms_.attach(e, t);
        const uint16_t id =
            npcs_.spawn(e, NPCArchetype(nd.archetype));
        if (id == L3D_NPC_INVALID) return false;
        if (nd.schedCount > 0 &&
            !sched_.set(id, nd.sched, nd.schedCount))
            return false;
        npcIds_[npcCount_++] = id;
    }
    for (size_t i = 0; i < def->regionCount; ++i) {
        const ScenarioRegionDef& rd = def->regions[i];
        regions_[i].box.mn =
            Vec3{Fx::from_int(rd.x0), Fx::from_int(rd.y0), Fx{}};
        regions_[i].box.mx =
            Vec3{Fx::from_int(rd.x1), Fx::from_int(rd.y1), Fx::from_int(2)};
        regions_[i].contentTag = rd.tag;
    }
    player_.init(ePlayer);
    player_.bind_inventory(&inv_);
    playerTr_ = Transform3{};
    refreshTexts();
    return true;
}

void Scenario::tick(uint32_t dtMs, uint32_t nowMs,
                    bool (*solid)(void*, int32_t, int32_t), void* ctx) {
    nowMs_ = nowMs;
    clock_.advance(dtMs);
    // Every scheduled NPC lives on its own timetable (solid-aware so nobody
    // clips walls); static NPCs simply stand.
    auto blocked = [solid, ctx](const Vec3& from, const Vec3& to) {
        (void)from;
        if (!solid) return false;
        return solid(ctx, to.x.to_int(), to.y.to_int());
    };
    for (size_t i = 0; i < npcCount_; ++i) {
        if (def_->npcs[i].schedCount == 0) continue;
        npc_dispatch_step(npcs_, transforms_, sched_, regions_, def_->regionCount,
                          npcIds_[i], clock_, dtMs, Fx::from_int(1), blocked);
    }
    // Walking away aborts the dialogue; the interaction record closes too.
    if (dsession_.state == uint8_t(DialogueState::ACTIVE) &&
        interact_target(npcs_, transforms_, playerTr_, Fx::from_int(3)) ==
            L3D_INTERACT_NONE) {
        dialogue_abort(dsession_);
        interact_end(isession_, nowMs);
        showFinal_ = false;
        say("You walked away.");
    }
    pumpQuests();
    refreshTexts();
}

Vec2 Scenario::npcPos(size_t i) const {
    if (i >= npcCount_) return Vec2{};
    const NPCAgent* a = npcs_.get(npcIds_[i]);
    if (!a) return Vec2{};
    const Transform3* tr = transforms_.get(a->entity);
    if (!tr) return Vec2{};
    return Vec2{tr->pos.x, tr->pos.y};
}

uint16_t Scenario::npcIdByTag(uint16_t tag) const {
    if (!def_) return L3D_NPC_INVALID;
    for (size_t i = 0; i < npcCount_; ++i) {
        if (def_->npcs[i].tag == tag) return npcIds_[i];
    }
    return L3D_NPC_INVALID;
}

uint16_t Scenario::dialogueFor(uint16_t tag) const {
    if (!def_) return DIALOGUE_NONE;
    for (size_t i = 0; i < npcCount_; ++i) {
        if (def_->npcs[i].tag != tag) continue;
        const uint16_t group = def_->npcs[i].variantGroup;
        if (group == 0) return def_->npcs[i].dialogue;
        // Data-driven variant selection: the highest required mastery the
        // profile meets wins, otherwise the fallback.
        const uint16_t v = dq_select_variant(lang_, *dbank_, group);
        return (v == DIALOGUE_NONE) ? def_->npcs[i].dialogue : v;
    }
    return DIALOGUE_NONE;
}

const ScenarioNPCDef* Scenario::npcDefByTag(uint16_t tag) const {
    if (!def_) return nullptr;
    for (size_t i = 0; i < npcCount_ && i < def_->npcCount; ++i) {
        if (def_->npcs[i].tag == tag) return &def_->npcs[i];
    }
    return nullptr;
}

static const char* itemName(const ItemBank* bank, uint16_t id) {
    if (!bank) return "item";
    const ItemDef* d = item_find(*bank, id);
    return (d && d->name) ? d->name : "item";
}

bool Scenario::pressE(uint32_t nowMs) {
    nowMs_ = nowMs;
    if (showFinal_) {
        // Dismiss the terminal panel.
        showFinal_ = false;
        dialogue_end(dsession_);
        interact_end(isession_, nowMs);
        refreshTexts();
        return true;
    }
    if (dsession_.state == uint8_t(DialogueState::ACTIVE)) return true; // held open
    const uint16_t target =
        interact_target(npcs_, transforms_, playerTr_, Fx::from_int(3));
    if (target == L3D_INTERACT_NONE) return false;
    // Item handoff shortcut: holding the item for a GIVE objective whose
    // recipient is the targeted NPC reports directly instead of reopening
    // small talk.
    const QuestRuntime* qr = def_ ? log_.find(def_->quest) : nullptr;
    if (qr && qr->state == uint8_t(QuestState::ACTIVE)) {
        const QuestDef* qd = quest_find(*qbank_, def_->quest);
        if (qd && qr->objective_idx < qd->objective_count) {
            const QuestObjective& o = qd->objectives[qr->objective_idx];
            const ScenarioNPCDef* nd = npcDefByTag(o.npc);
            if (o.type == uint8_t(ObjectiveType::GIVE) && nd &&
                target == npcIdByTag(o.npc) && inv_.has(o.item, o.count)) {
                quest_report(log_, *qbank_, player_, def_->quest,
                             uint8_t(ObjectiveType::GIVE), o.npc, o.item,
                             o.count);
                char msg[96]{};
                std::snprintf(msg, sizeof(msg), "%s delivered!",
                              itemName(ibank_, o.item));
                say(msg);
                pumpQuests();
                refreshTexts();
                return true;
            }
        }
    }
    if (interact_try(isession_, target, nowMs) != InteractResult::STARTED)
        return true; // ALREADY/COOLDOWN: consumed, nothing new
    const ScenarioNPCDef* nd = nullptr;
    for (size_t i = 0; i < npcCount_; ++i) {
        if (npcIds_[i] == target) {
            nd = npcDefByTag(def_->npcs[i].tag);
            break;
        }
    }
    if (!nd) {
        interact_end(isession_, nowMs);
        return true;
    }
    const uint16_t dlg = dialogueFor(nd->tag);
    EffectResult applied{};
    if (!dq_begin(dsession_, *dbank_, log_, *qbank_, *ibank_, player_, dlg,
                  target, &applied)) {
        interact_end(isession_, nowMs);
        return true;
    }
    const DialogueDef* dd = dialogue_find(*dbank_, dlg);
    if (dd) {
        // Entry exposure is recorded when the node is shown (renderPanel
        // shows dsession_.node; observe it here for determinism).
        const DialogueNode* at = dialogue_find_node(*dd, dsession_.node);
        if (at) lang_observe_dialogue(lang_, *vbank_, *at);
    }
    if (applied.kind == uint8_t(EffectApply::QUEST_EVENT) &&
        applied.quest_event == uint8_t(QuestEvent::STARTED)) {
        const QuestDef* qd = quest_find(*qbank_, def_->quest);
        char msg[96]{};
        std::snprintf(msg, sizeof(msg), "Quest started: %s",
                      (qd && qd->title) ? qd->title : "unknown");
        say(msg);
    }
    refreshTexts();
    return true;
}

bool Scenario::pressAnswer(int n) {
    if (dsession_.state != uint8_t(DialogueState::ACTIVE)) {
        // Terminal panel up: answers do nothing but stay consumed.
        return showFinal_;
    }
    if (n < 1 || n > 4) return true; // consumed, out of range
    DialogueStep st = dq_choose(dsession_, *dbank_, player_, log_, *qbank_,
                                *ibank_, lang_, *vbank_, size_t(n - 1));
    if (!st.advanced) {
        // Wrong answer: the NPC asks again (verdict already recorded).
        if (st.verdict == uint8_t(ResponseVerdict::INCORRECT))
            say("Hmm, that doesn't help. Try again.");
        refreshTexts();
        return true;
    }
    const DialogueDef* dd = dialogue_find(*dbank_, dsession_.dialogue);
    if (dd) {
        const DialogueNode* at = dialogue_find_node(*dd, dsession_.node);
        if (at) lang_observe_dialogue(lang_, *vbank_, *at);
    }
    if (st.effect.kind == uint8_t(EffectApply::APPLIED)) {
        const DialogueNode* at =
            dd ? dialogue_find_node(*dd, dsession_.node) : nullptr;
        // Name the received item when the effect hands one over.
        const char* what = "Item";
        if (at && at->effect.kind == uint8_t(DialogueEffectKind::GIVE_ITEM))
            what = itemName(ibank_, at->effect.p1);
        char msg[96]{};
        std::snprintf(msg, sizeof(msg), "%s received!", what);
        say(msg);
    }
    if (st.effect.kind == uint8_t(EffectApply::QUEST_EVENT) &&
        st.effect.quest_event == uint8_t(QuestEvent::STARTED)) {
        const QuestDef* qd = quest_find(*qbank_, def_->quest);
        char msg[96]{};
        std::snprintf(msg, sizeof(msg), "Quest started: %s",
                      (qd && qd->title) ? qd->title : "unknown");
        say(msg);
    }
    if (dsession_.state == uint8_t(DialogueState::COMPLETED)) {
        showFinal_ = true; // keep the terminal line visible until E
        interact_end(isession_, nowMs_);
    }
    refreshTexts();
    return true;
}

bool Scenario::saveGame(const char* path) {
    if (!path) return false;
    SaveInput<8, 8, 32> in{&clock_, &player_, &log_, &lang_};
    static char staging[2048]{};
    SaveResult why = SaveResult::MALFORMED;
    const size_t n = save_write(staging, sizeof(staging), in, &why);
    if (n == 0 || why != SaveResult::OK) return false;
    if (save_validate(staging, n) != SaveResult::OK) return false;
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t wrote = std::fwrite(staging, 1, n, f);
    std::fclose(f);
    if (wrote != n) return false;
    say("Game saved.");
    refreshTexts();
    return true;
}

bool Scenario::loadGame(const char* path) {
    if (!path) return false;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        say("No save found.");
        refreshTexts();
        return false;
    }
    static char staging[2048]{};
    const size_t n = std::fread(staging, 1, sizeof(staging), f);
    std::fclose(f);
    if (n == 0 || n >= sizeof(staging)) return false;
    if (save_validate(staging, n) != SaveResult::OK) {
        say("Save is invalid.");
        refreshTexts();
        return false;
    }
    // Fresh pools in the fixed spawn order reproduce the same runtime ids,
    // so the save's adopted entity references stay valid by construction.
    const DialogueBank* db = dbank_;
    const QuestBank* qb = qbank_;
    const ItemBank* ib = ibank_;
    const VocabularyBank* vb = vbank_;
    const ScenarioDef* def = def_;
    if (!init(db, qb, ib, vb, def)) return false;
    const SaveResult r =
        save_read(staging, n, clock_, player_, log_, lang_, *qb, *vb, *ib);
    if (r != SaveResult::OK) {
        say("Save is invalid.");
        refreshTexts();
        return false;
    }
    showFinal_ = false;
    say("Game loaded.");
    refreshTexts();
    return true;
}

bool Scenario::dialogueOpen() const {
    return dsession_.state == uint8_t(DialogueState::ACTIVE) || showFinal_;
}

void Scenario::say(const char* text) {
    size_t i = 0;
    while (text[i] && i + 1 < sizeof(message_)) {
        message_[i] = text[i];
        ++i;
    }
    message_[i] = '\0';
    messageUntil_ = nowMs_ + 4000;
}

const char* Scenario::message() const {
    if (!message_[0] || nowMs_ > messageUntil_) return "";
    return message_;
}

// Reward XP total for the claim line (quests without XP show none).
static uint32_t claimXp(const QuestDef* qd) {
    uint32_t total = 0;
    if (!qd) return total;
    for (size_t i = 0; i < qd->reward_count; ++i) {
        if (qd->rewards[i].kind == uint8_t(RewardType::XP))
            total += qd->rewards[i].p1;
    }
    return total;
}

void Scenario::refreshTexts() {
    // Objective line follows the scenario quest; names come from content.
    objective_[0] = '\0';
    const QuestRuntime* qr = def_ ? log_.find(def_->quest) : nullptr;
    if (qr) {
        const QuestDef* qd = quest_find(*qbank_, def_->quest);
        if (qr->state == uint8_t(QuestState::ACTIVE) && qd &&
            qr->objective_idx < qd->objective_count) {
            const QuestObjective& o = qd->objectives[qr->objective_idx];
            using OT = ObjectiveType;
            if (o.type == uint8_t(OT::TALK)) {
                const ScenarioNPCDef* nd = npcDefByTag(o.tag);
                std::snprintf(objective_, sizeof(objective_), "Quest: talk to %s (E)",
                              nd ? nd->name : "someone");
            } else if (o.type == uint8_t(OT::REACH)) {
                std::snprintf(objective_, sizeof(objective_),
                              "Quest: reach the destination");
            } else if (o.type == uint8_t(OT::COLLECT)) {
                std::snprintf(objective_, sizeof(objective_), "Quest: get %s",
                              itemName(ibank_, o.item));
            } else if (o.type == uint8_t(OT::GIVE)) {
                const ScenarioNPCDef* nd = npcDefByTag(o.npc);
                std::snprintf(objective_, sizeof(objective_), "Quest: give %s to %s (E)",
                              itemName(ibank_, o.item),
                              nd ? nd->name : "someone");
            } else {
                std::snprintf(objective_, sizeof(objective_),
                              "Quest: follow your journal");
            }
        } else if (qr->state == uint8_t(QuestState::COMPLETED)) {
            std::snprintf(objective_, sizeof(objective_),
                          "Quest complete! Well done.");
        } else if (qr->state == uint8_t(QuestState::CLAIMED)) {
            const QuestDef* qd = quest_find(*qbank_, def_->quest);
            std::snprintf(objective_, sizeof(objective_), "%s claimed. +%u XP",
                          (qd && qd->title) ? qd->title : "Quest",
                          claimXp(qd));
        }
    }
    if (!objective_[0]) {
        const char* first =
            (def_ && def_->npcCount > 0) ? def_->npcs[0].name : "the NPC";
        std::snprintf(objective_, sizeof(objective_),
                      "Explore: find %s (walk to the NPC, press E)", first);
    }
    // Proximity prompt (suppressed while a dialogue panel is up).
    prompt_[0] = '\0';
    if (!dialogueOpen()) {
        const uint16_t target =
            interact_target(npcs_, transforms_, playerTr_, Fx::from_int(3));
        for (size_t k = 0; k < npcCount_; ++k) {
            if (npcIds_[k] == target && def_ && k < def_->npcCount) {
                std::snprintf(prompt_, sizeof(prompt_), "%s nearby - press E",
                              def_->npcs[k].name ? def_->npcs[k].name : "NPC");
                break;
            }
        }
    }
}

void Scenario::pumpQuests() {
    if (!def_) return;
    QuestRuntime* qr = log_.find(def_->quest);
    if (!qr) return;
    if (qr->state == uint8_t(QuestState::ACTIVE)) {
        const QuestDef* qd = quest_find(*qbank_, def_->quest);
        if (qd && qr->objective_idx < qd->objective_count) {
            const QuestObjective& o = qd->objectives[qr->objective_idx];
            using OT = ObjectiveType;
            if (o.type == uint8_t(OT::REACH)) {
                // Any region carrying the objective tag counts.
                for (size_t i = 0; i < def_->regionCount; ++i) {
                    Region3 box{};
                    box.box.mn = Vec3{Fx::from_int(def_->regions[i].x0),
                                      Fx::from_int(def_->regions[i].y0), Fx{}};
                    box.box.mx = Vec3{Fx::from_int(def_->regions[i].x1),
                                      Fx::from_int(def_->regions[i].y1),
                                      Fx::from_int(2)};
                    box.contentTag = def_->regions[i].tag;
                    if (box.contentTag != o.tag) continue;
                    if (aabb_contains(box.box, playerTr_.pos)) {
                        if (quest_report(log_, *qbank_, player_, def_->quest,
                                         uint8_t(OT::REACH), o.tag, 0,
                                         0) == QuestEvent::OBJECTIVE_DONE)
                            say("Destination reached!");
                    }
                }
            } else if (o.type == uint8_t(OT::COLLECT)) {
                if (inv_.has(o.item, o.count)) {
                    if (quest_report(log_, *qbank_, player_, def_->quest,
                                     uint8_t(OT::COLLECT), 0, o.item,
                                     inv_.count_of(o.item)) == QuestEvent::OBJECTIVE_DONE) {
                        char msg[96]{};
                        std::snprintf(msg, sizeof(msg), "%s collected!",
                                      itemName(ibank_, o.item));
                        say(msg);
                    }
                }
            }
        }
        qr = log_.find(def_->quest);
    }
    if (qr && qr->state == uint8_t(QuestState::COMPLETED)) {
        if (quest_claim(log_, *qbank_, *ibank_, player_, def_->quest) ==
            QuestEvent::CLAIMED) {
            const QuestDef* qd = quest_find(*qbank_, def_->quest);
            char msg[96]{};
            std::snprintf(msg, sizeof(msg), "Quest complete! +%u XP", claimXp(qd));
            say(msg);
        }
    }
}

const char* Scenario::objectiveText() const { return objective_; }
const char* Scenario::promptText() const { return prompt_; }

void Scenario::renderPanel(uint8_t* px, uint16_t w, uint16_t h,
                           uint32_t stride) const {
    if (!px || w < 64 || h < 64) return;
    // HUD: objective (top) + timed message under it.
    draw_text(px, w, h, stride, 8, 4, objective_, 255);
    if (message()[0]) draw_text(px, w, h, stride, 8, 16, message(), 200);
    // Proximity prompt above the dialogue zone.
    if (prompt_[0] && !dialogueOpen())
        draw_text(px, w, h, stride, 8, int(h) - 24, prompt_, 255);
    if (!dialogueOpen()) return;
    // Dialogue panel: bottom box with speaker, wrapped text, choices.
    const DialogueDef* dd = dialogue_find(*dbank_, dsession_.dialogue);
    if (!dd) return;
    const DialogueNode* at = dialogue_find_node(*dd, dsession_.node);
    if (!at) return;
    const int margin = 8;
    const int panel_h = 148;
    const int y0 = int(h) - panel_h - 4;
    for (int y = y0; y < int(h) - 4; ++y) {
        for (int x = margin; x < int(w) - margin; ++x) {
            if (x < 0 || y < 0 || x >= int(w) || y >= int(h)) continue;
            px[size_t(y) * stride + size_t(x)] = 32; // PANEL
        }
    }
    const char* who = "NPC:";
    char speaker[64]{};
    for (size_t i = 0; i < npcCount_; ++i) {
        if (def_->npcs[i].tag == dd->npc_tag) {
            std::snprintf(speaker, sizeof(speaker), "%s:",
                          def_->npcs[i].name ? def_->npcs[i].name : "NPC");
            who = speaker;
            break;
        }
    }
    draw_text(px, w, h, stride, margin + 4, y0 + 6, who, 200);
    // Greedy word wrap into at most 5 text lines. The line buffer covers
    // the widest supported panel (256 cells); longer content wraps, and
    // any remainder past the last line is dropped (never overflows).
    const int chars = (int(w) - 2 * (margin + 4)) / 8;
    const int perLine = chars < 250 ? chars : 250;
    char lines[5][256]{};
    size_t li = 0, ci = 0;
    const char* p = at->text;
    bool truncated = false;
    while (*p && !truncated) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char* word = p;
        while (*p && *p != ' ') ++p;
        const size_t wlen = size_t(p - word);
        if (ci > 0 && ci + 1 + wlen > size_t(perLine)) {
            lines[li][ci] = '\0';
            ++li;
            ci = 0;
            if (li >= 5) {
                truncated = true;
                break;
            }
        }
        if (ci > 0) {
            if (ci >= 255) {
                truncated = true;
                break;
            }
            lines[li][ci++] = ' ';
        }
        for (size_t k = 0; k < wlen; ++k) {
            if (ci >= 255) {
                truncated = true;
                break;
            }
            lines[li][ci++] = word[k];
        }
    }
    if (!truncated && li < 5) lines[li][ci] = '\0';
    for (size_t r = 0; r <= li && r < 5; ++r) {
        if (!lines[r][0]) continue;
        draw_text(px, w, h, stride, margin + 4, y0 + 20 + int(r) * 10,
                  lines[r], 255);
    }
    // Choices (player picks 1..N) or terminal hint.
    if (at->choice_count == 0) {
        draw_text(px, w, h, stride, margin + 4, y0 + 20 + 5 * 10,
                  "(press E to continue)", 200);
    } else {
        for (size_t c = 0; c < at->choice_count && c < 4; ++c) {
            char buf[96]{};
            buf[0] = char('1' + c);
            buf[1] = ')';
            buf[2] = ' ';
            size_t k = 0;
            while (at->choices[c].text[k] && k + 3 < sizeof(buf) - 1) {
                buf[3 + k] = at->choices[c].text[k];
                ++k;
            }
            buf[3 + k] = '\0';
            draw_text(px, w, h, stride, margin + 4,
                      y0 + 76 + int(c) * 12, buf, 255);
        }
    }
}

} // namespace l3d
