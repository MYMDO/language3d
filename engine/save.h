#pragma once
// Roadmap Phase 13: versioned local persistence (infrastructure, not gameplay).
//
// The save system READS and WRITES snapshots; it owns no subsystem.
// Format choice (analyzed per spec): ONE line-based decimal text format for
// host AND RP2040 — deterministic (fixed field order), portable (ASCII/LF),
// endian-safe (no binary integers), versioned, independent of C++ memory
// layout (no raw struct dumps), tiny codec (hand-rolled integer I/O, no
// locale, no heap). Saves stay portable across platforms as a feature.
//
// Two-phase atomicity is structural: serialize into the CALLER's staging
// buffer, save_validate() it, and only then commit (write-temp+rename on
// host flash/SD). Truncated staging never validates, so a torn write can
// never look like a valid save. The engine itself never touches files.
//
// Schema v1 (exact line order, LF separated, final "END" line):
//   L3DSAVE 1
//   CLOCK <day:u32> <minute:u16> <acc:u32> <scale:u32>
//   PLAYER <entity:u16> <xp:u32> <level:u16> <rep:i16> <flags:u32>
//   COUNTERS <8 x u16>
//   INVENTORY <n:u16>            + n lines  SLOT <id:u16> <count:u16>
//   QUESTS <n:u16>               + n lines  QUEST <def:u16> <state:u8> <obj:u8> <prog:u16>
//   LANGUAGE <src:u8> <tgt:u8> <level:u8> <n:u16>
//                               + n lines  WORD <id:u16> <exp:u16> <uses:u16> <ok:u16> <bad:u16>
//   END
//
// Version policy: v1 supported; higher versions report NEEDS_MIGRATION at
// the single dispatch point (future migrations hook in there, none are
// pre-built). Content ids (items/quests/words) are validated against the
// CURRENT banks on load: unknown ids reject the save (strict, deterministic)
// instead of silently dropping progress. Entity ids persist opaquely; the
// owner rebinds live references after load. Pointers, platform/renderer
// objects, caches and sessions are never serialized by construction.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL,
// no FILE I/O in the engine (buffers in, buffers out).
#include "language.h"
#include "player.h"
#include "quest.h"
#include "schedule.h" // GameClock (one-way; schedule knows nothing of save)

#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t SAVE_VERSION = 1;
constexpr size_t SAVE_MAX_LINE = 160;

enum class SaveResult : uint8_t {
    OK = 0,
    BAD_VERSION,      // unparsable version line
    NEEDS_MIGRATION,  // well-formed header, supported < version
    MALFORMED,        // grammar/range violation
    TRUNCATED,        // ended before END (torn write included)
    OVERFLOW,         // staging buffer too small
    UNKNOWN_ID        // content id not present in the current banks
};

// Everything the writer needs, as non-owning references. Sizes are
// template parameters so any pool capacity works without heap.
template <size_t INV, size_t QN, size_t LN>
struct SaveInput {
    const GameClock* clock{nullptr};
    const PlayerState<INV>* player{nullptr};
    const QuestLog<QN>* quests{nullptr};
    const LanguageProfile<LN>* language{nullptr};
    // No banks: the writer dumps ids opaquely (banks validate on load).
};

// Serialize into out[cap]. Returns bytes written, or 0 with *why set when
// the buffer is too small (caller retries bigger). Deterministic: the same
// state always yields identical bytes.
template <size_t INV, size_t QN, size_t LN>
size_t save_write(char* out, size_t cap, const SaveInput<INV, QN, LN>& in,
                  SaveResult* why);

// Structural validation only (no state touched). The atomic-commit gate:
// validate the staging buffer BEFORE persisting it anywhere.
SaveResult save_validate(const char* data, size_t len);

// Apply a validated buffer: repopulates clock/player/inventory/quests/
// language. Pointers are preserved (inventory association) or rebound by
// the owner afterwards; entity ids are adopted opaquely. Returns OK or the
// first rejection reason; on failure the targets may be PARTIALLY written
// — callers must validate() first and treat load as all-or-nothing.
template <size_t INV, size_t QN, size_t LN>
SaveResult save_read(const char* data, size_t len, GameClock& clock,
                     PlayerState<INV>& player, QuestLog<QN>& quests,
                     LanguageProfile<LN>& language, const QuestBank& qbank,
                     const VocabularyBank& vbank, const ItemBank& ibank);

