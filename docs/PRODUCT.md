# Language3D — Product

Portable fixed-point 3D language-learning game (Linux / Windows / RP2040+ST7789).
Current public build: **v0.51.1 prerelease** ("Integrated Playtest, Two Scenarios") —
the recommended build for external playtesting. `v0.51.0` is kept as a historical build.

## What ships in v0.51.1

- **Scenario A — Getting to the Station** (`station`, default launch).
- **Scenario B — An Apple for Anna** (`shop`, `--scenario shop`).
- Shared Scenario runtime (`ScenarioDef`): dialogue, response evaluation
  (CORRECT/PARTIAL/INCORRECT), adaptive dialogue variants, quests,
  inventory, per-scenario save/load.
- Integrated Playtest & QA Framework: Test Center (`--playtest`),
  headless regression (`--headless-playtest --all`, 9/9), replay verification.
- Builds: Linux x86_64, Windows x86_64, RP2040 firmware (.uf2/.bin/.hex + debug bundle).

## Authoritative docs (do not duplicate — link, don't copy)

- Playtest guide: `PLAYTEST.md` (controls, P0/P1/P2 checklists, bug reporting).
- Architecture: `docs/architecture/` (one file per subsystem) + roadmap phase table
  in `docs/architecture.md` (roadmap phases ≠ semver).
- History: `docs/CHANGELOG.md`. Contributor rules: `AGENTS.md`.
- Product layer (this file + siblings): `docs/MVP.md` (what "done" means),
  `docs/ROADMAP.md` (what next).

## Product direction

A small, technically interesting, lightweight 3D game/demo with a clear playable
loop and language-learning value. The portable engine is an advantage, not the
product by itself. Public release target: Linux + Windows builds, simple controls,
stable startup, short playable experience, screenshots, short gameplay video,
README, reproducible build. Steam is a later distribution target, not the current
objective.
