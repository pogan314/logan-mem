---
title: claude-mem (thedotmack/claude-mem)
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
sources: [docs/wiki/research-extracts/repos-round-1.md, docs/wiki/research-extracts/repos-toplevel-docs.md, live `gh api repos/thedotmack/claude-mem` call on 2026-08-21]
---

- URL: github.com/thedotmack/claude-mem
- License: Apache-2.0
- Stars: 91,430 — last push: 2026-08-20 (verified live 2026-08-21)

## What it does

- An auto-recall memory system for Claude Code: hooks capture session activity, a background worker process summarizes it, and later sessions get relevant past notes injected automatically.
- Stores notes in a local SQLite database with FTS5 keyword search, plus an optional separate Chroma vector database for semantic search.
- "File Read Gate": when the agent reads a file with prior notes, it appends a timeline of past work on that file.
- Uses a graduated retrieval ladder: check if you already know enough, then fetch by ID, then a structural outline, then a full file read as a last resort.
- Watches actual Claude subscription usage windows and stops background compression before it would blow the plan's quota.

## How its memory works

- Capture: hooks fire on every tool use (under 20ms), and the real summarization runs in a separate background worker so typing never blocks.
- Store: a SQLite table `observations` (id, kind, text, facts, files_read/files_modified, content_hash, etc.), plus optional Chroma vectors; deduped by content hash, so only byte-for-byte identical text is caught.
- Retrieve: session-start injection, the File Read Gate, and search all use the same cheapest-first ladder; falls back from vector to keyword search automatically if the vector database is unreachable.
- No promotion step exists — it's purely a recall system; nothing turns a repeated note into a standing rule or skill.

## Files that implement it

| path | what it does | verified-exists? |
|---|---|---|
| src/cli/handlers/file-context.ts | current File Read Gate handler — allows the read and appends a timeline | yes |
| docs/public/file-read-gate.mdx | docs page — still describes an older deny-and-block behavior that no longer matches the code | yes |
| src/services/worker/search/strategies/HybridSearchStrategy.ts | `intersectWithRanking()` — how vector and keyword results actually get fused | yes |
| src/services/worker/search/SearchOrchestrator.ts | coordinates the two retrievers before fusion | yes |

## Good

- Progressive-disclosure retrieval ladder used consistently across three subsystems: session start, the Read gate, and search.
- Quota-aware brake — tracks the real 5-hour and 7-day rolling subscription usage windows (thresholds 95%/93%/92%) and stops background work before blowing the plan.
- Tree-sitter structural map across 24 languages, giving a cheaper "outline" tier before a full file read.
- Background summarization runs in a fully separate worker process so it never blocks the main session.

## Bad

- The docs (`file-read-gate.mdx`) describe the File Read Gate as blocking the read; the actual code in `file-context.ts` allows the read and just appends a timeline — the advertised token savings are overstated compared to what the shipped code does.
- A historical incident, now fixed in v13.3.0: the background worker silently billed a user's Claude subscription at retail per-token rates when no API key was configured — one user reported roughly $2,089 drained in a month before finding the cause.
- Multiple open resource-leak issues: a worker that doesn't reap headless subprocesses (203 accumulated over 4 days in one report, about 45GB RAM), and orphaned Chroma vector-database processes (hundreds reported, about 2GB leaked).
- Dedup only catches byte-for-byte identical text — near-duplicate notes still pile up.
- No promotion or self-improvement mechanism of any kind.

## Worth stealing

- The cheapest-first retrieval ladder (know already, then fetch by ID, then outline, then full read) as a general retrieval discipline.
- A quota-aware brake that watches real subscription usage windows before running background work.
- Verifying a "docs describe X, code does Y" claim by reading the handler directly instead of trusting the docs page — this repo is itself the example of why that check matters.

## Old project's verdict

History that binds nothing — the retrieval ladder and quota-brake pattern were adopted conceptually; running the tool itself was rejected, mainly over the separate background-worker-plus-Chroma-process architecture blamed for the leak and billing incidents.
