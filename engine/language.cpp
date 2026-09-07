// Roadmap Phase 12 translation unit: bank lookup/validation + self-check.
// Unreferenced, language_selfcheck() is dropped by --gc-sections.
#include "language.h"

namespace l3d {

const VocabularyEntry* vocab_find(const VocabularyBank& bank, uint16_t id) {
    if (!bank.entries || id == VOCAB_NONE) return nullptr;
    for (size_t i = 0; i < bank.count; ++i) {
        if (bank.entries[i].id == id) return &bank.entries[i];
    }
    return nullptr;
}

bool vocab_validate(const VocabularyEntry& e) {
    if (e.id == 0 || e.id == VOCAB_NONE) return false;
    if (!e.lemma || !e.lemma[0]) return false;
    size_t len = 0;
    while (e.lemma[len] && len <= VOCAB_MAX_LEMMA) ++len;
    if (len == 0 || len > VOCAB_MAX_LEMMA) return false;
    if (e.lang == uint8_t(DialogueLang::UNDEFINED) ||
        e.lang >= uint8_t(DialogueLang::COUNT))
        return false;
    if (e.pos == uint8_t(WordPOS::UNKNOWN) ||
        e.pos >= uint8_t(WordPOS::COUNT))
        return false;
    if (e.cefr == uint8_t(DialogueCEFR::UNDEFINED) ||
        e.cefr > uint8_t(DialogueCEFR::C2))
        return false;
    if (!e.category || !e.category[0]) return false;
    return true;
}

bool vocab_validate_bank(const VocabularyBank& bank, uint16_t* bad_id) {
    auto fail = [&](uint16_t id) {
        if (bad_id) *bad_id = id;
        return false;
    };
    if (!bank.entries && bank.count != 0) return fail(VOCAB_NONE);
    for (size_t i = 0; i < bank.count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (bank.entries[j].id == bank.entries[i].id)
                return fail(bank.entries[i].id);
        }
        if (!vocab_validate(bank.entries[i])) return fail(bank.entries[i].id);
    }
    return true;
}

bool language_selfcheck() {
    static const char hello[] = "hello";
    static const char food[] = "food";
    static const VocabularyEntry entries[] = {
        {101, hello, uint8_t(DialogueLang::EN), uint8_t(WordPOS::NOUN), // greeting as noun-ish
         uint8_t(DialogueCEFR::A1), "greeting"},
        {201, food, uint8_t(DialogueLang::EN), uint8_t(WordPOS::NOUN),
         uint8_t(DialogueCEFR::A1), "food"},
    };
    static const VocabularyBank bank{entries, 2};
    if (!vocab_validate_bank(bank, nullptr)) return false;
    if (vocab_find(bank, 102) != nullptr) return false;

    LanguagePair pair{uint8_t(DialogueLang::UNDEFINED),
                      uint8_t(DialogueLang::EN)};
    if (language_pair_valid(pair)) return false; // source required
    pair.source = uint8_t(DialogueLang::PL);
    if (!language_pair_valid(pair)) return false; // PL -> EN works
    LanguageProfile<8> prof{};
    prof.init(pair);
    if (!prof.record(bank, 101, LanguageEvent::SEEN)) return false;
    if (!prof.record(bank, 101, LanguageEvent::CORRECT)) return false;
    const VocabularyProgress* s = prof.progress_of(101);
    if (!s || s->exposures != 2 || s->correct != 1) return false;
    if (mastery_of(*s) != Mastery::INTRODUCED) return false;
    if (prof.record(bank, 999, LanguageEvent::SEEN)) return false; // unknown
    if (!prof.ensure_seen(bank, 201)) return false;
    if (prof.ensure_seen(bank, 201)) return false; // idempotent
    return true;
}

} // namespace l3d
