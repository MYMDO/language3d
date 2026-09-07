# Language system foundation (Roadmap Phase 12, v0.47.0)

Status: implemented. Deterministic vocabulary/exposure/progress state over
the previously inert metadata. No assessment, no adaptive difficulty, no
speech — those read this state later without reshaping it.

## A separate layer, not a dependency

```text
                GAME (orchestration: tests now, game loop later)
                 │
      ┌──────────┼──────────┐
      ▼          ▼          ▼
   Dialogue    Quest      Items
      │          │          │
      │     topic│          │
      ▼          ▼          ▼
   vocab[]    topic=1    vocab[]
      │          │          │
      └──────────┼──────────┘
                 ▼
          Language System
          (pair/profile/bank/events)
```

`language.h` includes `dialogue.h`/`item.h` one way only (codes + tag
arrays); no gameplay unit includes `language.h`. No renderer/platform
dependencies.

## Data model

- `LanguagePair{source, target}` — any combination, validated (concrete,
  distinct). No hardcoded English; Ukrainian arrives as content rows.
- `VocabularyEntry{id, lemma, lang, pos, cefr, category}` — 32 B flash.
- `VocabularyBank` — static/generated (`content/vocabulary/*.vocab` →
  `tools/build_vocab.py` → tables), validated at build + CI.
- `LanguageProfile<N>` — pair, CEFR level, fixed progress slots.
- `VocabularyProgress{id, exposures, uses, correct, incorrect}` — 10 B.
- Events `SEEN / USED / CORRECT / INCORRECT`; unknown ids and full
  profiles are ignored, never corrupt. `ensure_seen` is idempotent for
  scenario replays. CORRECT/INCORRECT exist but nothing reports them yet
  (assessment phase wires them; tested inert-today at unit level).
- Mastery ladder (placeholder, documented): NEW → INTRODUCED (≥1 exp) →
  LEARNING (≥3) → FAMILIAR (≥3 correct) → MASTERED (≥5 correct).

## Orchestration semantics (tested)

NPC node entry → SEEN its tags; player choice → USED the entered node's
tags; item receipt → SEEN its tags. Quest scenario linkage is topic-level
(`topic=1` transportation on Getting to the Station); topic→vocab
expansion is future work. First content: 18-entry Ukrainian→English
A1/A2 set actually referenced by the station scenario (every dialogue and
item tag resolves — asserted by test).

## Memory & footprint

`Profile<32>` = **336 B**, `<64>` = **656 B SRAM**; bank = 32 B/word flash.
Vocabulary tables linked into host tests only (verified absent from the
firmware map). Firmware `.bin`: **60436 → 60436 (+0 B)**. Desktop: no
measurable change. Tests: 31 → 33.

## First learning loop (proven by language_scenario_test)

Greet Anna (hello/help SEEN) → ask (station SEEN+USED) → walk quest →
take ticket (ticket SEEN) → station reaches LEARNING while quest hits
CLAIMED. Gameplay state and learning state advance side by side.

## Next (Phase 13 — Save / Progression, then adaptive)

Persist profile + world state (PlayerPersistent pattern extends here);
then assessment-driven CORRECT/INCORRECT, repetition and adaptive
difficulty over these exact counters.
