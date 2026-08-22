---
title: codebase-memory-mcp (DeusData/codebase-memory-mcp)
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-22 11:53 CDT"
sources: [docs/wiki/research-extracts/repos-round-1.md, docs/wiki/research-extracts/repos-toplevel-docs.md, live `gh api repos/DeusData/codebase-memory-mcp` calls on 2026-08-21 and 2026-08-22]
---

**This is no longer just a candidate we're evaluating.** As of logan-mem version 01, this repo is vendored wholesale into `spine/` via `git subtree` (upstream tag `v0.10.8`) and renamed `logan-spine-mcp`. This file stays a research-fact page about the **upstream** project — what it does on its own, independent of our fork — because that's still a fact about a repo we don't control. For what we changed and why, see `spine/LOGAN-CHANGES.md` in this repo (not wiki content — it's a running changelog of our modifications, owned outside `docs/wiki/`).

- URL: github.com/DeusData/codebase-memory-mcp
- License: MIT
- Stars: 39,863 — last push: 2026-08-22 (verified live 2026-08-22)

## What it does

- An MCP server that builds a tree-sitter/LSP-based knowledge graph of a codebase (functions, classes, calls) so an agent can query structure instead of reading files one at a time.
- Runs fully local, no API key required, using a local embedding model for its semantic search mode.
- Ships a hook that injects graph hits into Grep/Glob calls automatically.
- Supports 35 languages at varying accuracy — 17 score 90%+, 2 score under 75% (OCaml 72%, Haskell 62%).
- Can trace a git diff down to the exact functions/classes changed, and find dead code via a graph query.

## How its memory works

- Capture: parses the repo with tree-sitter/LSP wrappers into nodes (functions/classes) and edges (calls/imports).
- Store: writes a zstd-compressed SQLite snapshot to `.codebase-memory/graph.db.zst`, 8-13x smaller than raw.
- Retrieve: `search_graph` (BM25, camelCase-aware) or `semantic_query` (local `nomic-embed-code` embeddings); `detect_changes` walks a git diff to exact symbols changed and tags hits by hop distance.
- Freshness: no live file watcher — the graph goes stale between explicit re-index runs.

## Files that implement it

| path | what it does | verified-exists? |
|---|---|---|
| src/cli/hook_augment.c | PreToolUse hook that augments Grep/Glob with graph symbols | yes |
| src/mcp/mcp.c | MCP server core | yes |
| docs/BENCHMARK.md | source of the 35-language accuracy table | yes |
| README.md | project overview and benchmark claims | yes |
| .codebase-memory/graph.db.zst | compressed graph snapshot | runtime, not a repo file |

## Good

- Zero-runtime static binary, no API key, genuinely local-first.
- The only one of the code-tools compared in the old research that has a real graph, rather than a live LSP wrapper or plain grep.
- `detect_changes` resolves a git diff straight to exact functions/classes changed, with hop-distance tagging.
- Fast — benchmarked indexing the Linux kernel (28M LOC) in 3 minutes on a laptop.

## Bad

- Still pre-1.0 with a sizable open-issue count — expect churn.
- Its own academic paper (arXiv 2603.27277, written by the same person who built the tool) reports the tool's answer quality is worse than a plain file-exploring agent (83% vs 92%) even though it's roughly 10x cheaper in tokens — a tradeoff its own README doesn't mention.
- The index drifts from the working tree between re-indexes — "the graph said X" isn't always today's truth.
- The installer downloads a prebuilt native binary and writes global agent config across many tools — a supply-chain surface, not scoped to one project.
- An internal hook self-aborts silently past a 2000ms deadline (`CBM_HOOK_DEADLINE_MS`), returning empty with no error.

## Worth stealing

- Committing a compressed, git-shareable graph snapshot with an auto-added `.gitattributes` `merge=ours` line to avoid binary-diff conflicts (though the old project's own research explicitly decided against shipping a binary index for its own case, favoring rebuild-on-demand).
- Tagging changed-code impact by hop distance from a git diff.
- A local embedding model for semantic search with zero API key.

## Old project's verdict

History that binds nothing — cited constantly as "the reference implementation" for a code-structure spine, but the final design built the thinnest possible spine with tree-sitter directly rather than depending on this tool, and its benchmark credibility was flagged as compromised once checked against its own paper.
