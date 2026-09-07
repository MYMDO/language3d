// Phase 16B core implementation (desktop-only, fixed buffers, stdio for
// replay/report files only — never in the gameplay path).
#include "playtest.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace l3d {
namespace playtest {

const char* event_name(EventType t) {
    switch (t) {
        case EventType::GAME_START: return "GAME_START";
        case EventType::GAME_RESET: return "GAME_RESET";
        case EventType::PLAYER_MOVE: return "PLAYER_MOVE";
        case EventType::NPC_INTERACT: return "NPC_INTERACT";
        case EventType::DIALOGUE_START: return "DIALOGUE_START";
        case EventType::DIALOGUE_CHOICE: return "DIALOGUE_CHOICE";
        case EventType::RESPONSE_EVALUATED: return "RESPONSE_EVALUATED";
        case EventType::LANGUAGE_EVENT: return "LANGUAGE_EVENT";
        case EventType::VARIANT_SELECTED: return "VARIANT_SELECTED";
        case EventType::QUEST_STATE_CHANGED: return "QUEST_STATE_CHANGED";
        case EventType::QUEST_OBJECTIVE_CHANGED: return "QUEST_OBJECTIVE_CHANGED";
        case EventType::ITEM_ADDED: return "ITEM_ADDED";
        case EventType::ITEM_REMOVED: return "ITEM_REMOVED";
        case EventType::SAVE: return "SAVE";
        case EventType::LOAD: return "LOAD";
        case EventType::SCENARIO_START: return "SCENARIO_START";
        case EventType::SCENARIO_END: return "SCENARIO_END";
        case EventType::ASSERTION_PASS: return "ASSERTION_PASS";
        case EventType::ASSERTION_FAIL: return "ASSERTION_FAIL";
        default: break;
    }
    return "NONE";
}

void EventLog::clear() {
    for (size_t i = 0; i < LOG_CAP; ++i) events[i] = Event{};
    head = 0;
    count = 0;
    seq = 0;
}

void EventLog::log(EventType t, uint16_t a, uint16_t b, uint16_t c) {
    events[head].type = uint8_t(t);
    events[head].a = a;
    events[head].b = b;
    events[head].c = c;
    head = (head + 1) % LOG_CAP;
    if (count < LOG_CAP) ++count;
    ++seq;
}

bool EventLog::at(size_t i, Event& out) const {
    if (i >= count) return false;
    out = events[(head + LOG_CAP - count + i) % LOG_CAP];
    return true;
}

uint32_t EventLog::hash() const {
    uint32_t h = 2166136261u; // FNV-1a 32
    for (size_t i = 0; i < count; ++i) {
        Event e{};
        at(i, e);
        h ^= e.type;
        h *= 16777619u;
        h ^= e.a & 0xFFu;
        h *= 16777619u;
        h ^= (e.a >> 8) & 0xFFu;
        h *= 16777619u;
        h ^= e.b & 0xFFu;
        h *= 16777619u;
        h ^= (e.b >> 8) & 0xFFu;
        h *= 16777619u;
        h ^= e.c & 0xFFu;
        h *= 16777619u;
        h ^= (e.c >> 8) & 0xFFu;
        h *= 16777619u;
    }
    h ^= count & 0xFFu;
    h *= 16777619u;
    return h;
}

void assert_begin(AssertCtx& ctx, EventLog& log) {
    ctx.log = &log;
    ctx.order = 0;
    ctx.failed = 0;
    ctx.last_message[0] = '\0';
}

static void record_assert(AssertCtx& ctx, bool ok, const char* what) {
    ++ctx.order;
    if (ctx.log)
        ctx.log->log(ok ? EventType::ASSERTION_PASS : EventType::ASSERTION_FAIL,
                     uint16_t(ctx.order & 0xFFFFu), 0, 0);
    if (!ok) {
        ++ctx.failed;
        size_t i = 0;
        while (what[i] && i + 1 < sizeof(ctx.last_message)) {
            ctx.last_message[i] = what[i];
            ++i;
        }
        ctx.last_message[i] = '\0';
    }
}

bool assert_true(AssertCtx& ctx, bool cond, const char* what) {
    record_assert(ctx, cond, what);
    return cond;
}

bool assert_eq_u32(AssertCtx& ctx, uint32_t actual, uint32_t expected,
                   const char* what) {
    record_assert(ctx, actual == expected, what);
    return actual == expected;
}

bool assert_dialogue_state(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                           const char* what) {
    return assert_eq_u32(ctx, actual, expected, what);
}

bool assert_verdict(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                    const char* what) {
    return assert_eq_u32(ctx, actual, expected, what);
}

bool assert_quest_state(AssertCtx& ctx, uint8_t actual, uint8_t expected,
                        const char* what) {
    return assert_eq_u32(ctx, actual, expected, what);
}

Registry& registry() {
    static Registry r{};
    return r;
}

bool register_scenario(const char* id, const char* name,
                       bool (*run)(AssertCtx&, EventLog&, char*, size_t,
                                   uint32_t)) {
    if (!id || !name || !run) return false;
    ScenarioDesc d{};
    size_t i = 0;
    while (id[i] && i + 1 < sizeof(d.id)) {
        d.id[i] = id[i];
        ++i;
    }
    d.id[i] = '\0';
    if (id[i] != '\0') return false;
    i = 0;
    while (name[i] && i + 1 < sizeof(d.name)) {
        d.name[i] = name[i];
        ++i;
    }
    d.name[i] = '\0';
    if (name[i] != '\0') return false;
    d.run = run;
    return registry().add(d);
}

bool Registry::add(const ScenarioDesc& d) {
    if (count >= MAX_SCENARIOS || !d.run || !d.id[0]) return false;
    items[count++] = d;
    return true;
}

const ScenarioDesc* Registry::find(const char* id) const {
    if (!id) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        size_t k = 0;
        while (id[k] && id[k] == items[i].id[k]) ++k;
        if (id[k] == '\0' && items[i].id[k] == '\0') return &items[i];
    }
    return nullptr;
}

