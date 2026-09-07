#pragma once
// Roadmap Phase 12: deterministic language-learning state layer.
//
// The language system is a SEPARATE educational layer over the gameplay
// architecture — never a dependency of gameplay subsystems. Gameplay units
// (dialogue/item/quest/...) know nothing of this header; orchestration
// records language events here and the profile accumulates exposure,
// usage and correctness per vocabulary id.
//
// Included downward units (dialogue.h for CEFR/lang codes, item.h for
// item vocabulary tags) know nothing back: one-way layering, no cycles.
// No renderer/platform dependencies. No heap, no exceptions, no STL.
//
// This phase is foundation only: exposure + vocabulary + progress. No
// assessment, no adaptive difficulty, no speech — those read this state
// later without reshaping it.
#include "dialogue.h" // DialogueCEFR / DialogueLang codes (one-way)
#include "item.h"     // ItemDef vocabulary tags (one-way)

#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t VOCAB_NONE = 0xFFFFu;
constexpr size_t VOCAB_MAX_LEMMA = 48;
constexpr size_t LANG_MAX_TAGS = DIALOGUE_MAX_TAGS; // shared tag arity

enum class WordPOS : uint8_t {
    UNKNOWN = 0,
    NOUN,
    VERB,
    ADJ,
    ADV,
    PHRASE,
    PREPOSITION,
    PRONOUN,
    NUMERAL,
    COUNT
};

// Profile-oriented pair: any source/target combination without engine
// changes (Ukrainian->English/German/Polish/Spanish today, anything else
// by adding content rows).
struct LanguagePair {
    uint8_t source{uint8_t(DialogueLang::UNDEFINED)};
    uint8_t target{uint8_t(DialogueLang::UNDEFINED)};
};

inline bool language_pair_valid(const LanguagePair& p) {
    if (p.source == uint8_t(DialogueLang::UNDEFINED) ||
        p.target == uint8_t(DialogueLang::UNDEFINED))
        return false;
    if (p.source >= uint8_t(DialogueLang::COUNT) ||
        p.target >= uint8_t(DialogueLang::COUNT))
        return false;
    return p.source != p.target;
}

struct VocabularyEntry {
    uint16_t id{0};
    const char* lemma{nullptr};
    uint8_t lang{uint8_t(DialogueLang::UNDEFINED)}; // target language
    uint8_t pos{uint8_t(WordPOS::UNKNOWN)};
    uint8_t cefr{uint8_t(DialogueCEFR::UNDEFINED)};
    const char* category{nullptr}; // content label (transport/food/...)
};

struct VocabularyBank {
    const VocabularyEntry* entries{nullptr};
    size_t count{0};
};

const VocabularyEntry* vocab_find(const VocabularyBank& bank, uint16_t id);
bool vocab_validate(const VocabularyEntry& e);
bool vocab_validate_bank(const VocabularyBank& bank, uint16_t* bad_id);

// Minimal deterministic event set. CORRECT/INCORRECT exist for the future
// assessment layer; nothing reports them yet (documented, tested inert).
enum class LanguageEvent : uint8_t {
    SEEN = 0, // encountered (dialogue line shown, item inspected, ...)
    USED,     // player actively engaged (choice taken, item used, ...)
    CORRECT,
    INCORRECT,
    COUNT
};

// Placeholder mastery ladder (deliberately simple; spaced repetition and
// adaptive models arrive later without changing counters):
//   MASTERED  correct >= 5
//   FAMILIAR  correct >= 3
//   LEARNING  exposures >= 3
//   INTRODUCED exposures >= 1
//   NEW       otherwise
enum class Mastery : uint8_t { NEW = 0, INTRODUCED, LEARNING, FAMILIAR, MASTERED };

struct VocabularyProgress {
    uint16_t id{VOCAB_NONE};
    uint16_t exposures{0};
    uint16_t uses{0};
    uint16_t correct{0};
    uint16_t incorrect{0};
};

inline Mastery mastery_of(const VocabularyProgress& s) {
    if (s.correct >= 5) return Mastery::MASTERED;
    if (s.correct >= 3) return Mastery::FAMILIAR;
    if (s.exposures >= 3) return Mastery::LEARNING;
    if (s.exposures >= 1) return Mastery::INTRODUCED;
    return Mastery::NEW;
}

template <size_t N>
struct LanguageProfile {
    static_assert(N >= 1 && N <= 512, "language profile capacity out of range");
    LanguagePair pair{};
    uint8_t level{uint8_t(DialogueCEFR::A1)}; // current CEFR (manual for now)
    VocabularyProgress slots[N]{};
    size_t tracked{0};

    void init(const LanguagePair& p) {
        pair = p;
        level = uint8_t(DialogueCEFR::A1);
        for (size_t i = 0; i < N; ++i) slots[i] = VocabularyProgress{};
        tracked = 0;
    }
    const VocabularyProgress* progress_of(uint16_t id) const {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].id == id) return &slots[i];
        }
        return nullptr;
    }
    bool full() const { return tracked >= N; }

    // Record one event for a bank-known id. Unknown ids and a full profile
    // are ignored (false) — content errors never corrupt progress.
    bool record(const VocabularyBank& bank, uint16_t id, LanguageEvent ev) {
        if (ev >= LanguageEvent::COUNT) return false;
        if (!vocab_find(bank, id)) return false;
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].id == id) {
                apply(slots[i], ev);
                return true;
            }
        }
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].id == VOCAB_NONE) {
                slots[i].id = id;
                apply(slots[i], ev);
                ++tracked;
                return true;
            }
        }
        return false; // profile full
    }

    // Idempotent variant: records SEEN only when the word has no exposures
    // yet (scenario replays must not inflate counts). Returns true when the
    // call changed state.
    bool ensure_seen(const VocabularyBank& bank, uint16_t id) {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].id == id)
                return false; // already known: no double count
        }
        return record(bank, id, LanguageEvent::SEEN);
    }

  private:
    static void apply(VocabularyProgress& s, LanguageEvent ev) {
        if (s.exposures < 0xFFFFu) ++s.exposures;
        if (ev == LanguageEvent::USED && s.uses < 0xFFFFu) ++s.uses;
        if (ev == LanguageEvent::CORRECT && s.correct < 0xFFFFu)
            ++s.correct;
        if (ev == LanguageEvent::INCORRECT && s.incorrect < 0xFFFFu)
            ++s.incorrect;
    }
};

// Orchestration helpers (gameplay -> language events). Unknown tag ids are
// skipped; returns how many events were recorded.
template <size_t N>
size_t lang_observe_dialogue(LanguageProfile<N>& profile,
                             const VocabularyBank& bank,
                             const DialogueNode& node) {
    size_t n = 0;
    for (size_t i = 0; i < node.lang.vocab_count; ++i) {
        if (profile.record(bank, node.lang.vocab[i], LanguageEvent::SEEN))
            ++n;
    }
    return n;
}

template <size_t N>
size_t lang_observe_item(LanguageProfile<N>& profile,
                         const VocabularyBank& bank, const ItemDef& def) {
    size_t n = 0;
    for (size_t i = 0; i < def.vocab_count; ++i) {
        if (profile.record(bank, def.vocab[i], LanguageEvent::SEEN)) ++n;
    }
    return n;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool language_selfcheck();

} // namespace l3d
