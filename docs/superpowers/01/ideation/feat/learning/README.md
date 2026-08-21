---
title: feat/learning — automatic learning, as the owner defines it
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md #1 #4 #5 #6, what-went-wrong.md, docs/wiki/claude-code-harness-facts.md]
---

# feat/learning

- The owner's defining example: "a coordinator coordinating a long running autonomous execution and executors keep making the same mistake so the coordinator changes their strategy instead of repeating that mistake indefinitely."
- That is not "promote a memory to a wider store", which is what the old build made. It is a **feedback loop inside a run**, plus a way for what the loop learned to outlive the run.
- Files: `in-run-learning.md` (the coordinator loop), `harness-components.md` (turning memories into skills / hooks / rules — the separate system where a human gate belongs).

## Two different things that got confused last time

| | In-run learning | Harness components |
|---|---|---|
| What it is | A coordinator notices a repeated failure during one long run and changes what it tells executors | A human reviews accumulated memories and turns some into an executing thing: a skill, a hook, a rule file, an agent definition |
| Time scale | Minutes to hours, inside one run | Weeks; a deliberate act |
| Human involved? | No. That is the point | Yes — the output executes, so someone signs off |
| Where it lives | Run-scoped notes, then durable memories at run end | A separate subsystem with its own folder and a review step |
| Did the old build have it? | No | No (it had "promote to a wider store", which is neither) |

## The smallest version that would satisfy the owner's example

- The coordinator keeps a run-scoped notes file (not a memory yet): every executor failure, one line, with what was tried.
- Before dispatching each new executor, the coordinator reads that file. If the same failure appears twice, it must change the brief — add a warning, a constraint, or a different approach — and write down that it did.
- At run end, a Stop-time pass turns the notes into durable memories: each repeated failure becomes a `gotcha` or `convention` in the repo's memory folder, so the **next** run's coordinator starts with it.
- No new infrastructure. A file, a rule in the coordinator's prompt, and the normal Stop-time capture.