// ---- single grammar driver (used by both validate and read) ----
struct SaveCursor {
    const char* p;
    const char* end;
    bool lit(const char* s) {
        const char* q = p;
        while (*s && q < end && *q == *s) {
            ++q;
            ++s;
        }
        if (*s) return false;
        if (q < end && *q != ' ' && *q != '\n') return false; // delimiter
        p = q;
        return true;
    }
    bool space() {
        if (p >= end || *p != ' ') return false;
        ++p;
        return true;
    }
    bool eol() {
        if (p >= end || *p != '\n') return false;
        ++p;
        return true;
    }
    bool eof() const { return p == end; }
};
// Integer codec (hand-rolled, locale-independent). Defined in save.cpp;
// declared here so the template driver below sees them (two-phase lookup).
size_t save_write_u32(char* out, size_t cap, uint32_t v);
size_t save_write_i16(char* out, size_t cap, int16_t v);
bool save_read_u32(const char*& p, const char* end, uint32_t& v);
bool save_read_u16(const char*& p, const char* end, uint16_t& v);
bool save_read_u8(const char*& p, const char* end, uint8_t& v);
bool save_read_i16(const char*& p, const char* end, int16_t& v);

// Precise failure: EOF mid-grammar is TRUNCATED (torn write),
// anything else structural is MALFORMED.
inline bool save_ended(const SaveCursor& c) { return c.p >= c.end; }
inline SaveResult cut(const SaveCursor& c) {
    return save_ended(c) ? SaveResult::TRUNCATED : SaveResult::MALFORMED;
}

