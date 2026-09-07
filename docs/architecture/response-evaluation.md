# Response evaluation (Roadmap Phase 15, v0.50.0)

Status: implemented. Deterministic, data-driven scoring of player
responses — no LLM, no speech, no heap. The v1 position proxy
("choice 1 is correct") is gone, replaced by intent matching.

## Model

```text
DialogueChoice
 ├── intent         what the player means (opaque content id, 0 = none)
 ├── vocab/grammar what the response practices
DialogueNode
 ├── expect_primary / expect_secondary   accepted intents (0 = legacy)
```

`dq_evaluate(node, choice)` is a pure function: primary match → CORRECT,
secondary → PARTIAL, otherwise INCORRECT; nodes without expectations yield
NONE and keep the legacy USED-only path (old content works unchanged).

## Flow consequences (the world reacts)

- CORRECT / PARTIAL → advance normally (entered-node effect applies).
- INCORRECT → the session HOLDS on the current node: the NPC asks again.
  The slice surfaces this as "Hmm, that doesn't help. Try again."
- Every evaluated pick records USED for the choice vocabulary plus
  CORRECT/INCORRECT per verdict — the events the mastery ladder consumes.

Example (clerk): "Yes, please." (request-ticket) → CORRECT → ticket;
"Just looking." (smalltalk) → PARTIAL → polite farewell, no ticket;
"I like trains." (offtopic) → INCORRECT → asked again, nothing given.
Same gameplay goal, linguistically differentiated handling.

## Layering (unchanged)

Evaluation lives in `dialogue_quest.h` next to the other orchestration —
the dialogue and quest engines stay mutually unaware. Scoring writes
only to `LanguageProfile`; quest/inventory effects flow through the
existing entered-node path. Assessment, repetition and adaptive
difficulty arrive later over these exact verdicts.

## Verification

`dialogue_quest_test` covers shuffled positions (CORRECT at index 1),
PARTIAL advance, INCORRECT hold + scoring, and legacy NONE;
`slice_test` plays a wrong clerk answer (no ticket, still open) before
the correct one; `playable_quest_test` and `language_scenario_test`
pass unchanged (legacy content path). Firmware `.bin` unchanged.

## Next (Phase 16 — Adaptive dialogue/content)

Mastery-gated variant selection generalized beyond `annaDialogue()`:
same goal, different linguistic presentation, driven by persisted
counters.
