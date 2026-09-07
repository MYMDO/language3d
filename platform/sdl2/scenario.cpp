// Phase 14 vertical slice orchestration (desktop-only, stdio for the save
// file; no SDL dependency so host tests drive the same code as the game).
#include "scenario.h"

#include <cstdio>
#include <cstring>

namespace l3d {

bool Scenario::init(const DialogueBank* db, const QuestBank* qb,
                    const ItemBank* ib, const VocabularyBank* vb) {
    if (!db || !qb || !ib || !vb) return false;
    dbank_ = db;
    qbank_ = qb;
    ibank_ = ib;
    vbank_ = vb;
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
    // Fixed spawn order (load relies on it: same order -> same ids).
    const uint16_t ePlayer = entities_.spawn(EntityKind::PLAYER);
    const uint16_t eAnna = entities_.spawn(EntityKind::NPC);
    const uint16_t eClerk = entities_.spawn(EntityKind::NPC);
    if (ePlayer == L3D_ENTITY_INVALID || eAnna == L3D_ENTITY_INVALID ||
        eClerk == L3D_ENTITY_INVALID)
        return false;
    Transform3 ta{};
    ta.pos = Vec3{Fx::from_int(18) + Fx::from_float(0.5f),
                  Fx::from_int(18) + Fx::from_float(0.5f), Fx{}};
    transforms_.attach(eAnna, ta);
    Transform3 tc{};
    tc.pos = Vec3{Fx::from_int(19) + Fx::from_float(0.5f),
                  Fx::from_int(17) + Fx::from_float(0.5f), Fx{}};
    transforms_.attach(eClerk, tc);
    anna_ = npcs_.spawn(eAnna, NPCArchetype::RESIDENT);
    clerk_ = npcs_.spawn(eClerk, NPCArchetype::CLERK);
    if (anna_ == L3D_NPC_INVALID || clerk_ == L3D_NPC_INVALID) return false;
    player_.init(ePlayer);
    player_.bind_inventory(&inv_);
    // Anna's day: MARKET plaza in working hours, HOME nook overnight.
    regions_[0].box.mn = Vec3{Fx::from_int(17), Fx::from_int(17), Fx{}};
    regions_[0].box.mx = Vec3{Fx::from_int(19), Fx::from_int(19), Fx::from_int(2)};
    regions_[0].contentTag = SLICE_MARKET_TAG;
    regions_[1].box.mn = Vec3{Fx::from_int(16), Fx::from_int(17), Fx{}};
    regions_[1].box.mx = Vec3{Fx::from_int(17), Fx::from_int(18), Fx::from_int(2)};
    regions_[1].contentTag = SLICE_HOME_TAG;
    regions_[2].box.mn = Vec3{Fx::from_int(16), Fx::from_int(16), Fx{}};
    regions_[2].box.mx = Vec3{Fx::from_int(20), Fx::from_int(20), Fx::from_int(2)};
    regions_[2].contentTag = SLICE_STATION_TAG;
    const ScheduleEntry day[] = {
        {360, 1080, SLICE_MARKET_TAG, uint8_t(SchedBehavior::STAY)},
        {1080, 360, SLICE_HOME_TAG, uint8_t(SchedBehavior::REST)},
    };
    if (!sched_.set(anna_, day, 2)) return false;
    playerTr_ = Transform3{};
    refreshTexts();
    return true;
}

void Scenario::tick(uint32_t dtMs, uint32_t nowMs,
                    bool (*solid)(void*, int32_t, int32_t), void* ctx) {
    nowMs_ = nowMs;
    clock_.advance(dtMs);
    // Anna lives on her schedule (solid-aware so she never clips walls).
    auto blocked = [solid, ctx](const Vec3& from, const Vec3& to) {
        (void)from;
        if (!solid) return false;
        return solid(ctx, to.x.to_int(), to.y.to_int());
    };
    npc_dispatch_step(npcs_, transforms_, sched_, regions_, 2, anna_, clock_,
                      dtMs, Fx::from_int(1), blocked);
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

Vec2 Scenario::annaPos() const {
    const NPCAgent* a = anna();
    if (!a) return Vec2{};
    const Transform3* tr = transforms_.get(a->entity);
    if (!tr) return Vec2{};
    return Vec2{tr->pos.x, tr->pos.y};
}

Vec2 Scenario::clerkPos() const {
    const NPCAgent* c = npcs_.get(clerk_);
    if (!c) return Vec2{};
    const Transform3* tr = transforms_.get(c->entity);
    if (!tr) return Vec2{};
    return Vec2{tr->pos.x, tr->pos.y};
}

uint16_t Scenario::annaDialogue() const {
    const VocabularyProgress* s = lang_.progress_of(SLICE_STATION_WORD);
    if (s && mastery_of(*s) >= Mastery::FAMILIAR)
        return SLICE_ANNA_FAMILIAR_DIALOGUE;
    return SLICE_ANNA_DIALOGUE;
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
    // Ticket handoff shortcut: holding the ticket for GIVE near the clerk
    // reports directly instead of reopening small talk.
    const QuestRuntime* qr = log_.find(SLICE_QUEST);
    if (target == clerk_ && qr && qr->state == uint8_t(QuestState::ACTIVE)) {
        const QuestDef* qd = quest_find(*qbank_, SLICE_QUEST);
        if (qd && qr->objective_idx < qd->objective_count) {
            const QuestObjective& o = qd->objectives[qr->objective_idx];
            if (o.type == uint8_t(ObjectiveType::GIVE) && inv_.has(o.item, o.count)) {
                quest_report(log_, *qbank_, player_, SLICE_QUEST,
                             uint8_t(ObjectiveType::GIVE), SLICE_CLERK_TAG,
                             o.item, o.count);
                say("Ticket delivered!");
                pumpQuests();
                refreshTexts();
                return true;
            }
        }
    }
    if (interact_try(isession_, target, nowMs) != InteractResult::STARTED)
        return true; // ALREADY/COOLDOWN: consumed, nothing new
    const uint16_t dlg = (target == anna_) ? annaDialogue() : SLICE_CLERK_DIALOGUE;
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
        applied.quest_event == uint8_t(QuestEvent::STARTED))
        say("Quest started: Getting to the Station");
    refreshTexts();
    return true;
}

bool Scenario::pressAnswer(int n) {
    if (dsession_.state != uint8_t(DialogueState::ACTIVE)) {
        // Terminal panel up: answers do nothing but stay consumed.
        return showFinal_;
    }
    if (n < 1 || n > 4) return true; // consumed, out of range
    DialogueStep st =
        dq_choose(dsession_, *dbank_, player_, log_, *qbank_, *ibank_, size_t(n - 1));
    if (!st.advanced) return true;
    const DialogueDef* dd = dialogue_find(*dbank_, dsession_.dialogue);
    if (dd) {
        const DialogueNode* at = dialogue_find_node(*dd, dsession_.node);
        if (at) {
            lang_observe_dialogue(lang_, *vbank_, *at);
            // The player's pick engages the entered node's vocabulary.
            for (size_t i = 0; i < at->lang.vocab_count; ++i)
                lang_.record(*vbank_, at->lang.vocab[i], LanguageEvent::USED);
            // Content convention (see content/dialogues/README.md): choice 1
            // is the constructive answer, so taking it counts as correct
            // language use. This is the v1 proxy until real assessment.
            if (n == 1) {
                for (size_t i = 0; i < at->lang.vocab_count; ++i)
                    lang_.record(*vbank_, at->lang.vocab[i],
                                 LanguageEvent::CORRECT);
            }
        }
    }
    if (st.effect.kind == uint8_t(EffectApply::APPLIED) &&
        dsession_.dialogue == SLICE_CLERK_DIALOGUE)
        say("Ticket received!");
    if (st.effect.kind == uint8_t(EffectApply::QUEST_EVENT) &&
        st.effect.quest_event == uint8_t(QuestEvent::STARTED))
        say("Quest started: Getting to the Station");
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
    if (!init(db, qb, ib, vb)) return false;
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

void Scenario::refreshTexts() {
    // Objective line follows the station quest.
    const char* obj = "Explore: find Anna (walk to the NPC, press E)";
    const QuestRuntime* qr = log_.find(SLICE_QUEST);
    if (qr) {
        const QuestDef* qd = quest_find(*qbank_, SLICE_QUEST);
        if (qr->state == uint8_t(QuestState::ACTIVE) && qd &&
            qr->objective_idx < qd->objective_count) {
            switch (qd->objectives[qr->objective_idx].type) {
                case uint8_t(ObjectiveType::TALK):
                    obj = "Quest: talk to Anna (E)";
                    break;
                case uint8_t(ObjectiveType::REACH):
                    obj = "Quest: reach the station district";
                    break;
                case uint8_t(ObjectiveType::COLLECT):
                    obj = "Quest: get a ticket from the clerk";
                    break;
                case uint8_t(ObjectiveType::GIVE):
                    obj = "Quest: give the ticket to the clerk (E)";
                    break;
                default:
                    obj = "Quest: follow your journal";
                    break;
            }
        } else if (qr->state == uint8_t(QuestState::COMPLETED)) {
            obj = "Quest complete! Well done.";
        } else if (qr->state == uint8_t(QuestState::CLAIMED)) {
            obj = "Station quest claimed. +100 XP";
        }
    }
    size_t i = 0;
    while (obj[i] && i + 1 < sizeof(objective_)) {
        objective_[i] = obj[i];
        ++i;
    }
    objective_[i] = '\0';
    // Proximity prompt (suppressed while a dialogue panel is up).
    const char* pr = "";
    if (!dialogueOpen()) {
        const uint16_t target =
            interact_target(npcs_, transforms_, playerTr_, Fx::from_int(3));
        if (target == anna_) pr = "Anna nearby - press E";
        else if (target == clerk_) pr = "Clerk nearby - press E";
    }
    size_t k = 0;
    while (pr[k] && k + 1 < sizeof(prompt_)) {
        prompt_[k] = pr[k];
        ++k;
    }
    prompt_[k] = '\0';
}

void Scenario::pumpQuests() {
    QuestRuntime* qr = log_.find(SLICE_QUEST);
    if (!qr) return;
    if (qr->state == uint8_t(QuestState::ACTIVE)) {
        const QuestDef* qd = quest_find(*qbank_, SLICE_QUEST);
        if (qd && qr->objective_idx < qd->objective_count) {
            const QuestObjective& o = qd->objectives[qr->objective_idx];
            using OT = ObjectiveType;
            if (o.type == uint8_t(OT::REACH)) {
                // Station district box (tag 3) contains the plaza the quest names.
                for (size_t i = 0; i < 3; ++i) {
                    if (regions_[i].contentTag != SLICE_STATION_TAG) continue;
                    if (aabb_contains(regions_[i].box, playerTr_.pos)) {
                        if (quest_report(log_, *qbank_, player_, SLICE_QUEST,
                                         uint8_t(OT::REACH), SLICE_STATION_TAG, 0,
                                         0) == QuestEvent::OBJECTIVE_DONE)
                            say("Station reached!");
                    }
                }
            } else if (o.type == uint8_t(OT::COLLECT)) {
                if (inv_.has(o.item, o.count)) {
                    if (quest_report(log_, *qbank_, player_, SLICE_QUEST,
                                     uint8_t(OT::COLLECT), 0, o.item,
                                     inv_.count_of(o.item)) == QuestEvent::OBJECTIVE_DONE)
                        say("Ticket collected!");
                }
            }
        }
        qr = log_.find(SLICE_QUEST);
    }
    if (qr && qr->state == uint8_t(QuestState::COMPLETED)) {
        if (quest_claim(log_, *qbank_, *ibank_, player_, SLICE_QUEST) ==
            QuestEvent::CLAIMED)
            say("Quest complete! +100 XP");
    }
}

const NPCAgent* Scenario::anna() const { return npcs_.get(anna_); }

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
    const char* who = (dd->npc_tag == SLICE_CLERK_TAG) ? "Clerk:" : "Anna:";
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