template <typename Sink>
SaveResult save_parse(const char* data, size_t len, Sink& sink) {
    if (!data) return SaveResult::MALFORMED;
    if (len == 0) return SaveResult::TRUNCATED;
    SaveCursor c{data, data + len};
    uint32_t v = 0;
    if (!c.lit("L3DSAVE") || !c.space() || !save_read_u32(c.p, c.end, v) || !c.eol())
        return SaveResult::BAD_VERSION;
    SaveResult r = sink.version(v);
    if (r != SaveResult::OK) return r;
    uint32_t a = 0, b = 0, d = 0;
    uint16_t w = 0;
    uint8_t by = 0;
    int16_t s16 = 0;
    // CLOCK day minute acc scale
    if (!c.lit("CLOCK") || !c.space() || !save_read_u32(c.p, c.end, a) || !c.space() ||
        !save_read_u16(c.p, c.end, w) || !c.space())
        return cut(c);
    {
        const uint16_t minute = w;
        if (!save_read_u32(c.p, c.end, b) || !c.space() ||
            !save_read_u32(c.p, c.end, d) || !c.eol())
            return cut(c);
        r = sink.clock(a, minute, b, d);
        if (r != SaveResult::OK) return r;
    }
    // PLAYER entity xp level rep flags
    if (!c.lit("PLAYER") || !c.space() || !save_read_u16(c.p, c.end, w) || !c.space())
        return cut(c);
    {
        const uint16_t entity = w;
        if (!save_read_u32(c.p, c.end, a) || !c.space() ||
            !save_read_u16(c.p, c.end, w) || !c.space())
            return cut(c);
        const uint16_t level = w;
        if (!save_read_i16(c.p, c.end, s16) || !c.space() ||
            !save_read_u32(c.p, c.end, b) || !c.eol())
            return cut(c);
        r = sink.player(entity, a, level, s16, b);
        if (r != SaveResult::OK) return r;
    }
    // COUNTERS c0..c7
    if (!c.lit("COUNTERS")) return cut(c);
    {
        uint16_t counters[PLAYER_COUNTERS]{};
        for (size_t i = 0; i < PLAYER_COUNTERS; ++i) {
            if (!c.space() || !save_read_u16(c.p, c.end, counters[i]))
                return cut(c);
        }
        if (!c.eol()) return cut(c);
        r = sink.counters(counters);
        if (r != SaveResult::OK) return r;
    }
    // INVENTORY n + SLOT lines
    if (!c.lit("INVENTORY") || !c.space() || !save_read_u16(c.p, c.end, w) || !c.eol())
        return cut(c);
    {
        const uint16_t nslots = w;
        r = sink.inv_begin(nslots);
        if (r != SaveResult::OK) return r;
        for (uint16_t i = 0; i < nslots; ++i) {
            uint16_t id = 0, count = 0;
            if (!c.lit("SLOT") || !c.space() ||
                !save_read_u16(c.p, c.end, id) || !c.space() ||
                !save_read_u16(c.p, c.end, count) || !c.eol())
                return SaveResult::TRUNCATED;
            r = sink.inv_slot(id, count);
            if (r != SaveResult::OK) return r;
        }
    }
    // QUESTS n + QUEST lines
    if (!c.lit("QUESTS") || !c.space() || !save_read_u16(c.p, c.end, w) || !c.eol())
        return cut(c);
    {
        const uint16_t nquests = w;
        r = sink.quests_begin(nquests);
        if (r != SaveResult::OK) return r;
        for (uint16_t i = 0; i < nquests; ++i) {
            uint16_t def = 0, prog = 0;
            uint8_t state = 0, obj = 0;
            if (!c.lit("QUEST") || !c.space() ||
                !save_read_u16(c.p, c.end, def) || !c.space() ||
                !save_read_u8(c.p, c.end, state) || !c.space() ||
                !save_read_u8(c.p, c.end, obj) || !c.space() ||
                !save_read_u16(c.p, c.end, prog) || !c.eol())
                return SaveResult::TRUNCATED;
            r = sink.quest(def, state, obj, prog);
            if (r != SaveResult::OK) return r;
        }
    }
    // LANGUAGE src tgt level n + WORD lines
    if (!c.lit("LANGUAGE") || !c.space() || !save_read_u8(c.p, c.end, by) || !c.space())
        return cut(c);
    {
        const uint8_t src = by;
        uint8_t tgt = 0, level = 0;
        uint16_t nwords = 0;
        if (!save_read_u8(c.p, c.end, tgt) || !c.space() ||
            !save_read_u8(c.p, c.end, level) || !c.space() ||
            !save_read_u16(c.p, c.end, nwords) || !c.eol())
            return cut(c);
        r = sink.language(src, tgt, level, nwords);
        if (r != SaveResult::OK) return r;
        for (uint16_t i = 0; i < nwords; ++i) {
            uint16_t id = 0, exp = 0, uses = 0, ok = 0, badw = 0;
            if (!c.lit("WORD") || !c.space() ||
                !save_read_u16(c.p, c.end, id) || !c.space() ||
                !save_read_u16(c.p, c.end, exp) || !c.space() ||
                !save_read_u16(c.p, c.end, uses) || !c.space() ||
                !save_read_u16(c.p, c.end, ok) || !c.space() ||
                !save_read_u16(c.p, c.end, badw) || !c.eol())
                return SaveResult::TRUNCATED;
            r = sink.word(id, exp, uses, ok, badw);
            if (r != SaveResult::OK) return r;
        }
    }
    if (!c.lit("END") || !c.eol() || !c.eof()) return cut(c);
    return sink.done();
}
size_t save_write_u32(char* out, size_t cap, uint32_t v);
size_t save_write_i16(char* out, size_t cap, int16_t v);
bool save_read_u32(const char*& p, const char* end, uint32_t& v);
bool save_read_u16(const char*& p, const char* end, uint16_t& v);
bool save_read_u8(const char*& p, const char* end, uint8_t& v);
bool save_read_i16(const char*& p, const char* end, int16_t& v);

// ---- strict cursor parser (single grammar, two sinks, no duplication) ----

