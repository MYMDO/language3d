# Adaptive dialogue content (Roadmap Phase 16A, v0.51.0)

Status: implemented. Deterministic rules-based variant selection over the
persisted `LanguageProfile` — not semantic/NLP/LLM evaluation. The verdict
machinery of Phase 15 is untouched; this phase closes its feedback loop.

## Model

Dialogues sharing a nonzero `variant_group` are interchangeable
presentations of one conversation. Each candidate declares an optional
`(require_word, require_mastery)` gate; the requirement-free entry is the
guaranteed fallback:

```text
profile + bank + group
  → eligible = fallback OR mastery(word) >= required
  → pick highest required mastery, ties → lowest dialogue id
  → none eligible and no fallback → DIALOGUE_NONE
```

`dq_select_variant()` is a pure function (no session, no heap). Legacy
group-0 dialogues are never selected through it — open by id as before,
byte-identical behavior. Thresholds are exact (`>=`), tested at
below/exact/above.

## Slice usage (generalized, not hardcoded)

`Scenario::annaDialogue()` no longer branches on a word: it selects over
Anna's group 1, where dialogue 1 is the fallback and dialogue 3 requires
station ≥ FAMILIAR. Adding a B1 variant later is a content row
(`variant=1 require=103:5`), not a code change — the function-per-NPC
pattern is contained to this single call site by construction.

## Content syntax

`dialogue <id> npc=<tag> [variant=<group>] [require=<word>:<level>]`,
level 0–4 (`NEW..MASTERED`); a level with no word is rejected by the
generator. Documented in `content/dialogues/README.md`.

## Verification

`dialogue_quest_test`: fallback-first, exact-threshold promotion,
highest-tier-wins, order-independent tie-break, missing-variant NONE,
legacy group-0 NONE. Slice flow (including the save→reload variant flip)
passes unchanged through the generalized selector. Firmware `.bin`
unchanged (60460 B).

## Next

Mastery-gated quest branches and adaptive content expansion over the same
selector; assessment-driven CORRECT/INCORRECT remain the only writers of
the counters it reads.
