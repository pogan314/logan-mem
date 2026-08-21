---
title: The verified flag — human review as a signal, never a gate
type: ideation
status: stale
created: "2026-08-21 13:06 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [owner-requirements.md #2 #3 #4]
---

# The verified flag

- The owner's words: "The correct 'approval' thing was just a YAML key/value that clarifies for the agent whether or not a memory was reviewed by a human."
- So: one field, `human_verified: true | false`. Default `false`. Nothing else changes about how the memory is stored, injected, or searched.

## What the flag does

- When two memories conflict and one is verified, the agent leans toward the verified one. That is the whole behaviour.
- Ranking may give verified memories a small boost. Small — an unverified gotcha from yesterday is usually worth more than a verified preference from March.
- Nothing filters on it. An agent never sees fewer memories because a human has not looked.

## How it gets set

| Way | When | Idea |
|---|---|---|
| Owner says "yes, keep that" in chat | During a session | The agent calls a one-line MCP tool that flips the flag and records who and when |
| Owner edits the file | Any time | It is YAML; a text editor works |
| Weekly glance | Once a week, five minutes | A tiny CLI lists the week's new memories; the owner marks keep / drop; keeps get `human_verified: true`, drops get `status: deactivated` |

## What this replaces from the old system

- The `inbox/` draft state and `pogan inbox` triage — gone.
- `provenance: human-decided | agent-written` — folded into `author` + `human_verified`.
- The two-person pull request to ratify a team rule — gone; a team rule is a verified memory with `scope: team`.
- "Promote a memory into a skill" is **not** this flag. That is a separate system (`../learning/harness-components.md`) where a human gate genuinely belongs, because the output executes.