// Structural-only sink: checks grammar + wire ranges, fills nothing.
struct SaveCheckSink {
    SaveResult version(uint32_t v) {
        if (v == SAVE_VERSION) return SaveResult::OK;
        return (v > SAVE_VERSION) ? SaveResult::NEEDS_MIGRATION
                                  : SaveResult::MALFORMED;
    }
    SaveResult clock(uint32_t, uint16_t minute, uint32_t, uint32_t scale) {
        if (minute >= MINUTES_PER_DAY || scale == 0) return SaveResult::MALFORMED;
        return SaveResult::OK;
    }
    SaveResult player(uint16_t, uint32_t, uint16_t, int16_t, uint32_t) {
        return SaveResult::OK;
    }
    SaveResult counters(const uint16_t*) { return SaveResult::OK; }
    SaveResult inv_begin(uint16_t) { return SaveResult::OK; }
    SaveResult inv_slot(uint16_t id, uint16_t count) {
        if (id == 0 || id == ITEM_NONE || count == 0) return SaveResult::MALFORMED;
        return SaveResult::OK;
    }
    SaveResult quests_begin(uint16_t) { return SaveResult::OK; }
    SaveResult quest(uint16_t def, uint8_t state, uint8_t obj, uint16_t) {
        if (def == 0 || def == QUEST_NONE) return SaveResult::MALFORMED;
        if (state > uint8_t(QuestState::CLAIMED)) return SaveResult::MALFORMED;
        if (obj >= QUEST_MAX_OBJECTIVES) return SaveResult::MALFORMED;
        return SaveResult::OK;
    }
    SaveResult language(uint8_t src, uint8_t tgt, uint8_t level, uint16_t) {
        if (src != uint8_t(DialogueLang::UNDEFINED) ||
            tgt != uint8_t(DialogueLang::UNDEFINED)) {
            LanguagePair pr{src, tgt};
            if (!language_pair_valid(pr)) return SaveResult::MALFORMED;
        }
        if (level > uint8_t(DialogueCEFR::C2)) return SaveResult::MALFORMED;
        return SaveResult::OK;
    }
    SaveResult word(uint16_t id, uint16_t, uint16_t, uint16_t, uint16_t) {
        if (id == 0 || id == VOCAB_NONE) return SaveResult::MALFORMED;
        return SaveResult::OK;
    }
    SaveResult done() { return SaveResult::OK; }
};

// Applying sink: fills live state, validating content ids against banks.
template <size_t INV, size_t QN, size_t LN>
struct SaveApplySink {
    GameClock& clk;
    PlayerState<INV>& pl;
    QuestLog<QN>& quests;
    LanguageProfile<LN>& lang;
    const QuestBank& qbank;
    const VocabularyBank& vbank;
    const ItemBank& ibank;

    SaveApplySink(GameClock& c, PlayerState<INV>& p, QuestLog<QN>& q,
                  LanguageProfile<LN>& l, const QuestBank& qb,
                  const VocabularyBank& vb, const ItemBank& ib)
        : clk(c), pl(p), quests(q), lang(l), qbank(qb), vbank(vb), ibank(ib) {
    }
    size_t inv_slots{0};
    size_t inv_max{0};
    bool inv_null{false};
    size_t quest_seen{0};
    size_t quest_max{0};
    size_t words_seen{0};
    size_t words_max{0};