static void copy_msg(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    size_t i = 0;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

bool run_scenario(const ScenarioDesc& d, EventLog& log, uint32_t seed,
                  uint32_t& out_hash, char* state_text, size_t state_cap,
                  uint32_t& out_failed, char* fail_msg_out, size_t fail_cap) {
    log.clear();
    log.log(EventType::SCENARIO_START, 0, 0, 0);
    AssertCtx ctx{};
    assert_begin(ctx, log);
    const bool ok = d.run ? d.run(ctx, log, state_text, state_cap, seed) : false;
    log.log(EventType::SCENARIO_END, ok ? 1 : 0, uint16_t(ctx.failed & 0xFFFFu), 0);
    out_hash = log.hash();
    out_failed = ctx.failed;
    copy_msg(fail_msg_out, fail_cap, ctx.last_message);
    (void)seed;
    return ok && ctx.failed == 0;
}

bool run_scenario_twice(const ScenarioDesc& d, uint32_t seed,
                        uint32_t& out_hash, char* state_text, size_t state_cap,
                        uint32_t& out_failed, char* fail_msg_out, size_t fail_cap) {
    EventLog first{};
    uint32_t h1 = 0, f1 = 0;
    if (!run_scenario(d, first, seed, h1, state_text, state_cap, f1, fail_msg_out,
                      fail_cap)) {
        out_hash = h1;
        out_failed = f1;
        return false;
    }
    EventLog second{};
    char dummy[STATE_TEXT_MAX]{};
    uint32_t h2 = 0, f2 = 0;
    if (!run_scenario(d, second, seed, h2, dummy, sizeof(dummy), f2, nullptr, 0)) {
        out_hash = h2;
        out_failed = f2;
        return false;
    }
    out_hash = h1;
    out_failed = f1 + f2;
    return h1 == h2 && f2 == 0;
}

static bool write_all(const char* path, const char* data, size_t len) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t w = len ? std::fwrite(data, 1, len, f) : 0;
    std::fclose(f);
    return w == len;
}

bool replay_save(const char* path, const ReplayFile& rf) {
    if (!path) return false;
    char buf[192]{};
    int n = std::snprintf(buf, sizeof(buf), "L3DREPLAY %u\nscenario %s\nseed %u\nhash %u\nEND\n",
                          rf.version, rf.scenario, rf.seed, rf.expected_hash);
    if (n <= 0 || size_t(n) >= sizeof(buf)) return false;
    return write_all(path, buf, size_t(n));
}

static bool read_line(const char*& p, const char* end, char* out, size_t cap) {
    size_t i = 0;
    while (p < end && *p != '\n') {
        if (i + 1 < cap) out[i++] = *p;
        ++p;
    }
    if (p >= end) return false; // truncated: no newline
    ++p;
    out[i] = '\0';
    return true;
}

static bool parse_u32(const char* s, uint32_t& v) {
    if (!*s) return false;
    uint32_t acc = 0;
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        if (++n > 10) return false;
        acc = acc * 10u + uint32_t(*s - '0');
        ++s;
    }
    if (*s || n == 0) return false;
    v = acc;
    return true;
}

