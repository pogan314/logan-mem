---
title: Capture paths — how memories get written with no human in the way
type: ideation
status: stale
created: "2026-08-21 13:14 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [owner-requirements.md #1 #2, what-went-wrong.md, docs/wiki/claude-code-harness-facts.md, docs/wiki/obra-episodic-memory.md]
---

# Capture paths

- Proposed rule one (backed by owner requirement #1 and #2): **every path ends with a live memory file.** No inbox, no triage, no draft state. If a capture is low quality, the fix is better capture or a later cleanup pass — never a gate.
- Proposed rule two: have **several** paths, each tied to a moment (see `memory-kinds.md`). The old system had one automatic path and it fired only on shell failures.
- Proposed rule three (backed by the standing no-paid-keys constraint): no path may call a paid API. Anything that needs a model rides the Claude subscription (an `agent`/`prompt` hook, or the local `claude` binary).

## Candidate paths for version 01

| Path | Trigger | What it writes | Needs a model? | Notes |
|---|---|---|---|---|
| **End-of-session reflection** | `Stop` hook (agent or prompt type) | 0–5 memories of any kind: decisions, failures, fixes, gotchas, status | Yes, via the hook | The workhorse. One prompt: "What happened this session that the next session would pay to know? Write each as a memory file." Verified hook types in `docs/wiki/claude-code-harness-facts.md` |
| **Owner said so** | The owner types something like "remember this" or states a preference | one `preference` / `convention` / `decision` | No — the main agent writes it via an MCP tool | The only path where the agent writes immediately, mid-session |
| **Failure then fix** | A tool call fails, then a later similar call succeeds | one `failure` + one `fix`, linked | Maybe — the hook can be a plain command that records the pair and lets Stop-time reflection write the prose | The old system's Bash counter, kept but no longer alone |
| **Transcript mining** | A scheduled or on-demand pass over past transcripts | any kinds | Yes, via `claude -p` | Proven to produce good lessons (the 29 old drafts). Runs only for sessions with no Stop-time capture, to avoid duplicates |
| **Coordinator in-run learning** | A coordinator sees executors repeat a mistake | a run-scoped note first; durable `gotcha` / `convention` at run end | Yes, it is the coordinator | See `../learning/` — this is the owner's defining example |

## What each path would need to guarantee (ideas)

- Writes a complete file (frontmatter + body) in one step, so a crash never leaves a half-memory.
- Fills `author` honestly: the person's handle when a human said it, `agent` otherwise.
- Sets `human_verified: false`. Only a human action flips it.
- Does not duplicate: before writing, search existing memories for the same anchor + kind and supersede instead of adding if it is the same lesson.
- Leaves a one-line event in a log so "did capture fire today?" is a `grep`, not a guess. The old system's event log was the single most useful evidence in the post-mortem.

## Dedup and cleanup (later, not a gate)

- Duplicates and junk are cleaned **after** the fact by a pass the owner can run, or by the weekly glance. They are never prevented by making capture ask permission.
- A memory never retrieved in N weeks is a candidate for `deactivated`, surfaced to the owner as a list, not auto-deleted.