    SaveResult version(uint32_t v) {
        if (v == SAVE_VERSION) return SaveResult::OK;
        return (v > SAVE_VERSION) ? SaveResult::NEEDS_MIGRATION
                                  : SaveResult::MALFORMED;
    }
    SaveResult clock(uint32_t day, uint16_t minute, uint32_t acc, uint32_t scale) {
        if (minute >= MINUTES_PER_DAY || scale == 0) return SaveResult::MALFORMED;
        clk.day = day;
        clk.minute = minute;
        clk.ms_acc = acc;
        clk.ms_per_minute = scale;
        return SaveResult::OK;
    }
    SaveResult player(uint16_t entity, uint32_t xp, uint16_t level, int16_t rep,
                      uint32_t flags) {
        pl.entity = entity;
        pl.xp = xp;
        pl.level = level;
        pl.reputation = rep;
        pl.flags = flags;
        return SaveResult::OK;
    }
    SaveResult counters(const uint16_t* c) {
        for (size_t i = 0; i < PLAYER_COUNTERS; ++i) pl.counters[i] = c[i];
        return SaveResult::OK;
    }
    SaveResult inv_begin(uint16_t n) {
        inv_null = (pl.inventory == nullptr);
        if (!inv_null) pl.inventory->clear();
        inv_slots = 0;
        inv_max = n;
        return SaveResult::OK;
    }
    SaveResult inv_slot(uint16_t id, uint16_t count) {
        const ItemDef* def = item_find(ibank, id);
        if (!def) return SaveResult::UNKNOWN_ID;
        if (count == 0) return SaveResult::MALFORMED;
        if (def->stackable) {
            if (count > def->max_stack) return SaveResult::MALFORMED;
        } else if (count != 1) {
            return SaveResult::MALFORMED;
        }
        if (inv_slots >= inv_max) return SaveResult::MALFORMED; // over count
        if (inv_null) return SaveResult::OVERFLOW; // nowhere to store
        size_t idx = 0;
        while (idx < INV && pl.inventory->slots[idx].count != 0) ++idx;
        if (idx >= INV) return SaveResult::OVERFLOW; // bigger world than here
        pl.inventory->slots[idx].id = id;
        pl.inventory->slots[idx].count = count;
        pl.inventory->used++;
        ++inv_slots;
        return SaveResult::OK;
    }
    SaveResult quests_begin(uint16_t n) {
        quests.init();
        quest_seen = 0;
        quest_max = n;
        return SaveResult::OK;
    }
    SaveResult quest(uint16_t def, uint8_t state, uint8_t obj, uint16_t prog) {
        const QuestDef* d = quest_find(qbank, def);
        if (!d) return SaveResult::UNKNOWN_ID;
        if (state > uint8_t(QuestState::CLAIMED)) return SaveResult::MALFORMED;
        if (obj > d->objective_count) return SaveResult::MALFORMED;
        if (quest_seen >= quest_max) return SaveResult::MALFORMED;
        if (quests.find(def)) return SaveResult::MALFORMED; // duplicate
        QuestRuntime* r = quests.track(def);
        if (!r) return SaveResult::OVERFLOW;
        r->state = state;
        r->objective_idx = obj;
        r->progress = prog;
        ++quest_seen;
        return SaveResult::OK;
    }
    SaveResult language(uint8_t src, uint8_t tgt, uint8_t level, uint16_t n) {
        LanguagePair pr{src, tgt};
        if (src != uint8_t(DialogueLang::UNDEFINED) ||
            tgt != uint8_t(DialogueLang::UNDEFINED)) {
            if (!language_pair_valid(pr)) return SaveResult::MALFORMED;
        }
        if (level > uint8_t(DialogueCEFR::C2)) return SaveResult::MALFORMED;
        lang.init(pr);
        lang.level = level;
        words_seen = 0;
        words_max = n;
        (void)n;
        return SaveResult::OK;
    }
    SaveResult word(uint16_t id, uint16_t exp, uint16_t uses, uint16_t ok,
                    uint16_t bad) {
        if (!vocab_find(vbank, id)) return SaveResult::UNKNOWN_ID;
        if (words_seen >= words_max) return SaveResult::MALFORMED;
        // Rebuild progress directly (counters are plain data by design).
        for (size_t i = 0; i < LN; ++i) {
            if (lang.slots[i].id == id)
                return SaveResult::MALFORMED; // duplicate
        }
        for (size_t i = 0; i < LN; ++i) {
            if (lang.slots[i].id == VOCAB_NONE) {
                lang.slots[i].id = id;
                lang.slots[i].exposures = exp;
                lang.slots[i].uses = uses;
                lang.slots[i].correct = ok;
                lang.slots[i].incorrect = bad;
                lang.tracked++;
                ++words_seen;
                return SaveResult::OK;
            }
        }
        return SaveResult::OVERFLOW;
    }
    SaveResult done() { return SaveResult::OK; }
};

// ---- deterministic writer (caller staging buffer, no heap) ----
struct SaveEmitter {
    char* out;
    size_t cap;
    size_t n{0};
    bool ok{true};
    bool put(char ch) {
        if (n >= cap) {
            ok = false;
            return false;
        }
        out[n++] = ch;
        return true;
    }
    bool str(const char* s) {
        while (*s) {
            if (!put(*s)) return false;
            ++s;
        }
        return true;
    }
    bool u32(uint32_t v) {
        char tmp[10]{};
        int len = 0;
        do {
            tmp[len++] = char('0' + (v % 10u));
            v /= 10u;
        } while (v);
        while (len--) {
            if (!put(tmp[len])) return false;
        }
        return true;
    }
    bool u16(uint16_t v) { return u32(v); }
    bool u8(uint8_t v) { return u32(v); }
    bool i16(int16_t v) {
        int32_t wide = v;
        if (wide < 0) {
            if (!put('-')) return false;
            wide = -wide;
        }
        return u32(uint32_t(wide));
    }
    bool eol() { return put('\n'); }
    bool sp() { return put(' '); }
};

