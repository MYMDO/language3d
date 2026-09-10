# AGENTS.md — Language3D contributor notes

Project: portable fixed-point 3D language-learning game (Linux / Windows / RP2040+ST7789).
Single version source: `VERSION` file. Releases only on `v*` tags (`release.yml`).

## Mission (Pareto rules)

Transform the MVP into the smallest convincing, playable, publicly releasable
3D game/demo. Product context: `docs/PRODUCT.md`; done means `docs/MVP.md`;
what's next lives in `docs/ROADMAP.md` — read the relevant one before acting.

- At every step, work on the highest **Impact × Confidence / Effort** item.
  Priority order: broken functionality → core loop → input/controls → player
  clarity → stability → minimal UI → content for the loop → build reliability →
  polish → optional features. Never work below a higher-priority blocker.
- **Pareto gate**: before implementing, confirm the task is in the highest
  current impact class. A correctness/stability blocker outranks the formula.
- Small, reversible changes; reuse existing systems; no speculative
  architecture, no rewrites without measurable benefit, no new dependencies
  without justification. "Cool" is not a criterion.
- Verify every meaningful change (build, relevant tests, behavior check,
  regression scan) and never claim success without verification.
- Token discipline: read only task-relevant files, prefer targeted search,
  reuse `docs/architecture/` as context cache instead of re-deriving it.

## Build & test (exact commands)

```bash
cmake --preset linux-release && cmake --build --preset linux-release
ctest --preset linux-release            # full suite (36 tests)
make test-all                           # legacy wrapper, same suite
make <name>-test                        # single test, e.g. make slice-test
PICO_SDK_PATH=$HOME/pico-sdk ./tools/build-rp2040.sh   # firmware (always clean dir)
python3 tools/build_dialogue.py --check content/dialogues  # content validation
```

- CMake is canonical. `Makefile` game binary lags `CMakeLists.txt` engine sources — keep both in sync when adding `.cpp` files.
- New engine `.cpp` must be added to: `LANGUAGE3D_ENGINE_SOURCES` (host), RP2040 target list, one `Makefile` test rule, and often the game `SRC`.
- New tests: register in `CMakeLists.txt` (`language3d_add_test`), `Makefile` (rule + `test-all` + `.PHONY`), nothing else needed — CI picks them up via CTest.
- New content (`.dlg`/`.item`/`.quest`/`.vocab`): add generator rule + `validate-content` job entry; content tables link into host tests/game ONLY, never firmware (verify: `grep -c generated_ build-rp2040/*.map` must be 0).

## Hard-won gotchas (all verified by real CI failures)

- **Display profiles are per-binary**: tests build with NO `L3D_*_PROFILE` define (240×240 default); only the game defines `L3D_PC_PROFILE`, firmware `L3D_RP2040_PROFILE`. Never link profile-mismatched objects (ODR break on `Config`).
- **`pico_sdk_import.cmake` must precede `project()`** or ARM toolchain detection breaks.
- **GCC ≤13 ICE** on `slots[i] = Slot{}` for structs containing arrays inside templates — use field-wise reset instead.
- **MSVC**: `OVERFLOW` is a system macro (use `NO_SPACE` et al.); `main` needs `(argc, argv)` for SDL2main; `.ps1` files must be pure ASCII (no em-dash — parser breaks without BOM).
- **Windows has no `/tmp`**: tests must use relative paths and clean up after themselves (`*.save` is gitignored).
- Test link rules are manual: `math.cpp` (TrigLut), `item.cpp` (`item_find`), `language.cpp` (`vocab_find`) — add as needed or you get `undefined reference`, not a compile error.
- RP2040 firmware baseline: `.bin` **60436** bytes. Any growth needs justification in the commit/PR. `arm-none-eabi-size` + `.map` inspection is the workflow.
- CI has a reproducibility guard: no `/home/…` or `/mnt/data` paths in build files.

## Architecture invariants (do not break)

- `engine/` has zero platform includes (grep-verifiable). Dependency direction is always `platform → engine`, `orchestration → engines`, never cycles (`dialogue.h` ⇄ `quest.h` must stay mutually unaware; glue lives in `dialogue_quest.h`).
- No heap / exceptions / RTTI / virtual dispatch in `engine/`; fixed pools, generational u16 ids, caller-owned storage.
- **Append new struct fields at the END** — aggregate initializers across tests/content would silently miscompile otherwise.
- Name collisions already resolved: legacy sprite pool is `SpritePool` (`entities.h`), gameplay identity is `EntityPool` (`entity.h`); legacy 2D `NPC` (`core.h`) vs `NPCAgent`.
- Gameplay `Game` class is legacy maze behavior — the vertical slice (`platform/sdl2/scenario.*`, SDL-free and unit-tested) drives the new systems without modifying it.
- Content is data: extend `.dlg`/`.item`/`.quest`/`.vocab` + generators, never hardcode trees/items in C++. Intent ids, vocab tags, location tags share opaque content namespaces documented in `content/*/README.md`.

## Conventions

- Commits: `feat(...)`, `fix(...)`, `docs(...)`, `ci(...)` — one logical phase per commit, pushed to `main` (CI must be 4/4 green). No tags except releases.
- Docs live in `docs/architecture/` (one file per subsystem) + roadmap phase table in `docs/architecture.md` (roadmap phases ≠ semver — keep them separate).
- Finish every milestone with: full local build+tests, clean RP2040 build + size check, `git status` clean, structured report.
