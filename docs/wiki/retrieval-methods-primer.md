---
title: Retrieval methods primer
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
sources: [general knowledge of established IR/RAG techniques, docs/superpowers/01/ideation/findings/raw/built-inventory.md]
---

# Retrieval methods primer

How an AI memory system finds "the right memory" out of everything it has stored. Each method below answers a different question about what "right" means.

## Keyword search (BM25 / SQLite FTS5)

- **What it is**: "Full-text search" means matching the actual words in your query against the actual words stored in each record — like Ctrl+F, but ranked. BM25 is the ranking formula: a document scores higher if your query's words appear often in it, but rarer words (that don't show up everywhere) count for more than common ones. FTS5 is SQLite's built-in full-text search engine — it stores a token index of every word in your records so a search doesn't have to scan the whole table.
- **Good at**: exact terms — a function name, an error code, a file path, a specific phrase. Fast, no extra model needed, deterministic (same query always gives the same order).
- **Bad at**: synonyms and paraphrase. Search "how do I undo a commit" and it will not find a record that only says "reverting changes" unless that word also appears.
- **Costs**: a database with a text index (FTS5 ships inside SQLite, so no separate service). No model to run. Cheap and instant.
- **Old system used**: FTS5 always on, `unicode61 remove_diacritics 2` tokenizer, BM25 ranking, capped at 20 candidates per partition.

## Vector / embedding search

- **What it is**: an embedding is a list of numbers (a vector) that a small model produces from a piece of text, positioned so that texts with similar *meaning* end up close together in that number-space — even if they don't share any words. "Semantic" search means searching by meaning instead of by exact word match. Producing an embedding requires running a model — either a small one locally on your machine, or a call out to an API. Search then means: embed the query, and find which stored embeddings are numerically closest to it (commonly by cosine similarity, a measure of how aligned two vectors are).
- **Good at**: paraphrase and synonyms — "undo a commit" can match "revert changes" because they mean similar things, even with zero shared words.
- **Bad at**: exact strings — it can miss an exact function name or error code that a keyword search would nail instantly, because embeddings blur precise wording into "roughly similar meaning." Also needs a model to be present and working; if that model is missing, vector search silently produces nothing.
- **Costs**: a model (small local one, e.g. tens of megabytes, or a paid API call per query and per stored item), and a vector-capable database or index to store and search the numbers. Slower and heavier than keyword search.
- **Old system used**: gated behind a `models: 'present'` config flag that a fresh install always set to `'absent'` — so vector search was built (`Xenova/bge-small-en-v1.5`, 384-dimension vectors, cosine similarity, stored in a `vec0` SQLite virtual table) but never actually turned on by any shipped command. It had to be enabled by hand.

## Hybrid search and RRF (reciprocal rank fusion)

- **What it is**: running keyword search and vector search separately, each producing its own ranked list, then merging the two lists into one. RRF is one way to merge them: each item gets a score based on *where it ranked* in each list (rank 1 scores highest, rank 2 less, and so on), and an item's scores from both lists get added together. An item that ranks well in both lists wins; an item that only one method found still gets counted, just with a smaller contribution.
- **Contrast with intersection**: intersection means keeping only items that showed up in *both* lists — which throws away anything only one method found. RRF keeps everything found by either method and ranks by combined evidence, so a match that only keyword search caught (an exact term) or only vector search caught (a paraphrase) can still surface, just lower.
- **Good at**: covering both exact-term and paraphrase queries without picking one method over the other in advance.
- **Bad at**: still inherits both methods' costs — you need both a text index and a working embedding model for hybrid to actually be "hybrid." If the vector side is off, "hybrid" quietly degrades to keyword-only.
- **Costs**: everything both underlying methods cost, plus the (cheap) fusion step itself.
- **Old system used**: `rrfFuse`, constant k=60, fusing the FTS5 list and the vector list (when the embedder was loaded) — then multiplying each fused score by a fixed weight depending which layer (project, shared, user, etc.) the record lived in.

## Graph expansion

- **What it is**: memories can be linked to each other (e.g. "this decision supersedes that one," "this fact relates to that one"). Graph expansion means: after finding some directly-matching memories, also pull in memories connected to them by one or two hops — walking the links like following "related pages" on a wiki.
- **Good at**: surfacing context the query itself didn't mention — a decision's justification, a linked follow-up, a record that only makes sense next to the one that was found.
- **Bad at**: drifting off-topic if links are noisy or too generous; a memory two hops away can be only loosely related to what was actually asked. Needs the links between memories to have been recorded in the first place — it can't invent relationships that were never captured.
- **Costs**: a graph structure (a table of links between records) rather than a new model. Compute cost is small — walking a few hops from a handful of seed records — but needs care so expanded results don't drown out or outrank the direct matches.
- **Old system used**: always on, no flag to disable it. Expanded from the top 10 fused results, up to 2 hops, with each hop's contribution decayed (weighted down) by half, walking both explicit links between records and one-hop category-graph neighbourhoods. Capped so a graph-walked record could never outrank a direct hit.

## Rerankers