template <size_t INV, size_t QN, size_t LN>
size_t save_write(char* out, size_t cap, const SaveInput<INV, QN, LN>& in,
                  SaveResult* why) {
    auto fail = [&](SaveResult r) -> size_t {
        if (why) *why = r;
        return 0;
    };
    if (!out || cap == 0) return fail(SaveResult::OVERFLOW);
    if (!in.clock || !in.player || !in.quests || !in.language)
        return fail(SaveResult::MALFORMED);
    SaveEmitter e{out, cap};
    e.str("L3DSAVE ");
    e.u32(SAVE_VERSION);
    e.eol();
    e.str("CLOCK ");
    e.u32(in.clock->day);
    e.sp();
    e.u16(in.clock->minute);
    e.sp();
    e.u32(in.clock->ms_acc);
    e.sp();
    e.u32(in.clock->ms_per_minute);
    e.eol();
    e.str("PLAYER ");
    e.u16(in.player->entity);
    e.sp();
    e.u32(in.player->xp);
    e.sp();
    e.u16(in.player->level);
    e.sp();
    e.i16(in.player->reputation);
    e.sp();
    e.u32(in.player->flags);
    e.eol();
    e.str("COUNTERS");
    for (size_t i = 0; i < PLAYER_COUNTERS; ++i) {
        e.sp();
        e.u16(in.player->counters[i]);
    }
    e.eol();
    // Inventory: non-empty slots in slot order.
    uint16_t nslots = 0;
    if (in.player->inventory) {
        for (size_t i = 0; i < INV; ++i) {
            if (in.player->inventory->slots[i].count != 0) ++nslots;
        }
    }
    e.str("INVENTORY ");
    e.u16(nslots);
    e.eol();
    if (in.player->inventory) {
        for (size_t i = 0; i < INV; ++i) {
            const ItemStack& s = in.player->inventory->slots[i];
            if (s.count == 0) continue;
            e.str("SLOT ");
            e.u16(s.id);
            e.sp();
            e.u16(s.count);
            e.eol();
        }
    }
    // Quests: tracked slots in slot order.
    uint16_t nquests = 0;
    for (size_t i = 0; i < QN; ++i) {
        if (in.quests->slots[i].def != QUEST_NONE) ++nquests;
    }
    e.str("QUESTS ");
    e.u16(nquests);
    e.eol();
    for (size_t i = 0; i < QN; ++i) {
        const QuestRuntime& r = in.quests->slots[i];
        if (r.def == QUEST_NONE) continue;
        e.str("QUEST ");
        e.u16(r.def);
        e.sp();
        e.u8(r.state);
        e.sp();
        e.u8(r.objective_idx);
        e.sp();
        e.u16(r.progress);
        e.eol();
    }
    // Language: tracked words in slot order.
    uint16_t nwords = 0;
    for (size_t i = 0; i < LN; ++i) {
        if (in.language->slots[i].id != VOCAB_NONE) ++nwords;
    }
    e.str("LANGUAGE ");
    e.u8(in.language->pair.source);
    e.sp();
    e.u8(in.language->pair.target);
    e.sp();
    e.u8(in.language->level);
    e.sp();
    e.u16(nwords);
    e.eol();
    for (size_t i = 0; i < LN; ++i) {
        const VocabularyProgress& s = in.language->slots[i];
        if (s.id == VOCAB_NONE) continue;
        e.str("WORD ");
        e.u16(s.id);
        e.sp();
        e.u16(s.exposures);
        e.sp();
        e.u16(s.uses);
        e.sp();
        e.u16(s.correct);
        e.sp();
        e.u16(s.incorrect);
        e.eol();
    }
    e.str("END");
    e.eol();
    if (!e.ok) return fail(SaveResult::OVERFLOW);
    if (why) *why = SaveResult::OK;
    return e.n;
}

template <size_t INV, size_t QN, size_t LN>
SaveResult save_read(const char* data, size_t len, GameClock& clock,
                     PlayerState<INV>& player, QuestLog<QN>& quests,
                     LanguageProfile<LN>& language, const QuestBank& qbank,
                     const VocabularyBank& vbank, const ItemBank& ibank) {
    if (!data) return SaveResult::MALFORMED;
    SaveApplySink<INV, QN, LN> sink(clock, player, quests, language, qbank,
                                   vbank, ibank);
    return save_parse(data, len, sink);
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool save_selfcheck();

} // namespace l3d
