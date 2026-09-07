// Phase 16B CLI: --playtest (Test Center), --headless-playtest,
// --replay. Normal launches pass through untouched (return -1).
#include "playtest_cli.h"

#include "playtest.h"

#include <cstdio>
#include <cstring>

namespace l3d {
namespace playtest {
namespace {

struct LastRun {
    bool valid{false};
    char id[SCENARIO_ID_MAX]{};
    uint32_t seed{1};
    uint32_t hash{0};
    uint32_t failed{0};
    bool passed{false};
    char fail_message[128]{};
    EventLog log{};
    char state[STATE_TEXT_MAX]{};
};

bool run_checked(const ScenarioDesc& d, uint32_t seed, EventLog& log_out,
                 char* state_out, size_t state_cap, uint32_t& hash_out,
                 uint32_t& failed_out, char* msg_out = nullptr,
                 size_t msg_cap = 0) {
    EventLog second{};
    char dummy[STATE_TEXT_MAX]{};
    uint32_t h1 = 0, h2 = 0, f1 = 0, f2 = 0;
    const bool ok1 =
        run_scenario(d, log_out, seed, h1, state_out, state_cap, f1, msg_out, msg_cap);
    const bool ok2 =
        run_scenario(d, second, seed, h2, dummy, sizeof(dummy), f2, nullptr, 0);
    hash_out = h1;
    failed_out = f1 + f2;
    return ok1 && ok2 && h1 == h2;
}

void print_result(const char* id, bool pass, uint32_t failed, const char* msg = nullptr) {
    if (pass)
        std::printf("[PASS] %s\n", id);
    else if (msg && msg[0])
        std::printf("[FAIL] %s (failed_assertions=%u, last: %s)\n", id, failed, msg);
    else
        std::printf("[FAIL] %s (failed_assertions=%u)\n", id, failed);
}

const char* arg_value(int argc, char** argv, const char* flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        size_t k = 0;
        while (flag[k] && flag[k] == argv[i][k]) ++k;
        if (flag[k] == '\0' && argv[i][k] == '\0') return argv[i + 1];
    }
    return nullptr;
}

bool has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        size_t k = 0;
        while (flag[k] && flag[k] == argv[i][k]) ++k;
        if (flag[k] == '\0' && argv[i][k] == '\0') return true;
    }
    return false;
}

uint32_t parse_seed(const char* s, bool& ok) {
    ok = false;
    if (!s || !*s) return 1;
    uint32_t v = 0;
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        if (++n > 10) return 1;
        v = v * 10u + uint32_t(*s - '0');
        ++s;
    }
    if (*s) return 1;
    ok = true;
    return v;
}

int cmd_headless(const ScenarioDesc* only, uint32_t seed, const char* record) {
    Registry& r = registry();
    size_t total = 0, passed = 0;
    for (size_t i = 0; i < r.count; ++i) {
        if (only && &r.items[i] != only) continue;
        ++total;
        EventLog log{};
        char state[STATE_TEXT_MAX]{};
        char msg[128]{};
        uint32_t hash = 0, failed = 0;
        const bool ok = run_checked(r.items[i], seed, log, state, sizeof(state),
                                    hash, failed, msg, sizeof(msg));
        print_result(r.items[i].id, ok, failed, msg);
        if (ok) ++passed;
        if (!ok) {
            char msg[128]{};
            std::snprintf(msg, sizeof(msg), "headless %s", r.items[i].id);
            write_report("playtest-report", r.items[i].id, seed, false, failed,
                         msg, log, state);
        }
    }
    if (total == 0) {
        std::printf("no scenarios matched\n");
        return 2;
    }
    if (record) {
        if (total != 1) {
            std::printf("--record needs exactly one scenario\n");
            return 2;
        }
        // Re-run is unnecessary: record from a fresh verified run.
        EventLog log{};
        char state[STATE_TEXT_MAX]{};
        uint32_t hash = 0, failed = 0;
        if (!run_checked(*only, seed, log, state, sizeof(state), hash, failed))
            return 1;
        ReplayFile rf{};
        size_t k = 0;
        while (only->id[k] && k + 1 < sizeof(rf.scenario)) {
            rf.scenario[k] = only->id[k];
            ++k;
        }
        rf.scenario[k] = '\0';
        rf.seed = seed;
        rf.expected_hash = hash;
        if (!replay_save(record, rf)) {
            std::printf("cannot write %s\n", record);
            return 1;
        }
        std::printf("recorded %s hash=%u\n", record, hash);
    }
    std::printf("%u/%u scenarios passed\n", uint32_t(passed), uint32_t(total));
    return passed == total ? 0 : 1;
}

int cmd_replay(const char* path) {
    ReplayFile rf{};
    if (!replay_load(path, rf)) {
        std::printf("cannot load replay %s\n", path ? path : "(null)");
        return 2;
    }
    uint32_t actual = 0, failed = 0;
    if (!replay_verify(rf, actual, failed)) {
        std::printf("[FAIL] replay %s (hash=%u)\n", rf.scenario, actual);
        return 1;
    }
    std::printf("[PASS] replay %s hash=%u\n", rf.scenario, actual);
    return 0;
}

void show_menu() {
    std::printf("\nLANGUAGE3D PLAYTEST\n");
    Registry& r = registry();
    for (size_t i = 0; i < r.count; ++i)
        std::printf("[%u] %s - %s\n", uint32_t(i + 1), r.items[i].id,
                    r.items[i].name);
    std::printf("[%u] Run All\n[L] Event Log\n[R] Reset\n[P] Replay last run\n"
                "[B] Bug Report\n[Q] Quit\n",
                uint32_t(r.count + 1));
}

