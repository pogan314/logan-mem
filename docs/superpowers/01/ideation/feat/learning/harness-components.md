---
title: Harness components — turning memories into things that execute
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md #4 #6, feat/memory/verified-flag.md, docs/wiki/claude-code-harness-facts.md]
---

# Harness components

- A memory is text an agent reads. A harness component is something the harness **runs**: a skill, a hook, a rule file (CLAUDE.md section), an agent definition, a deny rule.
- The owner's original idea: review accumulated memories and turn some into org-wide or repo-specific harness components. This is the one place a human gate is right, because the output executes.
- This is a **separate subsystem** from memory. It reads memories; it does not change how memories work. Probably version 02+. Noted here so it is not confused with capture again.

## The pipeline (idea)

| Step | Who | What happens |
|---|---|---|
| 1. Candidate surfaces | automatic | A memory that keeps being retrieved, or a `convention` that keeps being re-learned, gets flagged "could be a component" |
| 2. Draft | agent | Writes the component: a SKILL.md, a hook script, a CLAUDE.md paragraph, an agent .md — into a staging folder, not installed |
| 3. Review | human | Reads the draft. Approves, edits, or rejects. This is the gate |
| 4. Install | human-triggered | The component goes into the right place: the repo's `.claude/` for repo-scoped, the plugin for org-wide |
| 5. Trace | automatic | The component records which memories it came from, so if a memory is later superseded, the component is flagged for re-review |

## Scope levels

- **Repo-specific**: lives in that repo (`.claude/skills/`, `.claude/agents/`, a CLAUDE.md section). Shared by cloning. The owner's "harness components for a given repo".
- **Org-wide**: lives in the team plugin, installed everywhere. Rarer, higher bar.
- **Personal**: lives in the user's own config. Lowest bar.

## Examples of the jump from memory to component

| Memory (text) | Component (executes) |
|---|---|
| `gotcha`: "`gh pr create` has no `--json`; parse stdout" — retrieved 9 times | A tiny skill `open-pr` that does it right, so nobody re-reads the gotcha |
| `convention`: "run the type-check before reporting done" — re-learned in 3 runs | A `Stop` hook that runs the type-check and blocks "done" if it fails |
| `preference`: "bullets not paragraphs in docs" — verified | A CLAUDE.md section in every repo, or a `PostToolUse` check on `.md` writes |
| `failure`: "agent set its own git identity" | A `PreToolUse` deny rule on `git -c user.email` |

## Proposed rules to keep it from becoming the old mess

- No component installs itself. Ever. Step 4 is a human typing a command.
- No component without a traced source memory. If it cannot say which lesson it encodes, it is not a learned component; it is just code.
- The count of components stays small. If the team has 40, the memories were not worth encoding, or the encoding is too eager.
