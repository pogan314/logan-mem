---
title: LangMem (langchain-ai/langmem)
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
sources: [docs/wiki/research-extracts/repos-round-1.md, docs/wiki/research-extracts/repos-rounds-2-9.md, docs/wiki/research-extracts/repos-toplevel-docs.md, live `gh api repos/langchain-ai/langmem` and `repos/langchain-ai/langgraph` calls on 2026-08-21]
---

- URL: github.com/langchain-ai/langmem
- License: MIT
- Stars: 1,620 — last push: 2026-08-11 (verified live 2026-08-21)

## What it does

- A library for giving an LLM agent long-term memory: extract facts from a conversation, store them, and retrieve them later.
- Splits memory into three kinds: semantic (facts), episodic (past events), procedural (learned instructions/prompts).
- Lets a developer choose synchronous extraction (on every message) or deferred/background extraction (after activity settles).
- Deletion is opt-in and never automatic — even when enabled, the model only recommends a removal; the calling code decides what happens.
- Depends on LangGraph's `BaseStore` for actual storage — it's an extraction/consolidation layer, not a standalone database.

## How its memory works

- Capture: `create_manage_memory_tool` (synchronous, "hot path") or `create_memory_store_manager` plus `ReflectionExecutor` (background, debounced — a burst of activity produces one consolidated write).
- Store: writes through LangGraph's `BaseStore`; two shapes are available — "Collections" (many small docs, keeps history) or "Profiles" (one schema-based record patched in place).
- Retrieve: reading the code (`MemoryStoreManager._sort_results`) shows results are sorted by raw similarity score only, despite the docs describing importance/recency-weighted ranking.
- Deletion: `enable_deletes` defaults to False; even when enabled, the manager returns a `RemoveDoc` object instead of deleting anything itself.
- Self-editing: `create_prompt_optimizer` can rewrite an agent's own system prompt over time from accumulated feedback, using one of three strategies (`gradient`, `metaprompt`, `prompt_memory`).

## Files that implement it

| path | what it does | verified-exists? |
|---|---|---|
| src/langmem/reflection.py | `ReflectionExecutor` — priority queue keyed by thread ID, cancels/reschedules a pending write on new activity | yes |
| src/langmem/knowledge/extraction.py | `MemoryManager`, `MemoryStoreManager._sort_results` (raw-similarity-only ranking) | yes |
| docs/docs/concepts/conceptual_guide.md | documents the three memory kinds and the (unimplemented) importance/recency ranking claim | yes |
| src/langmem/prompts/optimization.py | `create_prompt_optimizer` — the three self-correction strategies | yes |
| docs/docs/guides/optimize_compound_system.md | `create_multi_prompt_optimizer` — attributes blame across multiple agent roles from one team-level score | yes |
| pyproject.toml | hard-pins `langgraph>=0.6.0` as a runtime dependency | yes |

## LangGraph (checkpoint/durability layer LangMem sits on)

LangGraph (`langchain-ai/langgraph`, 40,185 stars, MIT, last push 2026-08-20, verified live 2026-08-21) is the state-checkpointing engine LangMem's storage depends on. It offers three explicit durability tiers (`"exit"`/`"async"`/`"sync"`, trading speed for write safety) and non-destructive state history — `get_state_history()` plus `update_state()` can fork a new branch without erasing the original. Its "time travel" only forks LangGraph's own internal state object though; it has no concept of the real filesystem or git repo, so rewinding the graph does not revert a file edit or a git commit. A known open issue (`langgraph#7094`) ties a memory leak to the default `durability="async"` setting under backend latency.

## Good

- The one system in the wider survey that is append-only by default for deletion — the model can only recommend, never execute, a removal.
- `ReflectionExecutor` debounces background writes so a burst of activity becomes one consolidated write instead of many.
- Names the "growing collection with history" vs "single always-current record" fork explicitly, as two named configurable memory shapes.
- Three durability tiers borrowed conceptually from LangGraph give a clean dial for how aggressively writes get flushed.

## Bad

- The docs describe ranking by importance and recency; the actual code in `_sort_results` sorts by raw similarity only.
- Hard-pins `langgraph>=0.6.0` — you cannot use LangMem's memory tools without also taking on LangGraph as a dependency.
- The documented default extraction backend is OpenAI embeddings plus Postgres, not zero-key by default.
- An open GitHub issue (`langmem#154`) reports a memory write silently failing when the tool schema doesn't serialize cleanly — the error is logged at DEBUG level, never raised.

## Worth stealing

- The append-only-by-default, recommend-don't-execute pattern for any deletion or rewrite decision a model makes.
- Debounced background consolidation keyed by thread/session ID.
- Naming the collection-vs-profile fork explicitly as a design decision rather than leaving it implicit.

## Old project's verdict

History that binds nothing — described as the closest thing found to the old project's own supersession model; adopted conceptually as the pattern for append-only writes and background consolidation, but rejected as an actual dependency because of the LangGraph coupling.
