---
title: Live pogan store evidence
type: ideation
status: ideation
created: "2026-08-21 13:02 CDT"
updated: "2026-08-21 13:02 CDT"
version: "01"
sources: [live read-only inspection of /home/ubuntu/.pogan/memory/ on 2026-08-21]
---

# Live pogan store evidence

Read-only inspection of `/home/ubuntu/.pogan/memory/` on 2026-08-21. Nothing under `/home/ubuntu/.pogan/` was modified.

## Record counts per world (`*.md` files outside `events/`)

| World | Count | Breakdown |
|---|---|---|
| personal | 4 | `user/facts/` (1), `user/recipes/` (1), `projects/pogan-live-check/facts/` (2, both `state: deactivated`) |
| org | 30 | `members/lgerard42/facts/` (1, active) + `members/lgerard42/inbox/` (29, all unreviewed drafts) |
| shared | 2 | `standards/` (1, active) + `categories/categories.md` (1, glossary infra, not a record) |

## Titles of every filed (non-inbox) record

- personal/projects/pogan-live-check/facts: "live verification session ran on ec2-lg-main" (deactivated)
- personal/projects/pogan-live-check/facts: "subagent capture works in live session" (deactivated)
- personal/user/facts: "pogan store initialized on ec2-lg-main" (active)
- personal/user/recipes: "Join a second machine to the pogan store" (active)
- org/members/lgerard42/facts: "ec2-lg-main is lgerard42's primary host" (active)
- shared/standards: "Join a second machine to the pogan store" (active, ratified copy of the personal recipe)

## Inbox drafts

29 drafts sit in `/home/ubuntu/.pogan/memory/org/members/lgerard42/inbox/`. First 10 by filename:

1. "`gh pr create` has no --json flag — parse the URL out of stdout"
2. Accepting a GitHub org invite grants base read, not repo write
3. Release gates need live-credential checks — a green stub suite proves nothing
4. pogan ratify must resolve queued promotions in both org and personal worlds
5. Prove a guard test can fail — mutate the line, don't trust a green suite
6. pogan search mints one session file per run, breaking session-shape rules
7. ai-configs sync edits each machine's live settings.json, so deletions undo
8. GitHub 'Approve' isn't real until Submit review — verify PR state live
9. Builder briefs must name the repo's typecheck script, never a bare tsc
10. Thread the path a write returned; don't re-derive it with readdirSync

## Event log totals (`events/2026-08/*.jsonl`, all worlds, 50 files, 111 events)

By type: retrieved 49, pull 17, injected 14, injection-budget-spent 11, captured 9, empty-search 4, published 2, deactivated 2, promotion-proposed 2, ratified 1.

By day: 2026-08-08 (28), 2026-08-09 (37), 2026-08-10 (32), 2026-08-11 (10), 2026-08-12 (3), 2026-08-21 (1). No shared-world event files exist.

## Filed vs. inbox

- The 6 filed/live records are bootstrap/meta notes about the tool's own setup, e.g. the "pogan store initialized on ec2-lg-main" fact body: "Host ec2-lg-main (this EC2 box) is lgerard42's primary work machine and the first host in the org memory store."
- The 29 inbox drafts (produced by a `distill-mine-trial` run against a real transcript) are specific engineering lessons, e.g. the `gh pr create` draft body opens: "pogan-mem's shared PR-opening helper called `gh pr create --json number,url`... Real `gh` (measured on 2.94.0) rejects that flag."
- None of the 29 inbox drafts have been triaged into the real store — every one is still an unreviewed draft as of this read.
