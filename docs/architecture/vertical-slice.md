# Vertical slice (Roadmap Phase 14, v0.49.0)

Status: playable. A human walks the real 3D maze (WASD + mouse, software
raycaster), finds Anna and the clerk as visible sprites, talks (E),
chooses answers (1-3), completes Getting to the Station, saves (F5),
reloads (F9) — and the greeting changes with earned mastery. No new engine
subsystem was added for this: it composes the existing ones.

## How to play (desktop Linux/Windows)

```text
WASD/arrows + mouse  walk the maze (click captures the mouse)
E near Anna/clerk    talk (prompt shows who is near)
1/2/3                answer in dialogue
F5 / F9              save / load language3d.save
F11                  fullscreen
```

Walkthrough: start (4,4) → explore to the plaza (18,18) → E at Anna →
"Yes" → quest starts → greet again (TALK done) → E at the clerk →
"Yes" (ticket) → E (deliver) → quest claimed (+100 XP, flag) → F5 →
replay Anna twice → she greets you as a familiar speaker.

## Architecture (composition, not new systems)

- `platform/sdl2/scenario.{h,cpp}` owns the slice runtime (all pools,
  clock, sessions) behind a testable, SDL-free interface. The game loop
  feeds it player state, mirrors its NPC positions into `Game::sprites()`
  (mutable accessor — **zero Game changes**), hides consumed keys, and
  draws its panel pre-present.
- `engine/font.h`: zero-dependency 8x8 font (768 B) + clipped text into
  indexed8. Same pixels on every target.
- E/answers route to the slice first; unhandled keys fall through to the
  legacy maze game untouched. Walking away aborts dialogue (ABORTED).
- Variant selection: station mastery ≥ FAMILIAR → `anna_familiar.dlg`
  (A2), else `anna_station.dlg`. Save→reload preserves it: persistence
  visibly affects the next playthrough — the closed learning loop.
- Content convention: dialogue choice 1 is the constructive answer and
  counts as correct language use (v1 proxy, documented in content/).

## Slice simplifications (documented, not hidden)

TALK needs a second greeting (fan-out observes ACTIVE quests only);
REACH/COLLECT auto-report from position/inventory; GIVE bypasses small
talk when the ticket is held; completed quests auto-claim; Anna is
schedule-anchored at the plaza (movement proven separately).

## Verification

`slice_test` plays the whole scenario headlessly (approach → E → answers
→ quest → claim → save → reload → variant flip → panel pixel counts);
`font_test` covers glyphs/clipping. CI smoke still passes; firmware
`.bin` +24 B (F5/F9 key codes), `.bss` unchanged. Two real bugs caught
before release: Windows has no `/tmp` (test save path is now relative +
gitignored), and the panel wrap buffer is hardened for long content.

## Next (Phase 15 — Language-aware dialogue)

More variants per level/topic driven by the same `annaDialogue()`
pattern; then adaptive content over the persisted counters.
