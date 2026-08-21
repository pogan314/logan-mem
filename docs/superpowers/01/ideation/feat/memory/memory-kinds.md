---
title: Memory kinds — what is worth remembering
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md #9, findings/live-store-evidence.md, the 29 inbox drafts at /home/ubuntu/.pogan/memory/org/members/lgerard42/inbox/ read 2026-08-21]
---

# Memory kinds

- The owner called the kinds list "the biggest failure" — not because the four kinds were wrong, but because only one (build failure) ever got written.
- So the question is not "what is the perfect taxonomy?" It is "what kinds of thing happen in a session that the next session would pay to know?" Each kind below names a **moment** that can trigger capture, so capture can be designed per kind.
- Kinds are a plain string. Start with this list, add freely, never migrate.

## Starter kinds (argue with these)

| Kind | The moment it captures | Example from real work | Who usually writes it |
|---|---|---|---|
| `decision` | "We chose X over Y because Z" was said or done | "Put memories inside the repo so clone = share" | agent, at Stop |
| `failure` | Something was tried and did not work, with the cause if known | "`gh pr create` has no `--json` flag; parse the URL from stdout" | agent, on a failed tool call or at Stop |
| `fix` | A failure got resolved — what actually fixed it | "Org invite grants read, not write; needs an explicit collaborator grant" | agent, when a retry succeeds |
| `recipe` | A sequence of steps that worked and will be needed again | "To join a second machine: install, auth gh as the same account, run init --join" | agent, at Stop |
| `fact` | A true thing about this project, tool, or environment that is not in the code | "Mutagen refuses absolute symlink targets" | agent, when discovered |
| `preference` | The owner expressed how they want things done | "Bullets, not paragraphs, in every doc" | agent, when the owner says it |
| `convention` | How this repo does something, by agreement | "Every file under docs/ has YAML frontmatter with status and updated" | agent or human |
| `gotcha` | A trap that looks fine and is not | "Deleting an MCP entry does not kill the already-running server process" | agent, at Stop |
| `status` | Where a piece of work stands right now | "Spine is deferred to version 02 unless under 500 lines" | agent, at Stop; expires fast |
| `question` | Something unresolved that the owner must decide | "Line budget for 01: 3k, 5k, or 8k?" | agent |

## Kinds deliberately not on the list

- `instinct` (the ECC idea of a confidence-scored habit). Confidence that never decays is a known failure mode; start without scores.
- `observation` (raw tool-event logs). That is transcript history, which the episodic layer already indexes. Memories are distilled, not raw.
- `standard` (a team rule ratified by two people). A rule is a `convention` with `scope: team` and `human_verified: true`.

## How to know the list is working

- After a week of normal use, count memories by kind. If one kind is over ~70% of the total, capture for the others is broken — which is exactly what happened last time (100% build failures).
- The owner's weekly five-minute glance is the quality check. A kind that never produces a memory the owner keeps is a kind to drop.
