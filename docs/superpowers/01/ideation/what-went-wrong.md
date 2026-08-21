---
title: What went wrong — things to avoid from the old build
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [findings/drift-analysis.md, findings/what-was-built.md, findings/live-store-evidence.md, the 2026-08-21 session's second-opinion analysis]
---

# Things to avoid

- Every item here is a trap the old build (`pogan-mem`, repo `../pogan-toolkit`) actually fell into. Evidence for each is in `findings/`.
- Read this before brainstorming, and again before writing any plan. The failure was not bad code. It was a good build of the wrong thing.

## The five fatal mistakes

| # | What happened | Why it killed the system | Proposed rule for logan-mem |
|---|---|---|---|
| 1 | **A human-approval queue in front of memory.** Mined lessons, quick captures, and failure nudges all landed in an `inbox/` that needed `pogan inbox` triage before a memory existed. | 29 good lessons sat unreviewed; 0 became memories. The owner got busy; the system starved. | Human review is a flag on a memory, never a gate. A memory is live the instant it is written |
| 2 | **Only one automatic capture path, and it was a Bash-failure counter.** Every other capture required the model to decide to call a tool. | The only memories that got written were "build broke". The owner saw that and turned it off. | Capture would need several automatic paths covering many kinds of lesson, built before anything else |
| 3 | **Everything shipped in one release.** Vectors, graph, spine, miner, encryption, multi-machine sync, key rotation, member offboarding, doctor with ~18 checks. | 33,945 lines of source, 2,856 of them health checks, for a store holding 6 records. Nobody could understand it, including the owner. | Build in versions. Version 01 gets a hard size budget. A feature that does not serve capture-to-recall waits |
| 4 | **Success was defined by a seven-clause gate that was never run.** The spec's acceptance test needed five days of capture, a 10-record sample, and a 45-minute-per-week triage budget. | There were never 10 records to sample. The gate measured a workflow the owner had already rejected ("heavy manual input defeats the purpose"). | Proposed: success = the owner looks at the memories and says they are useful, checked weekly by hand in five minutes |
| 5 | **Install scattered across six surfaces.** Hooks in `/home/ubuntu/.claude/settings.json`, an MCP entry in `/home/ubuntu/.claude.json`, a symlink at `/home/ubuntu/.claude/skills/pogan`, deny rules, git hooks via `core.hooksPath` on three repos. `uninstall` could not remove its own MCP entry. | Teardown took two attempts and a hand-kill of a running process. Re-enable is two commands, undocumented. It silently reinstalled itself the same day it was removed. | One plugin. `/plugin install` and `/plugin uninstall` are the whole story. Nothing else touches the machine |

## The process traps (how agents drifted)

- **Research without a budget.** The owner handed over 3 repos. Agents surveyed ~60 systems across 9 review rounds and 83 report files. Each round added features to "stay elite". Proposed rule: research answers a question; it does not generate requirements.
- **Specs that cite themselves into the code.** 87 of 97 source files carry spec-section citations in comments — 1,974 of them. Agents treat a citation as law. The owner's own audit found 8 of 33 citations in one file were wrong. Proposed rule: code comments say what the code does, never which spec paragraph ordered it.
- **"Execute this plan" instructions that outlive the plan.** 9 of 12 plan files still say it. Proposed rule: a plan is a document with a status field, and a stale plan is deleted or archived, not banner-patched.
- **Adjectives as requirements.** "Elite", "full version", "no dumbed-down v1" each became a reason to add a subsystem. Proposed rule: requirements are behaviours the owner can watch happen. "Elite" is not one.
- **Claiming done without running it.** Checkbox tallies were closed on commit messages and plan prose. Reconciliation later found the live-verification layer "largely unexecuted". Proposed rule: a box closes on code, tests, a diff, or live state — never on a message.
- **Docs describing the design instead of the code.** README, HOW-IT-WORKS, and spec all needed a 2026-08-21 sweep to match reality. Proposed rule: if a doc describes behaviour, it links the code line, and the line is checked when the doc is touched.
- **One session open for weeks.** Drift compounds inside one context. Proposed rule: short sessions with a clear deliverable each; the repo, not the conversation, holds state.

## Design traps (ideas that sounded right and were wrong)

- **Three stores with different rules** (personal / org / shared, plus per-person drawers enforced by git hooks). The owner could not tell two of them apart. Attribution belongs in a field, not a folder tree.
- **A two-person pull-request gate on team rules.** A solo user cannot pass it. It gated exactly one record in the system's life.
- **Encryption for client memories** stored outside the client's repo, read only by internal agents. Built, key folder empty, never used, and it makes search results incomplete whenever a key is missing.
- **A code map as a throwaway cache.** Rebuilt from scratch each time, no place for a human note. The owner wanted the opposite: committed, annotated, in the repo.
- **Four locked memory kinds** (`decision`, `fact`, `failed`, `recipe`) in a schema enum. Adding a fifth meant a migration. The kinds list must be cheap to grow.
- **Hooks limited to plain commands.** Claude Code offers `prompt` and `agent` hook types that can run a model at Stop to write lessons. The old system never used them, so "automatic learning" had no cheap home.
- **Promotion confused with learning.** Promotion (copy a memory to a wider store) got built; learning (a coordinator changing strategy mid-run) never did. They are different systems and the owner only asked for the second.
- **Provenance theatre.** Every record carried `born_in.agent/model/effort` and `provenance: human-decided`; the live records all say `agent: cli, model: none, effort: unknown`. A field nobody fills is noise.

## What was actually fine (so we do not over-correct)

- Plain markdown files with YAML frontmatter as the storage format. Human-readable, git-friendly, zero infrastructure.
- SQLite FTS5 keyword search as the always-on baseline. It worked, it was fast, and it needs no model.
- "Never hard-delete; deactivate or supersede." Cheap, and it makes mistakes reversible.
- The transcript miner's output quality. The 29 inbox drafts are genuinely good lessons. Mining works; only the gate in front of it failed.
- Riding the Claude subscription via the local `claude` binary instead of a paid key. The one mechanism that made "no paid keys" real.
