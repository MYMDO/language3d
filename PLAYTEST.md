# Language3D v0.51.0 — Adaptive Dialogue Playtest

> **Experimental playtest build, not a finished game.** v0.50.0 verified
> response evaluation; v0.51.0 adds the first adaptive learning loop:
> your earned mastery changes how Anna speaks to you. Please report what
> breaks — usability feedback (P2 below) is currently the most valuable.

## Download

Get the archive for your platform from the
[Releases page](../../releases) (`language3d-0.51.0-…`):

| Platform | File | Notes |
|---|---|---|
| Linux x86_64 | `language3d-0.51.0-linux-x86_64.tar.gz` | unpack, run `./language3d-…/language3d` |
| Windows x86_64 | `language3d-0.51.0-windows-x86_64.zip` | unpack, run `language3d.exe`; self-contained, no redistributable needed |
| RP2040 (Pico) | `language3d-0.51.0-rp2040.uf2` | drag onto the `RPI-RP2` drive (see `docs/platforms/rp2040.md` for wiring) |

## Controls (desktop)

Click the window to capture the mouse (`Esc` releases it).

| Key | Action |
|---|---|
| `W`/`S`, `↑`/`↓` | move forward / back |
| `A`/`D`, `Q`/`C` | strafe left / right |
| `←`/`→`, mouse | turn |
| `E` near Anna / clerk | talk (a prompt appears when someone is near) |
| `1`–`3` | answer in dialogue |
| `F5` / `F9` | save / load (`language3d.save` in the working directory) |
| `F11` | fullscreen |

## What to play

Start at the maze entrance, explore east to the plaza (around 18,18),
talk to **Anna** ("Yes, of course."), talk to her again, walk to the
**clerk**, take the **ticket** ("Yes, please." — try "I like trains."
first to see a wrong answer held open), hand it over, then save with
`F5`. Talk to Anna twice more: once she trusts your station directions,
she greets you with harder lines. Save, quit, reload with `F9` — the
harder greeting persists.

## Feedback levels (copy into your report)

```text
P0 — functionality
[ ] Game launches, 3D maze renders, movement + mouse look work
[ ] Anna and the clerk are visible as characters
[ ] E opens dialogue near an NPC (prompt shows beforehand)
[ ] Answers 1-3 advance dialogue; a wrong answer keeps it open
[ ] Quest starts, progresses (talk → station → ticket → handover)
[ ] Quest completes with +100 XP; F5 saves, F9 restores

P1 — language system
[ ] Helpful answers score CORRECT, smalltalk PARTIAL, off-topic INCORRECT
[ ] Mastery visibly grows with correct answers
[ ] After 3 helpful Anna talks, her greeting changes (A2 variant)
[ ] Repeat interaction keeps working after the change
[ ] Save/load preserves the new greeting

P2 — usability (most valuable right now)
[ ] Is it clear what to press and when?
[ ] Is it clear WHY an answer was wrong?
[ ] Is the dialogue-variant change noticeable?
[ ] Is the quest goal understandable?
```

## Known limitations (by design at this stage)

- Answer scoring is **intent matching on authored choices, not NLP**:
  the game understands which scripted reply you picked, not free text.
- `PARTIAL` answers (e.g. polite smalltalk) advance the dialogue; only a
  clearly off-topic answer holds it open.
- Quest flow is simplified (talk twice for TALK, auto-claim on completion).
- No enemies, trading, multiplayer, audio, or settings yet.
- The 3D view is a technical prototype: coarse wall textures, large flat
  surfaces, compact HUD. Functionality first, presentation later.
- RP2040 CI proves the firmware **builds**; display/input still need
  real-hardware verification — report your board results.

## Expected behavior

Correct answers advance dialogue and quests; off-topic answers get a
"Hmm, that doesn't help" and the NPC asks again; quitting without `F5`
loses progress; deleting `language3d.save` starts fresh.

## How to report a bug

Open a [GitHub issue](../../issues/new) with: OS + version, downloaded
file name, what you did (checklist step), what happened vs. expected,
and `language3d.save` if save/load is involved. Console output
(`[ASSET]`/`[ARCH]` lines) helps a lot.