int cmd_menu() {
    Registry& r = registry();
    LastRun last{};
    char line[64]{};
    for (;;) {
        show_menu();
        std::printf("> ");
        if (!std::fgets(line, sizeof(line), stdin)) return 0;
        if ((line[0] == 'q' || line[0] == 'Q') && (line[1] == '\n' || !line[1]))
            return 0;
        if ((line[0] == 'l' || line[0] == 'L') && (line[1] == '\n' || !line[1])) {
            if (!last.valid) {
                std::printf("no run yet\n");
                continue;
            }
            for (size_t i = 0; i < last.log.count; ++i) {
                Event e{};
                last.log.at(i, e);
                std::printf("%u %s %u %u %u\n", uint32_t(i),
                            event_name(EventType(e.type)), e.a, e.b, e.c);
            }
            continue;
        }
        if ((line[0] == 'r' || line[0] == 'R') && (line[1] == '\n' || !line[1])) {
            last = LastRun{};
            std::printf("reset\n");
            continue;
        }
        if ((line[0] == 'p' || line[0] == 'P') && (line[1] == '\n' || !line[1])) {
            if (!last.valid) {
                std::printf("no run yet\n");
                continue;
            }
            const ScenarioDesc* d = r.find(last.id);
            if (!d) {
                std::printf("scenario gone\n");
                continue;
            }
            EventLog log{};
            char state[STATE_TEXT_MAX]{};
            uint32_t hash = 0, failed = 0;
            const bool ok =
                run_checked(*d, last.seed, log, state, sizeof(state), hash, failed);
            std::printf(ok && hash == last.hash ? "[PASS] replay %s hash=%u\n"
                                                : "[FAIL] replay %s\n",
                        last.id, hash);
            continue;
        }
        if ((line[0] == 'b' || line[0] == 'B') && (line[1] == '\n' || !line[1])) {
            if (!last.valid) {
                std::printf("no run yet\n");
                continue;
            }
            const bool ok = write_report(
                "playtest-report", last.id, last.seed, last.passed, last.failed,
                last.fail_message[0] ? last.fail_message : "-",
                last.log, last.state);
            std::printf(ok ? "bundle in playtest-report/\n" : "bundle failed\n");
            continue;
        }
        // Numeric choice: scenario index or Run All.
        uint32_t n = 0;
        {
            bool digits = false;
            for (size_t i = 0; line[i] >= '0' && line[i] <= '9'; ++i) {
                digits = true;
                n = n * 10u + uint32_t(line[i] - '0');
            }
            if (!digits || n == 0 || n > r.count + 1) {
                std::printf("unknown choice\n");
                continue;
            }
        }
        if (n == r.count + 1) {
            size_t passed = 0;
            for (size_t i = 0; i < r.count; ++i) {
                EventLog log{};
                char state[STATE_TEXT_MAX]{};
                char msg[128]{};
                uint32_t hash = 0, failed = 0;
                const bool ok = run_checked(r.items[i], 1, log, state,
                                            sizeof(state), hash, failed, msg,
                                            sizeof(msg));
                print_result(r.items[i].id, ok, failed, msg);
                if (ok) ++passed;
            }
            std::printf("%u/%u scenarios passed\n", uint32_t(passed),
                        uint32_t(r.count));
            continue;
        }
        const ScenarioDesc& d = r.items[n - 1];
        EventLog log{};
        char state[STATE_TEXT_MAX]{};
        char msg[128]{};
        uint32_t hash = 0, failed = 0;
        const bool ok =
            run_checked(d, 1, log, state, sizeof(state), hash, failed, msg,
                        sizeof(msg));
        print_result(d.id, ok, failed, msg);
        last.valid = true;
        size_t k = 0;
        while (d.id[k] && k + 1 < sizeof(last.id)) {
            last.id[k] = d.id[k];
            ++k;
        }
        last.id[k] = '\0';
        last.seed = 1;
        last.hash = hash;
        last.failed = failed;
        last.passed = ok;
        last.fail_message[0] = '\0';
        last.log = log;
        k = 0;
        while (state[k] && k + 1 < sizeof(last.state)) {
            last.state[k] = state[k];
            ++k;
        }
        last.state[k] = '\0';
    }
}

} // namespace

int playtest_cli(int argc, char** argv) {
    const bool menu = has_flag(argc, argv, "--playtest");
    const bool headless = has_flag(argc, argv, "--headless-playtest");
    const char* replay = arg_value(argc, argv, "--replay");
    if (!menu && !headless && !replay) return -1;
    register_all_scenarios();
    if (replay && (menu || headless)) {
        std::printf("use --replay alone\n");
        return 2;
    }
    if (replay) return cmd_replay(replay);
    if (menu && headless) {
        std::printf("use --playtest or --headless-playtest, not both\n");
        return 2;
    }
    if (menu) return cmd_menu();
    bool seed_ok = true;
    uint32_t seed = 1;
    if (has_flag(argc, argv, "--seed")) {
        seed = parse_seed(arg_value(argc, argv, "--seed"), seed_ok);
        if (!seed_ok) {
            std::printf("bad --seed value\n");
            return 2;
        }
    }
    const char* scenario = arg_value(argc, argv, "--scenario");
    const char* record = arg_value(argc, argv, "--record");
    const bool all = has_flag(argc, argv, "--all");
    if (scenario && all) {
        std::printf("use --scenario or --all, not both\n");
        return 2;
    }
    const ScenarioDesc* only = nullptr;
    if (scenario) {
        only = registry().find(scenario);
        if (!only) {
            std::printf("unknown scenario %s\n", scenario);
            return 2;
        }
    }
    return cmd_headless(only, seed, record);
}

} // namespace playtest
} // namespace l3d
