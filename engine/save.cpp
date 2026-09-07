// Roadmap Phase 13 translation unit: integer codec + self-check.
// Unreferenced, save_selfcheck() is dropped by --gc-sections.
#include "save.h"

namespace l3d {

namespace {
bool read_run_digits(const char*& p, const char* end, uint32_t& v, int max_digits) {
    if (p >= end || *p < '0' || *p > '9') return false;
    uint32_t acc = 0;
    int n = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (++n > max_digits) return false;
        acc = acc * 10u + uint32_t(*p - '0');
        ++p;
    }
    v = acc;
    return true;
}
} // namespace

size_t save_write_u32(char* out, size_t cap, uint32_t v) {
    SaveEmitter e{out, cap};
    e.u32(v);
    return e.ok ? e.n : 0;
}

size_t save_write_i16(char* out, size_t cap, int16_t v) {
    SaveEmitter e{out, cap};
    e.i16(v);
    return e.ok ? e.n : 0;
}

bool save_read_u32(const char*& p, const char* end, uint32_t& v) {
    return read_run_digits(p, end, v, 10);
}

bool save_read_u16(const char*& p, const char* end, uint16_t& v) {
    uint32_t wide = 0;
    if (!read_run_digits(p, end, wide, 5) || wide > 0xFFFFu) return false;
    v = uint16_t(wide);
    return true;
}

bool save_read_u8(const char*& p, const char* end, uint8_t& v) {
    uint32_t wide = 0;
    if (!read_run_digits(p, end, wide, 3) || wide > 0xFFu) return false;
    v = uint8_t(wide);
    return true;
}

bool save_read_i16(const char*& p, const char* end, int16_t& v) {
    bool neg = false;
    if (p < end && *p == '-') {
        neg = true;
        ++p;
    }
    uint32_t wide = 0;
    if (!read_run_digits(p, end, wide, 5)) return false;
    if (!neg && wide > 32767u) return false;
    if (neg && wide > 32768u) return false;
    v = int16_t(neg ? -int32_t(wide) : int32_t(wide));
    return true;
}

SaveResult save_validate(const char* data, size_t len) {
    if (!data) return SaveResult::MALFORMED;
    SaveCheckSink sink{};
    return save_parse(data, len, sink);
}

bool save_selfcheck() {
    // Codec round-trips (deterministic, locale-independent).
    char buf[32]{};
    if (save_write_u32(buf, sizeof(buf), 4294967295u) != 10) return false;
    const char* p = buf;
    uint32_t v = 0;
    if (!save_read_u32(p, buf + 10, v) || v != 4294967295u) return false;
    if (save_write_i16(buf, sizeof(buf), -32768) != 6) return false; // "-32768"
    p = buf;
    int16_t s = 0;
    if (!save_read_i16(p, buf + 6, s) || s != -32768) return false;
    if (save_write_u32(buf, 3, 12345) != 0) return false; // overflow path
    p = buf;
    uint8_t b = 0;
    const char bad[] = "300";
    p = bad;
    if (save_read_u8(p, bad + 3, b)) return false; // > 255 rejected

    // Minimal save round-trip through validate + read.
    GameClock clock{};
    clock.day = 2;
    clock.minute = 90;
    PlayerState<4> player{};
    player.init(0x0107);
    player.add_xp(1500);
    Inventory<4> inv{};
    inv.init();
    player.bind_inventory(&inv);
    QuestLog<4> quests{};
    quests.init();
    LanguageProfile<8> lang{};
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    lang.init(pair);
    SaveInput<4, 4, 8> in{&clock, &player, &quests, &lang};
    static char staging[1024]{};
    SaveResult why = SaveResult::MALFORMED;
    const size_t n = save_write(staging, sizeof(staging), in, &why);
    if (n == 0 || why != SaveResult::OK) return false;
    if (save_validate(staging, n) != SaveResult::OK) return false;
    // Exact bytes twice: determinism.
    static char staging2[1024]{};
    if (save_write(staging2, sizeof(staging2), in, nullptr) != n) return false;
    for (size_t i = 0; i < n; ++i) {
        if (staging[i] != staging2[i]) return false;
    }
    GameClock c2{};
    PlayerState<4> p2{};
    Inventory<4> inv2{};
    inv2.init();
    p2.bind_inventory(&inv2);
    QuestLog<4> q2{};
    q2.init();
    LanguageProfile<8> l2{};
    static const QuestBank qb{nullptr, 0};
    static const VocabularyBank vb{nullptr, 0};
    static const ItemBank ib{nullptr, 0};
    if (save_read(staging, n, c2, p2, q2, l2, qb, vb, ib) != SaveResult::OK)
        return false;
    if (c2.day != 2 || c2.minute != 90) return false;
    if (p2.entity != 0x0107 || p2.xp != 1500 || p2.level != 2) return false;
    // Truncated staging never validates (atomic-commit gate).
    if (save_validate(staging, n - 1) != SaveResult::TRUNCATED) return false;
    return true;
}

} // namespace l3d
