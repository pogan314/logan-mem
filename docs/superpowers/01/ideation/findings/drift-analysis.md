---
title: Drift analysis — what was asked vs what was built
type: ideation
status: ideation
created: "2026-08-21 13:03 CDT"
updated: "2026-08-21 13:03 CDT"
version: "01"
sources: [owner-quotes-timeline.md, what-was-built.md, live-store-evidence.md, /home/ubuntu/.pogan/memory/*/events/ read on 2026-08-21]
---

# Drift analysis

- Question answered: how far did the old build drift from what the owner asked for?
- Answer: **almost no drift on features, total drift on shape.** Nearly everything asked for got built. The thing the owner cared about most — automatic learning — got wired backwards into a manual queue.
- Evidence rows cite the owner's dated words (see `owner-quotes-timeline.md`) and the code or live state checked on 2026-08-21.

## The scoreboard

| Owner asked for (date) | What shipped | Verdict |
|---|---|---|
| "the full elite version in v1" (07-30); no V1/V2 split (07-31) | Everything in one release: vectors, graph, spine, miner, encryption, multi-machine sync | Delivered literally — and it is the direct cause of the over-engineering |
| AI auto-writes and auto-filters memories; human approval only to turn a memory into a skill (08-08) | Mined drafts, quick captures, and failure nudges all route to `inbox/` needing triage. Only the MCP `mem_log` tool writes live | **Missed** — the review gate landed one level too low |
| "heavy manual input defeats the entire fucking purpose" (08-11) | The spec's acceptance gate budgets **45 minutes per week** of owner triage | **Inverted** — manual work became a designed-in requirement |
| Rejected the capture trigger (test fails / build breaks / user corrects you) as "NOTHING like what I wanted" (08-11) | Contract widened in a later commit | Fixed too late — capture had already stopped on 08-09 |
| One plugin, nothing scattered (08-11) | Six install surfaces; `uninstall` cannot remove its own MCP entry; re-enable is two undocumented commands | **Missed** — the consolidation plan (T2–T10) was never executed |
| Cross-tool memory: Codex, Gemini, Cursor, Kimi, Grok (08-02) | Plain-file store and MCP are portable; every capture and injection path is a Claude Code hook | **Missed** in practice |
| Org-level memory comparing members to improve the harness (08-01) | Member / offboard / key machinery built; 1 org record exists; the second machine never joined | Built, never populated |
| "elite-level, extremely high-performing" or worthless (08-08) | Supermemory's benchmark suite missed the pre-declared recall floor (0.229) | **Missed**, still open in the old ledger |
| Graph engineering plus a queryable spine (07-30) | Spine build / modules / html, 2-hop graph expansion, hand-seeded categories | Delivered |
| No hard deletes, only deactivation, except by the owner (07-30) | Enforced; 2 records sit deactivated | Delivered |

## Three numbers that explain the outcome

1. **Capture happened on 2 days, not 5.** The event log holds 110 events across 2026-08-08 → 08-12 (plus 1 stray `injection-budget-spent` event on 08-21 from the hours the toolkit was accidentally reinstalled — 111 total). `captured` events appear on 08-08 (7, all from the release-gate check itself) and 08-09 (2). Nothing after. The system's own success gate required ≥5 capture days.
2. **6 memories filed, 29 stuck.** The 6 filed records are all about the tool installing itself. The 29 drafts in `/home/ubuntu/.pogan/memory/org/members/lgerard42/inbox/` are real engineering lessons. Mining worked; the gate in front of it did not.
3. **The acceptance test was unrunnable.** Clause 4 needed a random 10-record sample rated by the owner. There were never 10 filed records.

## Why capture stopped (owner's statement, not inference)

> "I stopped it because it was only capturing like build failures and not capturing literally anything else that would actually help me."

- Matches the code: the only capture path that fires without the model choosing to call a tool is the Bash-failure counter in `src/plugin/hooks/postToolUse.ts`.

## What this means for version 01

- The owner's requirements were clear and stable. The build did not misread them; it re-shaped them to fit a review-heavy architecture it had already committed to.
- The fix is not "read the requirements harder." It is: build the automatic capture → live memory → recall loop **first**, show the owner the memories, and let nothing else in until that loop produces memories the owner calls useful.