bool replay_load(const char* path, ReplayFile& rf) {
    if (!path) return false;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[256]{};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    if (n == 0 || n >= sizeof(buf) - 1) return false;
    buf[n] = '\0';
    const char* p = buf;
    const char* end = buf + n;
    char line[96]{};
    if (!read_line(p, end, line, sizeof(line))) return false;
    uint32_t ver = 0;
    {
        // "L3DREPLAY <version>"
        const char* s = line;
        while (*s && *s != ' ') ++s;
        if (*s != ' ') return false;
        if (!parse_u32(s + 1, ver)) return false;
        char head[10]{};
        size_t i = 0;
        while (line[i] && line[i] != ' ' && i + 1 < sizeof(head)) {
            head[i] = line[i];
            ++i;
        }
        head[i] = '\0';
        if (std::strcmp(head, "L3DREPLAY") != 0) return false;
    }
    if (ver != REPLAY_VERSION) return false;
    rf = ReplayFile{};
    rf.version = ver;
    if (!read_line(p, end, line, sizeof(line))) return false;
    if (std::strncmp(line, "scenario ", 9) != 0) return false;
    size_t i = 0;
    while (line[9 + i] && i + 1 < sizeof(rf.scenario)) {
        rf.scenario[i] = line[9 + i];
        ++i;
    }
    rf.scenario[i] = '\0';
    if (!rf.scenario[0] || line[9 + i] != '\0') return false;
    if (!read_line(p, end, line, sizeof(line))) return false;
    if (std::strncmp(line, "seed ", 5) != 0) return false;
    if (!parse_u32(line + 5, rf.seed)) return false;
    if (!read_line(p, end, line, sizeof(line))) return false;
    if (std::strncmp(line, "hash ", 5) != 0) return false;
    if (!parse_u32(line + 5, rf.expected_hash)) return false;
    if (!read_line(p, end, line, sizeof(line))) return false;
    if (std::strcmp(line, "END") != 0) return false;
    return true;
}

bool replay_verify(const ReplayFile& rf, uint32_t& actual_hash,
                   uint32_t& out_failed) {
    const ScenarioDesc* d = registry().find(rf.scenario);
    if (!d) return false;
    char state[STATE_TEXT_MAX]{};
    if (!run_scenario_twice(*d, rf.seed, actual_hash, state, sizeof(state),
                            out_failed))
        return false;
    return actual_hash == rf.expected_hash && out_failed == 0;
}

static bool make_dir(const char* path) {
#ifdef _WIN32
    return ::_mkdir(path) == 0;
#else
    return ::mkdir(path, 0755) == 0;
#endif
}

static bool join_path(char* out, size_t cap, const char* dir, const char* file) {
    size_t i = 0;
    while (dir[i] && i + 1 < cap) {
        out[i] = dir[i];
        ++i;
    }
    if (i + 1 < cap) out[i++] = '/';
    size_t k = 0;
    while (file[k] && i + 1 < cap) {
        out[i++] = file[k++];
    }
    out[i] = '\0';
    return file[k] == '\0';
}

bool write_report(const char* dir, const char* scenario_id, uint32_t seed,
                  bool passed, uint32_t failed, const char* fail_message,
                  const EventLog& log, const char* state_text) {
    if (!dir || !scenario_id) return false;
    make_dir(dir); // best effort: may already exist
    char path[REPORT_PATH_MAX]{};
    char body[2048]{};
    // report.txt
    int n = std::snprintf(
        body, sizeof(body),
        "Language3D playtest report\nscenario: %s\nseed: %u\nresult: %s\n"
        "failed_assertions: %u\nfailure: %s\nevents: %u\nlog_hash: %u\n",
        scenario_id, seed, passed ? "PASS" : "FAIL", failed,
        fail_message ? fail_message : "-", uint32_t(log.count), log.hash());
    if (n <= 0 || size_t(n) >= sizeof(body)) return false;
    if (!join_path(path, sizeof(path), dir, "report.txt")) return false;
    if (!write_all(path, body, size_t(n))) return false;
    // events.log
    {
        char ev[LOG_CAP * 40]{};
        size_t pos = 0;
        for (size_t i = 0; i < log.count; ++i) {
            Event e{};
            log.at(i, e);
            int m = std::snprintf(ev + pos, sizeof(ev) - pos, "%u %s %u %u %u\n",
                                  uint32_t(i), event_name(EventType(e.type)),
                                  e.a, e.b, e.c);
            if (m <= 0 || pos + size_t(m) >= sizeof(ev)) break;
            pos += size_t(m);
        }
        if (!join_path(path, sizeof(path), dir, "events.log")) return false;
        if (!write_all(path, ev, pos)) return false;
    }
    // scenario.txt
    {
        char sc[128]{};
        int m = std::snprintf(sc, sizeof(sc), "scenario: %s\nseed: %u\n",
                              scenario_id, seed);
        if (m <= 0 || size_t(m) >= sizeof(sc)) return false;
        if (!join_path(path, sizeof(path), dir, "scenario.txt")) return false;
        if (!write_all(path, sc, size_t(m))) return false;
    }
    // state.txt
    if (!join_path(path, sizeof(path), dir, "state.txt")) return false;
    if (!write_all(path, state_text ? state_text : "",
                   state_text ? std::strlen(state_text) : 0))
        return false;
    // replay.l3dr
    ReplayFile rf{};
    size_t k = 0;
    while (scenario_id[k] && k + 1 < sizeof(rf.scenario)) {
        rf.scenario[k] = scenario_id[k];
        ++k;
    }
    rf.scenario[k] = '\0';
    rf.seed = seed;
    rf.expected_hash = log.hash();
    if (!join_path(path, sizeof(path), dir, "replay.l3dr")) return false;
    return replay_save(path, rf);
}

} // namespace playtest
} // namespace l3d
