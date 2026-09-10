# Language3D — MVP definition and status (vs v0.51.1)

The MVP is successful when a new user can launch, understand, play, complete,
understand the result, and replay a short but complete experience.

## Criteria vs v0.51.1 (evidence-linked, no optimism)

| # | Criterion | Verdict | Evidence |
|---|---|---|---|
| 1 | Launch the game | ✅ | Linux/Windows builds run; see `PLAYTEST.md` Download |
| 2 | Understand what to do without developer help | ⚠️ | In-game HUD objective line exists (`Scenario::objectiveText`), but there is no tutorial or intro; first-time players currently depend on `PLAYTEST.md` |
| 3 | Move around | ✅ | Movement + collision covered by headless scenarios; `PLAYTEST.md` Controls |
| 4 | Interact with the world | ✅ | E-talk with prompt, answers 1–3, GIVE handoff; covered by tests |
| 5 | Complete a simple objective | ✅ | Both quests completable end-to-end (station +100 XP, apple +50 XP); covered by `slice_test`, `shop_slice_test`, headless slice scenarios |
| 6 | Understand the result | ✅ | Claim line (`"<quest> claimed. +N XP"`) and `Quest complete!` message shown in HUD |
| 7 | Restart / replay | ⚠️ | No in-game restart; fresh start requires deleting the save file; replay exists only via recordings (`--replay`) and headless reruns |

## Reading guide

- ✅ = verified in v0.51.1 (tests and/or headless and/or manual playtest).
- ⚠️ = partially true; the gap is a candidate for P2 (usability) triage.
- ❌ = not present. None of the 7 criteria is ❌.

The two ⚠️ items (first-time guidance, in-game restart) must be confirmed or
rejected by real P0/P1/P2 feedback before any implementation — see `ROADMAP.md`.
