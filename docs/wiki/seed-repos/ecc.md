---
title: ECC (affaan-m/ECC)
type: wiki
status: research-fact
created: "2026-08-21 13:15 CDT"
updated: "2026-08-21 13:15 CDT"
sources: [docs/wiki/research-extracts/repos-round-1.md, docs/wiki/research-extracts/repos-toplevel-docs.md, live `gh api repos/affaan-m/ECC` call on 2026-08-21]
---

- URL: github.com/affaan-m/ECC
- License: MIT
- Stars: 241,696 — last push: 2026-08-21 (verified live 2026-08-21)

## What it does

- A large bundle of Claude Code skills (283+) built around one core idea: capture what happens during a coding session and turn it into reusable "instincts" — short notes with a confidence score.
- Ships hooks that fire on every tool call (`PreToolUse`/`PostToolUse`) so capture is deterministic rather than depending on the model choosing to invoke a skill.
- An optional background process (a cheaper Claude model called through the local CLI) reads captured events and writes instinct files.
- At session start it injects the highest-confidence instincts into context, capped by character count.
- Has a promotion path (`/evolve`) that can turn a well-supported instinct into a permanent skill.

## How its memory works

- Capture: hooks on every tool call write to `observations.jsonl`, after running text through a secret-scrubbing regex filter first.
- Store: a background Haiku process reads observations and writes instinct files (markdown + YAML frontmatter) carrying a confidence score.
- Retrieve: at session start, up to 6 highest-confidence instincts inject into context, hard-capped around 8,000 characters, with a truncation marker if cut.
- Promote: `/evolve` can turn an instinct into a skill; the promotion rule is hard-coded (must appear in 2+ projects, average confidence ≥0.8).

## Files that implement it

| path | what it does | verified-exists? |
|---|---|---|
| skills/continuous-learning-v2/scripts/instinct-cli.py | CLI managing instinct records — reads confidence, never recalculates it | yes |
| skills/continuous-learning-v2/SKILL.md | describes a confidence increase/decay scheme the code doesn't implement | yes |
| skills/continuous-learning-v2/hooks/observe.sh | deterministic PreToolUse/PostToolUse capture hook | yes |
| skills/continuous-learning-v2/agents/observer.md | background observer agent definition | yes |
| skills/continuous-learning-v2/agents/observer-loop.sh | background loop, shells out to local `claude --model haiku --print` | yes (old research cited it as a bare top-level `observer-loop.sh`; live search found it under this path instead) |
| scripts/hooks/session-start.js | session-start injection — confidence threshold and char-cap constants live here | yes |
| scripts/hooks/pre-compact.js | worktree-aware pre-compaction summary hook | yes |
| scripts/auto-update.js | manual self-update (`git fetch` + `git pull --ff-only`), no signature/checksum check | yes |
| observations.jsonl | raw capture log | runtime, not a repo file |
| ${XDG_DATA_HOME}/ecc-homunculus | instinct .md/.yaml storage location | runtime, not a repo file |

## Good

- Deterministic hook-based capture (fires every time) instead of a skill the model may or may not invoke.
- Secret-scrubbing regex runs before anything touches disk.
- Background distillation shells out to the local `claude` CLI binary instead of a paid API key, riding the existing subscription.
- "GateGuard" forces the agent to investigate (grep importers, check schema) before its first edit to a file in a session — a real-time substitute for a maintained dependency graph.
- Worktree-aware pre-compaction summaries keep multiple checkouts of the same repo from cross-contaminating.

## Bad

- `SKILL.md` documents a confidence-update scheme (+0.05 on confirm, -0.1 on contradict, -0.02/week decay); reading `instinct-cli.py` end to end shows the CLI never recalculates confidence — it only reads whatever value is already in the file, so scoring only happens if the background process runs and follows its natural-language instructions.
- The background observer is off by default and has several open, unresolved issues on Windows/macOS (a stat-field bug breaking every write/read, a hook wrapper echoing stdin back to stdout).
- `/evolve --generate` is plain string concatenation of existing notes, not an AI synthesis step, despite reading like one.
- Installing ECC installs all 283 bundled skills by default — no way to install just the memory piece.
- `scripts/auto-update.js` has no signature, checksum, or GPG verification of pulled content.

## Worth stealing

- Deterministic hook capture over skill-based capture, for anything that must fire every time.
- Riding the local CLI subscription (`claude --model haiku --print`) instead of a metered API key for background processing.
- GateGuard's "force a grep before the first edit" pattern as a cheap stand-in for a maintained dependency graph.
- Worktree-aware summaries keyed to which checkout is currently active.

## Old project's verdict

History that binds nothing — GateGuard's policy and the ride-the-subscription trick were the two things explicitly adopted; the confidence-scoring machinery and auto-promotion were explicitly rejected as unenforced and ungated.
