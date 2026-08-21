---
title: feat/episodic — searchable conversation history
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md #10, docs/wiki/obra-episodic-memory.md, open-questions.md #2]
---

# feat/episodic

- **Episodic memory** here means: every past Claude Code / Codex session, indexed so an agent can ask "what did we discuss about X?" and get the actual exchange back.
- It is a different layer from curated memories. Curated memories are short lessons an agent should act on. Episodic history is the raw record an agent can search when a lesson is not enough ("we fixed this once — how exactly?").
- The reference implementation is `github.com/obra/episodic-memory` — profiled live in `docs/wiki/obra-episodic-memory.md`. The owner is interested in the concept; packaging is open.

## The two layers, side by side

| | Curated memory (feat/memory) | Episodic history (this) |
|---|---|---|
| Unit | A short lesson file | A whole conversation, chunked |
| Written by | Capture paths, at specific moments | Indexing the transcripts that already exist on disk |
| Human review | Optional flag | None, ever |
| Injected at session start? | Yes, the relevant few | No — searched on demand |
| Answers | "What should I do here?" | "What did we do last time, exactly?" |

## Options for version 01

| Option | What it means | Cost | Risk |
|---|---|---|---|
| **A. Adopt `obra/episodic-memory` as-is** | Install its plugin beside ours. Use its MCP search tools | Near zero to build | It has had 0 commits in 90 days and 58 open issues (2026-08-21); two recent issue titles mention sync never converging and summarizer subprocesses launching the user's MCP servers. "Adopt" means use and watch, not depend on |
| **B. Build a thin version inside logan-mem** | Index transcripts under `~/.claude/projects/` into our own SQLite; one search tool | Medium — the parser and embedder are the real work | Scope creep; it is a second system inside version 01 |
| **C. Skip in 01** | Ship curated memory only; add episodic in 02 | Zero | The owner wants it; deferring is a judgment call |

- Leaning: **A for 01**, because it costs nothing and shows whether the owner actually uses conversation search day to day. If yes, B in 02, reusing our own store. If no, C forever.

## What we would steal if building our own (from the wiki profile)

- Index the transcript JSONL that Claude Code already writes; do not capture anything new.
- Local embeddings only (`Xenova/bge-small-en-v1.5` via Transformers.js), no API key — the same model the old build used, so the cost is known.
- Chunk by exchange (one user turn + one assistant turn), not by session.
- A `SessionStart` hook that only kicks off a background index sync, never blocks.
- Search returns short previews first, full exchange on request (progressive disclosure).