- **What it is**: a second, usually slower and more accurate model that takes the top handful of results from a first-pass search (keyword, vector, or hybrid) and re-sorts just those, looking more carefully at how well each one actually answers the query. Rerankers commonly use a "cross-encoder," a model that reads the query and one candidate document together (rather than comparing separately-computed vectors), which is more accurate but too slow to run over an entire memory store.
- **Good at**: improving precision at the very top of the results — catching cases where the first-pass ranking put a mediocre match above a better one.
- **Bad at**: running over everything — it's too slow for that, so it only ever touches a small shortlist the first-pass search already narrowed down. If the first pass missed the right memory entirely, reranking can't recover it.
- **Costs**: another model (typically larger/slower than an embedding model) and the time budget to run it — real added latency, which is why it's usually applied only to a small top-N.
- **Old system used**: built (`bge-reranker-base`) but shipped off by default and never had an automatic download path, so it never actually ran in the shipped product — confirmed off in the one live store checked.

## Progressive disclosure

- **What it is**: instead of returning full memory contents for every search result, return short summaries (or just titles/ids/scores) first, and only fetch the full text for the specific item the agent actually decides it needs. This keeps the search response small.
- **Good at**: saving context tokens — an AI agent's usable "memory" for a conversation (its context window) is limited, and dumping ten full records into it to show three-word titles' worth of relevance is wasteful.
- **Bad at**: adding a round trip — the agent has to make a second call to get the full content of anything it wants to actually read, which costs a bit of latency and an extra tool call.
- **Costs**: no new model or database; it's an API design choice about how much a search call returns.
- **Old system used**: `mem_search` returned only ids, titles, and scores; a separate `mem_get(id)` call fetched the full record body. A third tool, `failed_before`, did an even cheaper title-only check for a prior recorded failure.

## Recency / importance / confidence scoring

- **What it is**: adjusting search ranking using signals other than text/meaning match — how recently a memory was created or used (recency), how significant it was marked as (importance), or how sure the system (or the model that wrote it) is that it's correct (confidence).
- **Good at**: surfacing what's likely still relevant (recent) or worth prioritizing (important) over stale or minor entries, when the raw text match alone can't tell the difference.
- **Bad at**: self-scored confidence — a known failure mode is a model rating its own memory's confidence when it writes it, and that score then never gets revisited or decayed over time even as the memory goes stale or gets contradicted. A number that was accurate on day one but never updates becomes actively misleading later, while still looking authoritative because it's a number.
- **Costs**: extra fields to store and maintain per memory (a timestamp is nearly free; importance/confidence usually need either a human judgment or another model call to assign, plus a policy for whether/how they change over time).

## Session-start injection vs. query-time retrieval

- **What it is**: two different moments a memory system can hand information to an agent. Session-start injection pushes a fixed block of context automatically at the beginning of a conversation, before the agent asks for anything. Query-time retrieval means the agent actively asks a search tool for specific information when it decides it needs it, mid-conversation.
- **Good at (injection)**: guarantees the agent sees baseline context even if it never thinks to ask — useful for things every session needs, like "here are your standing instructions."
- **Bad at (injection)**: uses up context tokens whether or not that content turns out relevant to what actually happens in the session, and it's a fixed snapshot from session start — it can't react to what the conversation turns out to be about.
- **Good at (query-time)**: only spends tokens on what's actually relevant to the specific question at hand, and can be re-run as the conversation's needs change.
- **Bad at (query-time)**: only works if the agent knows to ask — if it doesn't recognize that a past memory is relevant, it never queries for it, so nothing gets surfaced.
- **Costs**: injection costs a fixed token budget every session regardless of use; query-time costs tokens only per call but depends entirely on the agent choosing to call it.
- **Old system used**: both, at different layers. A blocking `SessionStart` hook built and injected a context block at the start of every session. A separate `UserPromptSubmit` hook re-ran on every prompt, doing an FTS-only (keyword-only, no vector arm) check and injecting a few matching records automatically. On top of both, the agent could also call `mem_search` itself at query time through an MCP tool.

## Summary

| Method | What it matches on | Good at | Bad at | Needs |
|---|---|---|---|---|
| Keyword (BM25/FTS5) | Exact words | Exact terms, names, codes | Synonyms, paraphrase | Just a text-indexed database |
| Vector/embedding | Meaning | Paraphrase, synonyms | Exact strings; needs a working model | An embedding model + vector-capable store |
| Hybrid + RRF | Both, merged by rank | Covering both exact and paraphrase queries | Still needs both underlying methods working | Both of the above, plus a fusion step |
| Graph expansion | Links between memories | Pulling in related context the query didn't name | Drifting off-topic; needs links to already exist | A link table, no new model |
| Reranker | Careful re-scoring of a shortlist | Fixing ranking mistakes at the very top | Can't fix a first pass that missed entirely; slow | A second (often heavier) model |
| Progressive disclosure | N/A — a response-shaping choice | Saving context tokens | An extra round trip to get full content | No new model or store |
| Recency/importance/confidence | Non-text signals | Prioritizing fresh or significant memories | Self-scored confidence that never decays | Extra fields, a maintenance policy |
| Session-start injection | Nothing — it's automatic | Guaranteed baseline context | Wastes tokens on irrelevant sessions, frozen at start | A fixed context budget |
| Query-time retrieval | Whatever the agent asks | Only spending tokens on what's relevant | Silent if the agent never thinks to ask | Agent judgment to call it |
