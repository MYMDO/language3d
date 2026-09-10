# Language3D — Roadmap (short, triage-driven)

No new gameplay phases until real P0/P1/P2 feedback from **both** v0.51.1
scenarios is collected and triaged. Changes until then: `v0.51.x` patches only.

## Now

1. **Collect**: external playtest of v0.51.1 (`PLAYTEST.md` checklists) on both
   scenarios. File every finding as an issue labeled P0 / P1 / P2 / Architectural.
2. **Triage** (per issue, in this order):
   - P0 functional break → fix in `v0.51.x`, verify with test + headless + CI.
   - P1 language/learning defect → content fix preferred over engine change.
   - P2 usability gap → smallest clarification first (text/prompt/docs); no new
     systems without evidence it blocks completion.
   - Architectural → record in `docs/architecture/` + `DECISIONS.md` equivalent;
     implement only if a P0/P1 depends on it.
3. **Release**: patch `v0.51.x` prereleases as needed; never rewrite a published release.

## Pareto gate (binding)

Before implementing anything, check: is this in the highest current impact class
(P0 break > P1 defect > confirmed P2 blocker)? If not, do not implement it.
Priority = Impact × Confidence / Effort — but a correctness/stability blocker
outranks the formula, it never waits for scoring.

## Explicit non-goals (until triage says otherwise)

No new scenarios, no new engine mechanics, no visual polish pass, no Steam work,
no rewrites of working systems. New work must trace to a triaged playtest finding.

## Next milestone decision

After triage: either continue `v0.51.x` quality iterations or define the next
milestone from the findings — not from speculation. The decision and its reasons
go into `docs/architecture.md` (phase table) and `CHANGELOG.md`.
