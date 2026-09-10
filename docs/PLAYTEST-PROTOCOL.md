# Language3D — External Playtest Protocol (v0.51.1)

Organizer-side procedure for the first blind playtest. No code, gameplay, or
`PLAYTEST.md` behavioral changes are part of this. `PLAYTEST.md` remains the
directed-testing checklist; this document covers only what it cannot: blind
onboarding observation and how a raw observation becomes a triaged issue.

## 1. Test package (minimal hand-off set)

Send the tester exactly this — nothing about the engine or architecture:

1. The v0.51.1 archive for their platform (from the GitHub Release page):
   `language3d-0.51.1-linux-x86_64.tar.gz` or `language3d-0.51.1-windows-x86_64.zip`.
2. `PLAYTEST.md` (v0.51.1, from the repo — it is NOT inside the archive).
3. The tester note below, saved as `README-TEST.txt` next to the unpacked game.
4. Linux only: one prerequisite line — the binary dynamically links SDL2:
   `sudo apt install libsdl2-2.0-0` (Debian/Ubuntu) or
   `sudo dnf install SDL2` (Fedora). Windows is self-contained.

Do NOT send the packaged `README.md`: it is a developer document with dead
links (it references `docs/`, `PLAYTEST.md`, build files absent from the
archive). Replacing it inside the archives is a packaging change reserved for
a future `v0.51.x` — for this round the sidecar `README-TEST.txt` covers it.

Verified hand-off readiness: unpacked Linux archive runs its own self-check
with no source tree present (`language3d --headless-playtest --all` → 9/9).

## 2. Tester note (copy verbatim — no coaching, no hints)

```text
Language3D v0.51.1 — playtest build, not a finished game.

Run: ./language3d            (station story)
     ./language3d --scenario shop   (market story, play after the first)

This is a blind test: try to figure the game out on your own first.
There are no wrong actions — getting stuck is useful data, keep going
or stop whenever you like (about 15-20 minutes per story is plenty).

After playing, reply with: where you got stuck, what you thought you
were supposed to do, what was unclear, whether you finished anything,
and what you did afterwards. OS + steps to reproduce for anything broken.
```

## 3. Session procedure (per tester, per scenario)

1. **Blind phase (10–20 min).** Tester plays with only the note above.
   Observer stays silent: no hints, no corrections. Record timestamps of
   every stuck point (>60 s without progress) and every surprise.
2. **Directed phase.** Hand over `PLAYTEST.md`; tester works the P0/P1/P2
   checklists for that scenario (station, then shop).
3. **Debrief** (the 8 questions): where stuck / what they thought the goal
   was / what was unclear / did they find interaction / did they understand
   the objective / did they finish / what after finishing / would they replay.
4. Collect artifacts for anything broken: OS, archive name, console output,
   `language3d.save` / `language3d-shop.save` if save/load is involved.

Target: 5–10 people who have never seen Language3D, covering both scenarios.

## 4. What data to record (observation sheet, one row per finding)

`tester / scenario / phase (blind|directed) / observation (quote) / stuck-time /
expected-vs-actual / artifacts attached / suspected class (P0|P1|P2|Arch)`

## 5. Promotion criteria (observation → triaged issue)

- **P0 functional**: the launch→result chain is uncompletable (crash, hang,
  dead control, corrupted save). Requires: deterministic repro steps or ≥2
  independent testers + artifacts. File immediately.
- **P1 language/learning**: scoring or adaptive-variant behavior contradicts
  the design in `docs/architecture/response-evaluation.md` /
  `adaptive-dialogue.md`. Requires: dialogue transcript (choices + verdicts)
  + expected vs actual + save file.
- **P2 usability**: friction that slows but does not block. Requires: observed
  in ≥2 testers independently (blind phase), or 1 tester + organizer confirms
  it is not a coaching artifact. Requires: quote + location + stuck-time.
  Single-tester remarks without stuck-time stay raw notes, not issues.
- **Architectural**: only when a P0/P1 reproduces in BOTH scenarios (shared
  runtime suspect). Requires evidence from station AND shop.

No finding becomes a roadmap task without passing its bar above. Priority
between qualified issues follows `docs/ROADMAP.md` (P0 > P1 > confirmed P2).

## 6. Sufficiency bar for round 1

The round is analyzable when: ≥5 blind sessions happened, both scenarios were
played blind at least twice each, and every P0 claim has repro artifacts.
Below that bar the outcome is "collect more", never "implement".
