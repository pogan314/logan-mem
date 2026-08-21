---
title: In-run learning — the coordinator that stops repeating mistakes
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md #5, feat/memory/capture-paths.md]
---

# In-run learning

- Scenario: a coordinator runs a plan with many executor subagents over hours. Executors keep hitting the same wall. Today the coordinator re-briefs the next executor the same way and the wall gets hit again.
- Goal: the coordinator **notices** and **changes its brief**, automatically, mid-run.

## The loop (idea)

```
executor N fails ──► coordinator logs one line: what failed, what was tried
                          │
                          ▼
        before dispatching executor N+1:
        read the run log ──► same failure seen before?
                          │
              no ─────────┼───────── yes
              │                       │
       dispatch as planned     change the brief: add the warning,
                               the constraint, or a different approach;
                               log that the strategy changed
                          │
                          ▼
        run ends ──► Stop-time capture turns repeated failures into
                     durable memories (gotcha / convention / fix)
                     so the NEXT run's coordinator starts with them
```

## What "notice" needs

- A run-scoped log the coordinator writes to and reads from. Plain text, one line per failure, deleted or archived at run end. Not a memory — memories are the distilled output.
- A cheap similarity test: "is this the same failure?" Start with the dumbest thing — same error string, same file, or same tool — and only get smarter if the dumb version misses.
- A rule in the coordinator's own instructions: "Read the run log before every dispatch. A failure seen twice changes the brief."

## What "change the brief" can mean

- Add a one-line warning to the executor prompt ("do not use `--json` on `gh pr create`; parse stdout").
- Add a constraint ("run the type-check before reporting done").
- Swap the approach ("stop retrying the API; use the CLI").
- Stop and ask the owner — if the same failure survives two brief changes, that is a signal the plan is wrong, not the executors.

## What survives the run

- Every failure that repeated becomes a durable memory in the repo's memory folder, written by the same Stop-time capture as everything else (`../memory/capture-paths.md`).
- Every brief change becomes a `convention` candidate: "executors in this repo must …". The owner's weekly glance decides if it sticks.
- Over time, this is how per-repo harness components get their raw material (`harness-components.md`).

## Why this needs almost no new code

- The coordinator already exists (it is a Claude session with a plan). The log is a file. The rule is a paragraph in its prompt. The Stop-time capture already has to exist for everything else.
- The expensive version — clustering failures, scoring strategies — is a version 02+ question, if the dumb version proves insufficient.
