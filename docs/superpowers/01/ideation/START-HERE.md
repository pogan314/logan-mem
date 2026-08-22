---
title: Start here — how to use this folder on day one of brainstorming
type: ideation
status: stale
created: "2026-08-21 13:15 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [every file in this folder; the 2026-08-21 readiness review]
---

# Start here

- You are about to brainstorm version 01 of logan-mem. This page says what to read, in what order, what to decide first, and what the session must produce.
- Everything in this folder is ideation. Nothing here is decided. The owner decides; these files only make the decisions easier to see.

## Read in this order (about 30 minutes)

| # | File | Why first |
|---|---|---|
| 1 | `../../../../CLAUDE.md` | The rules of this repo, including what you may and may not do with the old repo |
| 2 | `owner-requirements.md` | The owner's own words. Everything else is interpretation of this |
| 3 | `what-went-wrong.md` | The traps. Read before proposing anything |
| 4 | `findings/drift-analysis.md` | The one-page proof of how a faithful build still failed |
| 5 | `starting-recommendations.md` | Opinions to argue with |
| 6 | `open-questions.md` | The decisions still open, with leanings |
| 7 | `feat/*/README.md` | One feature area at a time, only as needed |
| 8 | `../../../wiki/` | Facts about other systems and about Claude Code, when a question needs one |

## Decide in this order — the first three constrain everything after them

| Order | Decision | Open question # | Why it comes first |
|---|---|---|---|
| 1 | Version 01 size budget (lines, commands, install surfaces) | 1 | Every later "should we include X?" is answered by whether X fits |
| 2 | Where memories live (in-repo folder + user folder, or something else) | 3, 4 | Capture, recall, sharing, and per-repo memory all depend on this |
| 3 | Which hook writes end-of-session memories | 5 | It is the single capture path 01 cannot ship without |
| 4 | Starter memory kinds and the frontmatter fields | 6 | Capture prompts and search are written against these |
| 5 | How `human_verified` gets set | 8 | Small, but it is the owner's one review touchpoint |
| 6 | Episodic: adopt `obra/episodic-memory`, build, or skip in 01 | 2 | Independent of the above; cheap to decide |
| 7 | Spine in 01 or 02 | 9, 10 | Probably 02; decide so nobody builds it by accident |
| 8 | Cross-tool scope, team rules, old-draft import, repo visibility | 11–14 | Can wait until the loop works |

## What the brainstorming session must produce

- One document: `docs/superpowers/01/brainstorming/version-01.md` — a short description of what version 01 is, what it is not, the decisions above with the owner's answer on each, and the success check (how the owner will know it works).
- That document's frontmatter gets `status: ideation` until the owner approves it, then `status: research-fact` is **wrong** for it — use a new status value, `decided`, and add that value to `CLAUDE.md`'s frontmatter list when the first decided doc exists.
- The moment that document exists, this whole `ideation/` folder is stale. Mark every file here `status: stale` in the same commit.

## What the session must NOT produce

- A spec. A plan. Code. A list of tasks for agents. A research round. Any file that starts with "execute".
- More ideation. If the session ends with new ideas and no decisions, it did not happen.

## If you are an agent, not the owner

- You can prepare, summarise, and ask. You cannot decide. Every row in the decision table is the owner's.
- If the owner answers something here in chat, write the answer into `open-questions.md`'s "already answered" table in that same turn, so the next session does not re-ask.
