# Playtest & QA framework (Roadmap Phase 16B, v0.51.0+)

Status: implemented. Permanent project infrastructure, not a debug menu.

## Layering (the one rule that matters)

```text
Playtest (platform/sdl2/playtest/)
   ↓ drives
public gameplay/API surfaces (Scenario, engines, save codec)
```

GameCore, renderer and platform backends include nothing from here —
production builds work identically with these files unlinked (proven by
firmware artifact inspection, see below). No LLM, NLP, network
telemetry, runtime scripting, or heap: all storage is fixed-capacity.

## Components

- **Modes** (`playtest_cli.cpp`): `language3d --playtest` opens the
  console Test Center; `--headless-playtest [--scenario ID | --all]
  [--seed N] [--record FILE]` runs without SDL; `--replay FILE`
  re-executes a recording and compares hashes. Normal launch is
  untouched (unknown args boot the game).
- **Scenarios** (`scenarios.cpp`): 8 deterministic gameplay checks —
  `basic-movement`, `npc-interaction`, `dialogue-evaluation`,
  `adaptive-dialogue`, `quest-flow`, `inventory`, `save-load`,
  `vertical-slice`. Each is a pure function of (fixture, ops): fresh
  locals per run, fixed timesteps, no wall-clock input. They call real
  gameplay paths (walk_move, interact_target, dq_choose, quest_report,
  save codec, the real `Scenario` class, real generated content banks).
- **Fixtures**: caller-owned bounded pools + real content; direct state
  presets (inventory stock, mastery counts) are the only allowed
  shortcut, and only for setup — never around the behavior under test.
  There is no RNG in the engines; `--seed` is recorded for future use.
- **Assertions** (`playtest.h/.cpp`): generic `assert_true`/`assert_eq`
  plus thin gameplay wrappers. Every check logs `ASSERTION_PASS/FAIL`
  with its order; failures print `expected/actual` context
  (`[FAIL] id (failed_assertions=N, last: <what>)`).
- **Event log**: 256-entry fixed ring of `{type, a, b, c}` (~20 gameplay
  event types). Engines stay log-free; the runner/scenarios log around
  real calls. FNV-1a hash over logical order; every scenario runs twice
  and must hash identically (determinism proof).
- **Replay** (v1, honest scope): `.l3dr` = versioned text
  (`scenario` + `seed` + expected hash). Record = dump after a verified
  run; playback = re-execute + compare. This reproduces deterministic
  input scenarios for bug reports; full op-step recording is future work.
- **Bug-report bundle** (`playtest-report/`): `report.txt` (version,
  platform, scenario, seed, result, failure, hash), `events.log`,
  `scenario.txt`, `state.txt`, `replay.l3dr`. Written on headless
  failure and on Test Center `[B]`; uploaded as a CI artifact on failure.
  No secrets, home paths, or personal data.
- **Test Center**: console menu — run one/all, `[L]` event log,
  `[R]` reset, `[P]` replay last, `[B]` bundle, `[Q]` quit. Text UI
  only, available in playtest mode only.

## What it does NOT do

No LLM/NLP, no network telemetry, no remote QA service, no runtime
scripting, no unrestricted allocation, no P2 usability automation
(human playtest stays human — see `PLAYTEST.md`).

## Verification

- `ctest` includes `playtest` (all 8 through one runner, no SDL).
- CI runs Headless Playtest on Linux + Windows; failure uploads the
  bundle. RP2040 stays build-tested (no hardware playtest claims).
- Firmware proof: playtest sources are absent from the RP2040 target;
  `.bin` size and `.bss` unchanged — checked per milestone.

## Desktop vs embedded

Everything above is desktop-only (`platform/sdl2/`). On RP2040 there is
no CLI: the same scenario logic could later run from a compile-time
debug entry point against bounded buffers, but that is explicitly out
of scope until real hardware testing exists.
