// Roadmap Phase 12 tests: pair/entry/bank/profile/events/mastery,
// content references, isolation, determinism, selfcheck.
#include "../engine/language.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>

using namespace l3d;

namespace l3d {
const VocabularyBank content_vocabulary(); // generated_vocab.cpp
}

static const VocabularyBank mini_bank() {
    static const char hello[] = "hello";
    static const char hilfe[] = "Hilfe";
    static const VocabularyEntry entries[] = {
        {101, hello, uint8_t(DialogueLang::EN), uint8_t(WordPOS::NOUN),
         uint8_t(DialogueCEFR::A1), "greeting"},
        {102, hilfe, uint8_t(DialogueLang::DE), uint8_t(WordPOS::NOUN),
         uint8_t(DialogueCEFR::A1), "help"},
    };
    static const VocabularyBank bank{entries, 2};
    return bank;
}

int main() {
    // --- language pairs: no hardcoded English, any combination ---
    LanguagePair uk_en{uint8_t(DialogueLang::UNDEFINED),
                       uint8_t(DialogueLang::EN)};
    L3D_REQUIRE(!language_pair_valid(uk_en)); // source required
    uk_en.source = uint8_t(DialogueLang::UNDEFINED); // still invalid
    L3D_REQUIRE(!language_pair_valid(uk_en));
    LanguagePair pl_de{3, 2}; // PL -> DE by raw codes
    L3D_REQUIRE(language_pair_valid(pl_de));
    LanguagePair same{uint8_t(DialogueLang::EN), uint8_t(DialogueLang::EN)};
    L3D_REQUIRE(!language_pair_valid(same)); // ...but self-pairs rejected
    LanguagePair uk_de{99, uint8_t(DialogueLang::DE)};
    L3D_REQUIRE(!language_pair_valid(uk_de)); // unknown source
    uk_en.source = 99; // NOTE: 99 is not a defined language either
    L3D_REQUIRE(!language_pair_valid(uk_en));

    // (DialogueLang has no Ukrainian code yet: profile tests below use
    // PL->EN as the concrete pair; adding UK is content-only work.)
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    L3D_REQUIRE(language_pair_valid(pair));

    // --- generated bank validates; entries match authored content ---
    const VocabularyBank bank = content_vocabulary();
    L3D_REQUIRE(bank.count == 18);
    uint16_t bad = 0;
    L3D_REQUIRE(vocab_validate_bank(bank, &bad));
    const VocabularyEntry* station = vocab_find(bank, 103);
    L3D_REQUIRE(station && std::strcmp(station->lemma, "station") == 0);
    L3D_REQUIRE(station->lang == uint8_t(DialogueLang::EN));
    L3D_REQUIRE(station->pos == uint8_t(WordPOS::NOUN));
    L3D_REQUIRE(station->cefr == uint8_t(DialogueCEFR::A1));
    L3D_REQUIRE(std::strcmp(station->category, "transport") == 0);
    const VocabularyEntry* thanks = vocab_find(bank, 105);
    L3D_REQUIRE(thanks && std::strcmp(thanks->lemma, "thank you") == 0);
    L3D_REQUIRE(vocab_find(bank, 999) == nullptr);

    // --- progress lifecycle + mastery ladder ---
    LanguageProfile<16> prof{};
    prof.init(pair);
    L3D_REQUIRE(prof.level == uint8_t(DialogueCEFR::A1));
    L3D_REQUIRE(prof.progress_of(103) == nullptr);
    L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::SEEN));
    const VocabularyProgress* s = prof.progress_of(103);
    L3D_REQUIRE(s && s->exposures == 1 && mastery_of(*s) == Mastery::INTRODUCED);
    L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::SEEN));
    L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::USED));
    s = prof.progress_of(103);
    L3D_REQUIRE(s->exposures == 3 && s->uses == 1);
    L3D_REQUIRE(mastery_of(*s) == Mastery::LEARNING);
    for (int i = 0; i < 3; ++i)
        L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::CORRECT));
    L3D_REQUIRE(mastery_of(*prof.progress_of(103)) == Mastery::FAMILIAR);
    for (int i = 0; i < 2; ++i)
        L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::CORRECT));
    L3D_REQUIRE(mastery_of(*prof.progress_of(103)) == Mastery::MASTERED);
    L3D_REQUIRE(prof.record(bank, 103, LanguageEvent::INCORRECT));
    s = prof.progress_of(103);
    L3D_REQUIRE(s->incorrect == 1 && s->exposures == 9);

    // --- idempotent replay guard ---
    L3D_REQUIRE(prof.ensure_seen(bank, 201));
    L3D_REQUIRE(!prof.ensure_seen(bank, 201));
    L3D_REQUIRE(prof.progress_of(201)->exposures == 1);

    // --- unknown ids never corrupt progress ---
    L3D_REQUIRE(!prof.record(bank, 999, LanguageEvent::SEEN));
    L3D_REQUIRE(prof.progress_of(999) == nullptr);
    L3D_REQUIRE(!prof.record(bank, 103, LanguageEvent::COUNT)); // bad event

    // --- isolation: mini bank knows nothing of the content bank ---
    const VocabularyBank mini = mini_bank();
    L3D_REQUIRE(mini.count == 2);
    L3D_REQUIRE(vocab_find(mini, 103) == nullptr);
    LanguageProfile<4> iso{};
    iso.init(pair);
    L3D_REQUIRE(iso.record(mini, 102, LanguageEvent::SEEN));
    L3D_REQUIRE(iso.progress_of(102)->exposures == 1);
    L3D_REQUIRE(!iso.record(mini, 103, LanguageEvent::SEEN));

    // --- profile capacity is bounded and honest ---
    LanguageProfile<2> tiny{};
    tiny.init(pair);
    L3D_REQUIRE(tiny.record(mini, 101, LanguageEvent::SEEN));
    L3D_REQUIRE(tiny.record(mini, 102, LanguageEvent::SEEN));
    L3D_REQUIRE(tiny.full());
    L3D_REQUIRE(prof.record(mini, 101, LanguageEvent::SEEN)); // other profile unaffected

    // --- determinism: same event stream, same counters ---
    LanguageProfile<16> a{}, b{};
    a.init(pair);
    b.init(pair);
    const LanguageEvent seq[] = {LanguageEvent::SEEN, LanguageEvent::USED,
                                 LanguageEvent::CORRECT, LanguageEvent::SEEN};
    for (int run = 0; run < 2; ++run) {
        LanguageProfile<16>& v = (run == 0) ? a : b;
        for (size_t i = 0; i < 4; ++i) v.record(bank, 110, seq[i]);
    }
    const VocabularyProgress* sa = a.progress_of(110);
    const VocabularyProgress* sb = b.progress_of(110);
    L3D_REQUIRE(sa && sb && sa->exposures == sb->exposures && sa->uses == sb->uses);
    L3D_REQUIRE(sa->correct == sb->correct && mastery_of(*sa) == mastery_of(*sb));

    // --- validator negatives ---
    {
        static const char l[] = "x";
        static const VocabularyEntry dup[] = {
            {1, l, 1, 1, 1, "c"},
            {1, l, 1, 1, 1, "c"},
        };
        static const VocabularyBank dup_bank{dup, 2};
        uint16_t bad_id = 0;
        L3D_REQUIRE(!vocab_validate_bank(dup_bank, &bad_id) && bad_id == 1);
        static const VocabularyEntry bad_lang{2, l, 0, 1, 1, "c"};
        L3D_REQUIRE(!vocab_validate(bad_lang));
        static const VocabularyEntry bad_pos{3, l, 1, 0, 1, "c"};
        L3D_REQUIRE(!vocab_validate(bad_pos));
        static const VocabularyEntry no_lemma{4, nullptr, 1, 1, 1, "c"};
        L3D_REQUIRE(!vocab_validate(no_lemma));
    }

    L3D_REQUIRE(language_selfcheck());

    std::printf("language OK\n");
    return 0;
}
