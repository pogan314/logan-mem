---
title: Research extract — repos studied in rounds 2–9 and spec reviews
type: wiki
status: research-fact
created: 2026-08-21
updated: 2026-08-21
sources: [the old repo's research reports under ../pogan-toolkit/docs/superpowers/brainstorming/ (reports/r1 … r9), read 2026-08-21]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions. The per-repo mechanisms, weaknesses, and file paths are external facts (file paths re-verified live in `../seed-repos/` for the seed repos only); every "adoption status" / "verdict" line is the OLD project's history and binds nothing here.

# External repos/products studied in docs/superpowers/brainstorming/reports/r2-r9, 00-review-history.md, 02-recommendations-second-opinion.md, discussion.Q+A.md, spec/reviews, spec/reports

Note on scope: several r9 files (27-30) and r8/25 contained zero mentions of any external memory-repo names on grep — they are internal consistency/sync/org-layer audits, not repo surveys. r5/17-board-plugin-salvage.md is a salvage review of pogan-toolkit's OWN prior internal plugin ("board-plugin"), not an external repo, so it is excluded below. r6/18-claude-code-primitives-check.md verifies Claude Code's own (Anthropic) hook/memory primitives, not a third-party repo, so excluded. GitHub-rulesets spike reports (r4/r6) are about a GitHub product feature unrelated to memory systems, and are not included as "memory system" entries.

---

## Mem0 (mem0ai/mem0)

1. **Good (praised/borrowable):** Its "core loop" pattern — hand a new fact plus nearest existing memories to a model, model decides ADD/UPDATE/DELETE/NOOP via structured tool call — is cited as the reference example of "let the model decide what's worth remembering," later corrected to be the WRONG example (see Bad). Mem0's own hosted Graph Memory is credited with "connecting facts across memories" contributing to multi-hop/temporal benchmark gains (docs.mem0.ai `docs/platform/features/graph-memory.mdx`, cited in r2/C-claim-audit.md:49). [r2/C-claim-audit.md:47-49]
2. **Bad/weak/rejected:** Mem0's ADD/UPDATE/DELETE model runs "with no confirmation, no diff, no record of what was destroyed," and "Mem0's own paper contains no error analysis of how often that decision is wrong" [02-recommendations-second-opinion.md:33; confirmed r6/20:53-55 against arXiv:2504.19413]. Mem0's own published ablation of its graph variant: 68.44% accuracy vs 66.88% for vector-only (marginal), while running "3x slower and using 2x the tokens," and it "lost on both single-hop and multi-hop sub-scores" [r2/B-2026-evidence.md:21; 02-recommendations-second-opinion.md:99]. Mem0's OpenMemory component shipped with no authentication on any endpoint (CVE-2026-59705, CVSS 9.3 — anyone could read/delete anyone's memories by supplying a different user_id, plus a DoS via a `pause` endpoint) and plaintext API-key storage plus an SSRF hole via `ollama_base_url` (CVE-2026-59706, CVSS 9.2) [r6/19:133-139; 02-recommendations-second-opinion.md:166]. "Still unresolved" status is UNVERIFIABLE — no GitHub Security Advisory found either way [r6/19:139]. A deduplication bug ("~11 real bits of collision detection, 11.7% of new entries on 4,841 memories silently discarded") is referenced in 02 but is UNVERIFIABLE — round-6 fact-check searched mem0ai/mem0 issues extensively and found only unrelated dedup bugs (#6515/#6531 TOCTOU hash race, a cosine-threshold false-positive bug), not this one [r6/19:121-125].
3. **File/module/schema paths cited:** `docs/migration/oss-v2-to-v3.mdx` (Mem0 v3 migration guide, confirms Neo4j/Memgraph/Kuzu/Apache AGE/Neptune graph-store providers were removed — "roughly 4,000 lines of integration code") [r2/C-claim-audit.md:43-45]; `docs/platform/features/graph-memory.mdx` (current, live: "Graph Memory is built in... always on," entity linking via co-occurrence not typed edges) [r2/C-claim-audit.md:47-53]; `docs/platform/platform-vs-oss.mdx` (flagged as stale/self-contradictory — still lists OSS as having "✅ External graph store") [r2/C-claim-audit.md:55]. Paper: arXiv:2504.19413.
4. **Verdict:** REJECTED as a pattern to copy for the core write-decision loop (LangMem is preferred instead — see below). Mem0's removal of its own graph backend from the OSS SDK, and its own ablation numbers, are used as evidence AGAINST building a model-extracted graph. **Reversed claim, tracked across rounds:** earlier drafts stated "Mem0 built a graph mode, ran it two years, found only marginal improvement, and deleted it" — this was found FALSE in round 2 (r2/C-claim-audit.md) and again flagged as needing correction through rounds 4, 6, 7 (r4/14:71-85; 00-review-history.md:11,45,56; discussion.Q+A.md:143-151) — Mem0's Graph Memory is live on every paid tier, never deleted; only the external-graph-database *plumbing* (Neo4j etc.) was dropped from the free/OSS SDK.

---

## LangMem (langchain-ai/langmem)

1. **Good:** Debounces background memory writes so a burst of activity produces one consolidated write via `ReflectionExecutor` — a priority queue keyed by thread ID that cancels/reschedules a pending write when new activity arrives (`src/langmem/reflection.py`, PROVEN, read directly) [r3/09:7]. Lets the reflecting LLM insert/patch/delete existing memory records in one atomic multi-tool call, shown the existing records plus a structured-extraction library (`trustcall`), bounded by `max_steps` (`src/langmem/knowledge/extraction.py`, PROVEN) [r3/09:9]. Splits memory into semantic (collection-vs-profile storage choice), episodic, procedural types, documented in `docs/docs/concepts/conceptual_guide.md` (PROVEN) [r3/09:11]. `create_prompt_optimizer` offers three interchangeable self-correction strategies — `gradient` (diagnose-then-fix, multi-step), `metaprompt` (single-call), `prompt_memory` (cheapest one-shot) — in `src/langmem/prompts/optimization.py` (PROVEN) [r3/09:13]. `create_multi_prompt_optimizer` attributes blame across multiple agent roles' prompts from one team-level score, in `docs/docs/guides/optimize_compound_system.md` (PROVEN) [r3/09:15]. It is "add-only by default": deletion is gated behind `enable_deletes` (default off), and even when enabled the memory manager only returns `RemoveDoc` objects for the calling code to act on, never deleting itself — confirmed directly against `langchain-ai.github.io/langmem/guides/extract_semantic_memories/` [02-recommendations-second-opinion.md:33; r6/20:57-59]. Named by 02 as "the one product surveyed that got this right" on append-only/no-silent-destruction design [02-recommendations-second-opinion.md:33].
2. **Bad/weak/rejected:** A real, open GitHub issue (`langchain-ai/langmem#154`) reports memory writes silently failing when the tool schema doesn't serialize cleanly — a `PydanticSerializationError` buried at DEBUG level, not raised as an exception, so a developer's Postgres `store` table stayed empty despite "correct" usage [r3/09:21]. LangMem's conceptual-guide docs claim retrieval ranking combines semantic similarity with "importance" and recency/frequency "strength," but reading the actual code (`MemoryStoreManager._sort_results` in `extraction.py`) shows it sorts purely by raw similarity score — no importance/recency weighting implemented. Flagged explicitly: "CLAIMED in docs, not found in source" [r3/09:43].
3. **File/module/schema paths cited:** `src/langmem/reflection.py` (`ReflectionExecutor`); `src/langmem/knowledge/extraction.py` (`MemoryManager.invoke`/`ainvoke`, `MemoryStoreManager._sort_results`); `docs/docs/concepts/conceptual_guide.md`; `src/langmem/prompts/optimization.py` (`create_prompt_optimizer`); `docs/docs/guides/optimize_compound_system.md` (`create_multi_prompt_optimizer`).
4. **Verdict:** ADOPTED (conceptually) — its add-only-by-default, model-recommends/code-executes deletion pattern is explicitly the design pogan-mem's docs point to as correct, and its debounce/consolidation mechanism is cited as the reference implementation for "background consolidation" [00-review-history.md:27].

---

## LangGraph (langchain-ai/langgraph, part of langchain-ai org)

1. **Good:** `get_state_history(config)` returns every past `StateSnapshot` for a thread, tagged with step number and source; you can resume from any checkpoint via `checkpoint_id`, or `update_state` to fork a new branch that leaves the original history intact — genuinely non-destructive (PROVEN from official docs, confirmed again at `docs.langchain.com/oss/python/langgraph/checkpointers` in r4/15) [r3/09:17; r4/15:65-70]. Three explicit durability tiers as a deliberate trade-off knob: `"exit"` (fastest/least safe), `"async"` (default), `"sync"` (slowest/safest) — cited as "a clean, reusable design for how aggressively pogan-mem should flush memory writes" [r3/09:23]. `BaseStore` cross-thread memory: hierarchical namespace tuples, runtime-templated fields, `get`/`search`/`put`/`delete` plus async variants, optional per-item TTL with refresh-on-access [r3/09:37].
2. **Bad/weak/rejected:** "Time travel"/replay only forks LangGraph's *internal state object* — it has no concept of the real filesystem, git repo, or shell Claude Code actually mutates; rewinding the graph does not revert a git commit, an edited file, or a sent message. Nodes after the chosen checkpoint fully re-execute, "including any LLM calls, API requests, or interrupts — which are always re-triggered during replay," so it is not deterministic replay [r3/09:17, confirmed r4/15:65-70]. Long-running background persistence has an open memory-leak bug tied to the default `durability="async"` setting (`langchain-ai/langgraph#7094`, open, 14 comments) — checkpoint-write coroutines accumulate across supersteps under backend latency [r3/09:23]. `interrupt()`/human-in-the-loop: on resume the node **restarts from its own beginning**, so any code before the `interrupt()` call re-runs — non-idempotent side effects double-fire unless guarded [r3/09:39].
3. **File/module/schema paths cited:** No specific internal file paths given for LangGraph itself beyond the doc/reference URLs above (`docs.langchain.com/oss/python/langgraph/checkpointers`, `reference.langchain.com/python/langgraph/types/Durability`); LangMem's file paths are listed separately above.
4. **Verdict:** PARTIALLY ADOPTED — the three-tier durability-flush model and the "resume-from-checkpoint" concept are cited as reusable design menus (CONCEPT-level, "for the rewind a coding agent use case" — the state-fork mechanism is PROVEN, but doesn't solve real filesystem/git rewind, which "has no LangGraph analog"). LangGraph's own "Reflection Agents" tutorial content (Reflection, Reflexion, Language Agent Tree Search) is explicitly noted as tutorial-only, never shipped as a maintained package or benchmarked product — treated as STRETCH/CONCEPT, not adopted [r3/09:31].

---

## LangChain (the broader langchain-ai org / blog, not a specific package)

1. **Good:** Names four specific ways a long agent run corrupts its own context — context poisoning, context distraction, context confusion, context clash — from the July 2025 blog post "Context Engineering for Agents" (PROVEN as a shipped taxonomy, descriptive not benchmarked) [r3/09:19].
2. **Bad/weak:** Self-reported, unverified numbers: "3-fold" improvement from routing tool descriptions through retrieval rather than stuffing all tool specs into context (CLAIMED, self-reported, no independent replication found); cites Anthropic's "up to 15x more tokens" for multi-agent systems, which LangChain did not measure itself, only cited [r3/09:33].
3. **File/module/schema paths cited:** None beyond the blog post itself.
4. **Verdict:** PARTIALLY ADOPTED — the four-failure-mode taxonomy is cited as a useful framework for testing pogan-mem's injection budget/hook design (00-review-history.md notes it under "the graph recommendation and the note on the five disciplines"); the token-reduction and multi-agent-cost figures are flagged unverified/cited, not built from directly.

---

## Aider (Aider-AI/aider)

1. **Good:** Its repo-map feature parses every file with tree-sitter, builds a graph of "file A uses something defined in file B," and runs PageRank over it to rank files by importance, giving artificial boosts (50x/10x edge weight) to files already open in chat or matching recent user words (PROVEN, source read directly, "close to a reference implementation of the parser-spine idea") [r3/10:7]. Praised again in 02-recommendations-second-opinion.md as "the same trick Google originally used to rank the web, and Aider already runs it over repositories," recommended for ranking which undocumented files matter most by importance rather than age [02-recommendations-second-opinion.md:125].
2. **Bad/weak:** Caches results in SQLite keyed on file modification time; **disables itself entirely on very large repos rather than degrading gracefully** — "a size ceiling to plan around" [r3/10:7].
3. **File/module/schema paths cited:** `repomap.py` ("worth reading the actual file, not just re-deriving the idea from scratch") [r3/10:7].
4. **Verdict:** ADOPTED (as a pattern) — the PageRank-over-import-graph idea is explicitly recommended to be reused for ranking undocumented files by importance in 02's Phase-4-adjacent section 4 [02-recommendations-second-opinion.md:125].

---

## OpenHands (OpenHands/software-agent-sdk, main org OpenHands/OpenHands)

1. **Good:** The "condenser" keeps a long autonomous run inside its context budget by replacing the first half of an append-only event log with a single summary event while leaving the second half word-for-word, repeating as the run continues. Fires on "soft" (token/size budget nearing) or "hard" (actual context-window error) triggers, with a hard trigger forcing full forget-and-summarize reset (PROVEN, source+docs read directly) [r3/10:9]. Re-confirmed independently at `docs.openhands.dev/sdk/arch/condenser`: head/tail/middle structure — preserves first `keep_first` events (default 4) plus recent tail verbatim, summarizes the middle; soft trigger = event count > `max_size`; hard trigger = a `CondensationRequest` event fired via `view.unhandled_condensation_request` on actual overflow [r4/15:59-63].
2. **Bad/weak:** Summarizing can itself violate the model's expected message structure (e.g., an unanswered tool call), so the condenser must detect that and skip a cycle rather than corrupt the transcript [r3/10:9].
3. **File/module/schema paths cited:** No specific internal file paths beyond the "condenser" module/doc page name (`docs.openhands.dev/sdk/arch/condenser`).
4. **Verdict:** ADOPTED — cited as "a concrete, shipped answer to how you keep a long autonomous run coherent without just truncating," listed among round-three additions integrated into the master capability list [00-review-history.md:31].

---

## Cline (cline/cline) — "Memory Bank"

1. **Good:** Memory Bank is a folder of plain markdown files (`projectbrief.md`, `activeContext.md`, `systemPatterns.md`, `techContext.md`, `progress.md`) that Cline's own system prompt says it "MUST read ALL... at the start of EVERY task — this is not optional." The interesting part is "the policy, not the format: full mandatory reload every task, no summarization, no selective loading — a deliberately simple, brute-force freshness guarantee" (PROVEN, docs read directly, re-confirmed at `docs.cline.bot/features/memory-bank` in r4/15) [r3/10:11; r4/15:73-77].
2. **Bad/weak:** "A purely manual-discipline system — if nobody remembers to say 'update memory bank,' the files silently go stale and nothing forces a re-sync" [r3/10:11]. Confirmed: updates happen "when user requests with **update memory bank**," implying no automatic refresh [r4/15:77].
3. **File/module/schema paths cited:** `projectbrief.md`, `activeContext.md`, `systemPatterns.md`, `techContext.md`, `progress.md` (the five Memory Bank files).
4. **Verdict:** Not explicitly adopted or rejected as a whole system in the excerpts read, but its staleness/manual-discipline weakness is cited as a cautionary example (contrasted against pogan-mem's proposed structural, code-derived staleness detection in 02-recommendations-second-opinion.md section 4).

---

## Roo Code (RooCodeInc/Roo-Code)

1. **Good:** Shares Cline's Memory Bank markdown convention (it's a fork ancestor) — no new capability [r3/10:35].
2. **Bad/weak:** The repository itself is now **archived** (shut down 15 May 2026 after the team pivoted to a cloud product, "Roomote," on 21 April 2026), users redirected to a community fork ("ZooCode") or back to Cline [r3/10:35]. Confirmed live at 24.4k stars with archive banner [r6/20:21-23]. Cited in 02-recommendations-second-opinion.md's supply-chain section as "a well-known agent fork archived at 24,364 stars" — one of four cited "abandonments" used as a supply-chain risk warning [02-recommendations-second-opinion.md:162].
3. **File/module/schema paths cited:** No file paths given.
4. **Verdict:** REJECTED / used as a cautionary supply-chain example (single-vendor tool risk), not a capability to adopt.

---

## Agno (agno-agi/agno)

1. **Good:** Ships two opposite memory-writing modes: "automatic" (every user message mechanically processed into memory) and "agentic" (the model gets memory-management tools — create/update/delete — and decides at run time what's worth keeping); agentic wins when both are on (PROVEN as a shipped feature, docs read directly) [r3/10:13].
2. **Bad/weak:** "Agno's own docs supplied no benchmark, latency, or accuracy numbers for either mode — there is no evidence on which mode actually produces better memory, only that both exist and are configurable" [r3/10:13].
3. **File/module/schema paths cited:** No file paths given.
4. **Verdict:** Cited as a confirming data point (a second/third example of "let the model decide what's worth remembering," alongside Mem0 and CrewAI) [r3/10:31] — not separately adopted/rejected.

---

## CrewAI (crewAIInc/crewAI)

1. **Good:** Recently collapsed its older four-type memory system into a single `Memory` class organizing memories into filesystem-like scopes (e.g. `/project/alpha`), letting an LLM infer scope/category/importance automatically rather than developer-designed schema (PROVEN, docs read directly; "genuinely new relative to the master list's storage-tradeoff chapter") [r3/10:15].
2. **Bad/weak:** Default embedder is OpenAI `text-embedding-3-large` and default categorization LLM is GPT-4o-mini — both paid — though a fully local path exists via Ollama for both roles. Second gotcha: switching embedding models changes vector dimensions, and CrewAI's docs explicitly warn old memory stores built on a different embedder's dimension count silently stop being compatible [r3/10:15].
3. **File/module/schema paths cited:** No file paths given (only the `Memory` class name and `/project/alpha`-style scope example).
4. **Verdict:** Cited as confirming evidence for the "let the model auto-file memory at write time" pattern; no explicit adopt/reject stated for pogan-mem specifically.

---

## OWASP Agent Memory Guard (OWASP/www-project-agent-memory-guard)

1. **Good:** Treats "an attacker gets malicious instructions written into an agent's persistent memory, surviving context resets and future sessions" as its own threat category, distinct from prompt injection. Runs as middleware in front of memory writes (works with LangChain, OpenAI Agents SDK, AutoGen/AG2), applies YAML-declared policies — allow/redact/quarantine/block — against protected-key tampering, PII leakage, abnormal write sizes, self-poisoning loops (PROVEN as shipping code, not just a checklist) [r3/10:17]. Confirmed as an official OWASP Incubator Project at `owasp.org/www-project-agent-memory-guard/` [r4/15:9-14]. Called "the only project anywhere treating inbound poisoning as its own threat" [00-review-history.md:31].
2. **Bad/weak:** The headline 92.5% detection rate / 59-microsecond latency figures are the project's own self-reported benchmark on 55 real-world attack payloads, not independently verified. The 92.5% figure lives on the PyPI page, not the main owasp.org landing page — same self-report, different location [r3/10:17; r4/15:11-14, 83].
3. **File/module/schema paths cited:** No specific internal source file paths given, only that it's a shipping PyPI package (`pypi.org/project/agent-memory-guard/`) with YAML policy rules.
4. **Verdict:** ADOPTED (conceptually) — cited in the master capability list as a round-three addition ("the memory store as an attack surface") [00-review-history.md:31], and pogan-mem's own design already commits to labeling outside content untrusted/quarantined-forever at capture, matching this pattern [02-recommendations-second-opinion.md:57].

---

## MemoryOS (BAI-LAB/MemoryOS)

1. **Good:** Splits memory into short-term/mid-term/long-term tiers where mid-term segments accumulate an automatically-computed "heat" score (visit frequency + length) and get promoted to long-term once heat crosses a threshold — no human/cron decides when (EMNLP 2025 Oral paper + reference implementation) [r3/10:19].
2. **Bad/weak:** Reports "49.11% and 46.18% average improvement in F1 and BLEU-1" on LoCoMo but the README does not name the specific baselines those percentages are measured against — CLAIMED, self-reported. Requires an OpenAI-compatible API key by default (Anthropic/Deepseek/Qwen/local vLLM also supported) — not local-first out of the box [r3/10:19].
3. **File/module/schema paths cited:** No specific file paths given.
4. **Verdict:** Not explicitly adopted/rejected — logged as a "genuinely new capability" data point, no follow-up decision recorded in the reviewed rounds.

---

## MemOS (MemTensor/MemOS)

1. **Good:** Organizes memory into a three-layer inspectable graph: L1 traces (raw execution records), L2 policies (learned behavioral patterns), L3 world models (crystallized knowledge) — revised via plain-language correction, not just appended to. Self-hosted open-source path uses Neo4j + Qdrant, no paid keys required (there's also a paid cloud tier, out of scope) [r3/10:21].
2. **Bad/weak:** Headline numbers (LoCoMo 88.83, LongMemEval 89.20, SWE-Bench 38.46, etc.) come from "OmniMemEval," a benchmark the project built itself and describes as evaluating "14 commercial memory products" — self-run, not independent (CLAIMED) [r3/10:21].
3. **File/module/schema paths cited:** No specific file paths given.
4. **Verdict:** Not explicitly adopted/rejected in the reviewed rounds beyond being logged as a "genuinely new capability."

---

## memanto (moorcheh-ai/memanto)

1. **Good:** Ships 13 built-in memory categories (instruction, fact, decision, goal, preference, relationship, etc.) as a first-class taxonomy, plus a three-verb API (`remember`, `recall`, `answer`) — "a fixed taxonomy is more decidable than free-text tagging" [r3/10:23]. Runs fully local via Docker, no mandatory paid key (hosted option has 100k-op free tier).
2. **Bad/weak:** "No independent evidence of how accurately memories actually get sorted into the right category — that claim rests on the project's own description" (CLAIMED) [r3/10:23].
3. **File/module/schema paths cited:** No file paths given.
4. **Verdict:** Not explicitly adopted/rejected — logged as a capability data point only.

---

## Continue.dev (continuedev/continue)

1. **Good:** N/A — no positive memory mechanism cited; only cited as a deprecation/abandonment data point.
2. **Bad/weak:** The `@Codebase` context provider (semantic search over an indexed embedding store of the repo) is formally deprecated in favor of "a more integrated approach to codebase awareness" (folding retrieval into agent-mode tool use rather than a separate RAG index) — confirmed at `docs.continue.dev/reference/deprecated-codebase` [r3/10:27; r4/15:33-37]. The whole `continuedev/continue` repository README states: "no longer actively maintained and is read-only for all users," following a final 2.0.0 release — confirmed directly from GitHub [r3/10:27; r4/15:33-37].
3. **File/module/schema paths cited:** No file paths given beyond the deprecation-doc URL.
4. **Verdict:** REJECTED / used as corroborating evidence — cited as "a second, independent vendor" (after Anthropic/Claude Code) deprecating embeddings-based codebase retrieval in favor of agentic search, strengthening the case against vector-indexed code retrieval [r3/10:27; 00-review-history.md:31].

---

## txtai (neuml/txtai)

1. **Good:** Bundles sparse+dense hybrid search, graph-network construction, and LLM-driven entity extraction for knowledge graphs into one Python package that runs fully offline with no mandatory API key — "a concrete existence proof that the fused-retrieval shape... doesn't require a heavyweight graph database to stand up" [r3/10:29].
2. **Bad/weak:** None stated — "doesn't add a new capability beyond what Cognee/GraphRAG already demonstrate conceptually."
3. **File/module/schema paths cited:** No file paths given.
4. **Verdict:** Not explicitly adopted/rejected — logged as confirming evidence that a smaller embedded alternative to Neo4j/Kuzu exists.

---

## AutoGen / AG2 (ag2ai/ag2)

1. **Good/Bad:** The `Teachability` capability existed in the old `pyautogen` (`autogen.agentchat.contrib.capabilities`) codebase but is **absent from the current `ag2ai/ag2` source tree** — a live GitHub code search for the class returns nothing; "appears to have been quietly dropped rather than carried forward into AG2's rewrite," with framework-level memory now expected to come from integrations (Mem0, etc.) instead [r3/10:36].
3. **File/module/schema paths cited:** `autogen.agentchat.contrib.capabilities` (old pyautogen module path, now absent).
4. **Verdict:** REJECTED / logged as "checked and found nothing new" — no capability adopted.

---

## Khoj

1. **Good:** A mature, well-run "second brain" personal-knowledge-management tool (36.1k stars) with genuine self-hosted, model-agnostic semantic search over personal documents.
2. **Bad/weak:** "Its memory model is document-retrieval-for-a-human-user, not agent-coding session memory; nothing here maps onto Cody's spine-plus-memory-layer design beyond what's already covered" [r3/10:37].
4. **Verdict:** REJECTED (out of scope for the coding-agent-memory design).

---

## Second-Me

1. **Good/Bad:** "An ambitious 'train a small local model to represent your identity' concept (Hierarchical Memory Modeling, L0/L1/L2 layers, 15.6k stars, no paid keys needed)" but stalled — last push 2025-09-30, roughly ten months of silence, "public roadmap still describing May 2025 plans." Conceptually already covered by hierarchical-memory entries (HippoRAG, MemGPT-style tiering) [r3/10:38].
4. **Verdict:** REJECTED / stalled, not adopted.

---

## amfs ("git for agent memory")

1. **Good:** Confidence-scored findings shared between agents, trust adjusted by real-world outcome feedback, plus git-like branch/diff/rollback of memory state.
2. **Bad/weak:** Only 59 stars, no visible commit history in the fetched README, branching/rollback features gated behind a paid Pro tier — "doesn't clear the bar for a newcomer worth treating as proven evidence" [r3/10:40].
4. **Verdict:** REJECTED (noted only "so the search is on record as having covered it").

---

## GEPA (gepa-ai/gepa)

1. **Good:** "Open-source models + GEPA beat Claude Opus 4.1" (Databricks case study, per README); works provider-agnostically via LangChain's `init_chat_model`; "35x faster than RL: 100–500 evaluations vs. 5,000–25,000+ for GRPO" — CONFIRMED at the project's own README, though the exact 100–500 figure is the README/marketing framing, not a line pulled from the peer-reviewed paper body (arXiv:2507.19457) [r4/15:23-30]. Corrected claim, tracked across rounds: an earlier draft wrongly claimed automated prompt optimizers inherently bill a metered API call per attempt — GEPA's own documentation disproves this, since it "runs against local models at near-zero cost" [00-review-history.md:33; 02-recommendations-second-opinion.md:139].
2. **Bad/weak:** None stated beyond the sourcing caveat above (README claim vs. paper claim).
3. **File/module/schema paths cited:** No internal file paths given, only the GitHub README itself.
4. **Verdict:** REJECTED as a discipline to build/adopt for pogan-mem ("Prompt engineering should come off the list... the free version is 'keep two or three wordings, log which one works, pick by eye'") — but the billing-cost objection to it was itself REVERSED/corrected in round three: GEPA is not inherently metered-API-expensive; the refusal survives only on the narrower ground that most of pogan-mem's prompts lack an automatically checkable score to optimize against [02-recommendations-second-opinion.md:139; 00-review-history.md:33].

---

## Microsoft GraphRAG (microsoft/graphrag)

1. **Good:** Ships a "Fast" indexing mode (`IndexingMethod.Fast`) replacing LLM-per-chunk entity extraction with cheap grammatical noun-phrase extraction — no model calls for graph construction — via `build_noun_graph`, backed by non-LLM extractors. Actively maintained (latest release v3.1.1, 18 July 2026), MIT-licensed [r2/C-claim-audit.md:124-132].
2. **Bad/weak:** The other, more radical half of "LazyGraphRAG" (deferring expensive reasoning to query time) never shipped — no lazy/deferred search engine exists in `query/structured_search/`, only `basic_search`/`drift_search`/`global_search`/`local_search`. The "fast" mode still runs LLM calls to summarize communities (`create_community_reports`), so it's cheaper, not free. The 1000x-cheaper-indexing / 700x-cheaper-query figures are Microsoft Research's own marketing numbers for a research prototype (Nov 2024 blog post, 20 months stale), not what the shipped `--method fast` flag delivers [r2/C-claim-audit.md:122-130]. Separately (via Han et al.), Microsoft's community-clustering GraphRAG loses to plain vector RAG on 3 of 4 test cells against HippoRAG 2 [r2/C-claim-audit.md:140-149]. Microsoft's own maintainers confirmed they have not implemented entity resolution, and a known bug silently deletes nodes instead of merging them ("Postgres"/"PostgreSQL"/"pg" becoming three disconnected things) [02-recommendations-second-opinion.md:93].
3. **File/module/schema paths cited:** `graphrag/config/enums.py` (defines `IndexingMethod.Fast`); `index/workflows/factory.py` (`_fast_workflows`, swaps `extract_graph` for `extract_graph_nlp`); `extract_graph_nlp.py`; `np_extractors/` directory containing `cfg_extractor.py` (context-free-grammar noun-phrase chunking), `regex_extractor.py`, `syntactic_parsing_extractor.py`; `query/structured_search/` directory containing `basic_search`, `drift_search`, `global_search`, `local_search`; `docs/blog_posts.md` (only file referencing "lazygraphrag" in a repo-wide code search, aside from `regex_extractor.py`).
4. **Verdict:** REJECTED for pogan-mem's core design (too expensive/lossy for prose extraction, entity-resolution bugs) but its NLP-based "fast" extraction mode is cited approvingly as evidence that "the field's response to LLM extraction cost was to make extraction cheap [with grammar rules], not abandon graphs" [r2/C-claim-audit.md:126-132, 172].

---

## qmd (tobi/qmd)

1. **Good:** "Mini cli search engine" combining BM25 full-text search (SQLite FTS5), local vector semantic search (300MB local embedding model), and an LLM re-ranker (640MB local reranker), merged via reciprocal rank fusion and position-aware blending, "all running locally" — 28,438 stars. Karpathy's own gist recommends it as the tool to reach for once a hand-maintained index outgrows itself. Design note: "Pure RRF can dilute exact matches when expanded queries don't match," so top keyword hits are preserved rather than trusting the reranker blindly [r2/E-named-voices.md:34, 182-187].
2. **Bad/weak:** Its own example-fixture numbers (BM25 ~0.50, vector ~0.70, hybrid ~1.00) are "a single fixture, not a benchmark, so treat as illustrative only." A widely repeated claim that Lütke called Claude Code's search "brute force" could not be verified — the X post it's attributed to actually says only "QMD 🫡" [r2/E-named-voices.md:184, 188].
3. **File/module/schema paths cited:** No specific internal source file paths given (README-level description only).
4. **Verdict:** Not explicitly adopted/rejected for pogan-mem in the reviewed material — cited as evidence for "hybrid keyword+vector+rerank, all local" being a coherent, defensible design point, and as a data point that "there is no knowledge graph anywhere in qmd" (i.e., even the search-maximalist camp doesn't reach for graphs) [r2/E-named-voices.md:186].

---

## Context Hub (andrewyng/context-hub)

1. **Good:** A documentation-retrieval layer for coding agents (13,878 stars) using keyword search over versioned markdown files with YAML frontmatter, driven from a CLI (`chub search openai`) and an agent skill — no embeddings, no vector store, no graph. Stated rationale: transparency (inspect exactly what the agent reads) and version control [r2/E-named-voices.md:84].
2. **Bad/weak:** None stated; "the README does not argue against vector search, it simply does not use it" — a revealed preference, not a stated position [r2/E-named-voices.md:86].
3. **File/module/schema paths cited:** No specific internal file paths given.
4. **Verdict:** Cited approvingly as part of "Camp A" (compile knowledge into files, navigate with index + keyword search) but not separately called out as adopted/rejected for pogan-mem specifically.

---

## Google Open Knowledge Format (GoogleCloudPlatform/knowledge-catalog)

1. **Good:** v0.2 spec (8,090 stars, updated 24 July 2026 by Amir Hormati): markdown files with YAML frontmatter and *untyped* markdown links between concepts — "the semantics are conveyed by surrounding prose, not by the link structure itself," with "no schema registry, no central authority, and no required tooling." Cited as evidence that "Google, given every opportunity and every incentive to specify a graph, specified a folder of markdown files in git" [r2/E-named-voices.md:232].
3. **File/module/schema paths cited:** `okf/SPEC.md`.
4. **Verdict:** Cited as corroborating evidence for the file-based (Camp A) position; no explicit adopt/reject action for pogan-mem stated.

---

## Canopy (LioraLabs/canopy)

1. **Good:** Semantic-search tool (tree-sitter parsing + vector embeddings + a graph of code relationships — calls, imports, containment) claiming 85–91% token reduction for AI-agent codebase exploration, benchmarked in a companion repo (canopy-bench, 132 runs across 11 exploration tasks on rust-analyzer, Gitea, Sentry). MIT-licensed, donation-funded (lower vendor-incentive risk) [r2/D-practitioner-consensus.md:17].
2. **Bad/weak:** Baseline is "Claude Code with no Canopy" (raw file reads), not compared against keyword/grep search — "measures 'indexed retrieval beats an agent poking around a large repo file-by-file,' not 'semantic beats keyword.'" Self-published, not independently reproduced [r2/D-practitioner-consensus.md:17, 46].
3. **File/module/schema paths cited:** No internal file paths given.
4. **Verdict:** Not explicitly adopted/rejected — cited as "the one place graphs/semantic search earned real endorsement for code specifically," but narrowly scoped to navigating very large unfamiliar codebases, not general agent memory [r2/D-practitioner-consensus.md:58].

---

## Rawq (auyelbekov/rawq)

1. **Good/Bad:** Claims a 10k-file codebase search returns "5-10 relevant chunks instead of 50+ full files" — "a qualitative claim, not the '4x fewer wasted tokens' headline that shows up in aggregator listings. No published methodology, no baseline stated explicitly" — "a builder with impressions dressed up as a number" [r2/D-practitioner-consensus.md:18].
4. **Verdict:** REJECTED / low-credibility tier, not adopted.

---

## HumanLayer's ACE-FCA series (humanlayer/advanced-context-engineering-for-coding-agents)

1. **Good:** Not a memory-system codebase but a coding-agent context-engineering practice guide/repo; concrete numbers from the original: 300k-LOC Rust codebase, ~170k-token context window, target 40-60% context fullness, ~$12k/month Opus spend, 35k LOC shipped in 7 hours, intern shipping 10 PRs by day 8. Its follow-up "Why Software Factories Fail" post is committed at `wsff.md` in the repo (22 July 2026); "Benchmarking Opus 5 on SlopCodeBench" at `benchmarking-opus-5-on-slop-code-bench.md` (24 July 2026): Opus 5 passes 4/17 checkpoints strictly (24%) vs 6% for Opus 4.8/Sonnet 5, while writing 5x more functions and triggering slop rules on 93% of its lines [r2/D-practitioner-consensus.md:19; r2/E-named-voices.md:150-152].
2. **Bad/weak:** "None of these three documents mention graph databases, vector search, or RAG at all" — their entire answer to long-run context management is markdown planning artifacts + human review, explicitly not a retrieval system [r2/D-practitioner-consensus.md:19; r2/E-named-voices.md:154].
3. **File/module/schema paths cited:** `wsff.md`, `benchmarking-opus-5-on-slop-code-bench.md` (both doc files inside the repo).
4. **Verdict:** Cited approvingly as "the most-cited coding-agent-specific context-management advice on HN" and as corroborating "start with no persistent memory system, use structured markdown, add retrieval only for a demonstrated need" [r2/D-practitioner-consensus.md:56, 58] — no memory-system component of it is separately adopted since it has none.

---

## wolffbe/dmas-memory (companion repo to Wolff & Bennati benchmark paper)

1. **What it is:** Not itself a memory product — the code/data companion to arXiv:2601.07978 (Wolff & Bennati), which benchmarks mem0/Graphiti/cognee/plain-RAG/full-context. README (hop 0, read via GitHub API) states the paper "was submitted to IEEE COMPSAC 2026" with no acceptance link yet, and the repo has 1 star [r2/C-claim-audit.md:91].
2. **Finding used from it:** v4 of the paper (13 July 2026) found mem0 (81.08% accuracy, $5.43 cost) and RAG-baseline (78.31%, $0.65) clustering well ahead of Graphiti (56.03%, $6.95) and cognee (55.27%, $2.99) — "all cross-cluster comparisons significant at p=0.002," attributed to "retrieval incompleteness rather than reasoning failure" for the graph systems [r2/C-claim-audit.md:67-79].
3. **File/module/schema paths cited:** No internal file paths given (README only).
4. **Verdict:** Used as evidence against graph-based memory (Graphiti, cognee) in favor of simpler vector/RAG approaches; the repo itself is not adopted (it's a benchmark harness, not a memory product).

---

## dial481/locomo-audit (Penfield Labs)

1. **What it is:** An independent audit tool/report, not a memory product. Its finding: the LoCoMo benchmark's LLM judge (gpt-4o-mini) accepted 62.81% of deliberately wrong answers when adversarially tested — confirmed as dated (March 2026) and in-window [r2/B-2026-evidence.md:64, 82].
2. **Verdict:** Used as evidence to discount LoCoMo-benchmark vendor claims broadly (Zep, Mem0, Letta, and most agent-memory vendors report LoCoMo scores as headline evidence) — cited repeatedly as the reason to build one's own 15-20 question answer key instead of trusting any vendor's benchmark number [02-recommendations-second-opinion.md:67]. Not itself a repo to adopt.

---

## codebase-memory-mcp (DeusData/codebase-memory-mcp)

1. **Good:** Its own headline number: "5 typical structural questions cost about 3,400 tokens through the map versus about 412,000 tokens reading files one at a time" (~99% fewer tokens, CLAIMED/self-reported, correctly hedged in the source document). Benchmarked on Django: 49,398 nodes, 196,022 edges, built in ~6 seconds on a laptop; queries under 1ms; a 5-level "who calls what" trace at 0.3ms (per the tool author's own academic paper, arXiv:2603.27277) though the tool's shipped README claims <10ms for the same operation (~30x looser, an internal source inconsistency); full dead-code scan ~150ms. Commits a zstd-compressed snapshot (8-13:1 ratio) with a `.gitattributes` line (`merge=ours`) auto-created on first export so concurrent binary-artifact edits never conflict. Cross-repo route matching via `CROSS_*` edges linking nodes across multiple repos indexed under the same store (confidence-scored, not a guarantee). 35-language benchmark: 17 languages score ≥90%, but OCaml scores 72% and Haskell 62% (Tier 3, <75%) [r6/20:7-41, 115-119].
2. **Bad/weak:** All of the above numeric claims are vendor/tool-author self-reported (the paper's first author, Martin Vogel, is the same person who built and ships the tool). The 0.3ms figure only exists in the academic paper's Table 8, not the tool's own README (which says <10ms for the same operation) — a source inconsistency the master document's numbers table doesn't flag [r6/20:19-23].
3. **File/module/schema paths cited:** `README.md`, `docs/BENCHMARK.md`.
4. **Verdict:** Cited approvingly as evidence for the general "cheap, tree-sitter-derived spine" design pattern (git-committed compressed snapshot with auto-merge config) — but 02-recommendations-second-opinion.md explicitly REJECTS committing any binary index to git for pogan-mem's own use ("One tool does this deliberately, with automatic conflict-free merging, and it is a reasonable choice for a shared team monorepo. Across 36 repositories it means binary churn... Rebuild, do not ship") [02-recommendations-second-opinion.md:146] — so the *pattern* is acknowledged as valid for other use cases but explicitly REJECTED for pogan-mem itself.

---

## Serena (oraios/serena)

1. **Good:** Wraps real language servers across 40+ languages (CONFIRMED) [r6/20:43-47].
2. **Bad/weak — REVERSED VERDICT:** An earlier draft of the master document claimed Serena "pings a server on every launch with your OS, version, and config," used to disqualify it for client-facing machines. This was found WRONG in round 6: Serena's own maintainer (GitHub Discussion #380, a security-audit thread) states on record "Serena doesn't send data anywhere, all data interaction is handled by the MCP client (the one exception is token counting by the Anthropic API, if explicitly configured)." Reading `analytics.py` directly confirms only local token-count estimation (tiktoken, or opt-in Anthropic API), no telemetry endpoint. The behavior actually belongs to codegraph (see below); it appears the claim migrated from the codegraph entry to the Serena entry during drafting and was never re-checked [r6/20:49-55; 00-review-history.md:45].
3. **File/module/schema paths cited:** `analytics.py`.
4. **Verdict:** REVERSED — Serena was wrongly disqualified in early rounds; round 6 clears it of the telemetry claim. No final adopt/reject decision for pogan-mem is stated beyond removing the disqualifying claim.

---

## codegraph (colbymchenry/codegraph)

1. **Good:** Watches the filesystem directly (native FSEvents/inotify/ReadDirectoryChangesW), triggers re-index on a debounce window, and runs "a fast filesystem-based reconciliation" catch-up pass on reconnect specifically to handle changes made while it wasn't running (e.g., a teammate's `git pull`, another editor, a finished agent session) — CONFIRMED against the README [r6/20:57-61; 00-master-capability-list.md:53].
2. **Bad/weak — this is the tool the mis-attributed Serena telemetry claim actually describes:** `TELEMETRY.md` documents a payload (`machine_id`, `codegraph_version`, `os/arch`) sent as **daily aggregated totals** (not every launch), opt-out via a visible default-on toggle at install time, an env var, or the `DO_NOT_TRACK` standard — explicitly excluding source code, file paths, and personal data [r6/20:55; r7/24-upstream-error-sweep.md:15]. Cited by 02-recommendations-second-opinion.md's refuse list: "The code-graph indexer codegraph sends a daily aggregated usage payload... unless its visible toggle is turned off... simply unacceptable defaults for client repositories" [02-recommendations-second-opinion.md:152].
3. **File/module/schema paths cited:** `README`, `TELEMETRY.md`.
4. **Verdict:** PARTIALLY adopted as a pattern (live filesystem-watch + reconnect reconciliation is praised as a capability, "Works today") but REJECTED as a tool to run as-is on client machines due to telemetry-on-by-default — "a setting to check rather than a disqualification" [00-master-capability-list.md:53].

---

## basic-memory (basicmachines-co/basic-memory)

1. **Good:** Derives a navigable graph live from `[[wikilink]]`-style markdown links plus YAML frontmatter typed "relations," with no separate database — CONFIRMED directly against the repo [r6/20:63-67].
3. **File/module/schema paths cited:** No specific internal file paths given beyond the wikilink/YAML-frontmatter convention description.
4. **Verdict:** Not explicitly adopted/rejected — cited as a confirming existence-proof for markdown-derived graphs without a separate DB.

---

## memsearch (zilliztech/memsearch)

1. **Good:** "Markdown is the source of truth, and the Milvus vector index is a derived cache that can be rebuilt at any time with `memsearch index`" — CONFIRMED directly against the README [r6/20:69-73]. This "markdown is truth, index is derived/throwaway" mechanism is cited approvingly and matches pogan-mem's own "derived vs. asserted" storage philosophy [02-recommendations-second-opinion.md — see "derived versus asserted" framing].
2. **Bad/weak:** The master document's client-support list ("targeting Claude Code, OpenCode, and Codex CLI") undercounts by one — the live README names **four** supported clients: Claude Code, OpenClaw, OpenCode, and Codex CLI (a `plugins/openclaw/` directory ships a full plugin). Could not determine if this is a stale miss or a since-added client (CHANGELOG.md 404'd) [r6/20:75].
3. **File/module/schema paths cited:** `plugins/openclaw/` (directory).
4. **Verdict:** ADOPTED (conceptually) — its "markdown is truth, vector index is a throwaway derived cache" design is explicitly the pattern pogan-mem's own architecture copies (see "derived vs asserted" seam in 02-recommendations-second-opinion.md section 1).

---

## ast-grep (ast-grep/ast-grep)

1. **Good:** Matches code by structure rather than text — e.g., a pattern like `console.log($ARG)` matches regardless of line breaks/spacing, using `$`+uppercase as a wildcard — CONFIRMED near-verbatim against its own docs [r6/20:95-99].
4. **Verdict:** Cited as a confirming capability example (structural matching); no explicit adopt/reject action recorded for pogan-mem in the reviewed material.

---

## fiberplane/drift

1. **Good:** Fingerprints the code a doc references — "hashes a normalized AST fingerprint (node kinds + token text, no whitespace or position data)" via XxHash3, and `drift check` flags a doc when that fingerprint stops matching — CONFIRMED explicitly against the repo [r6/20:101-105]. Cited again in discussion.Q+A.md: "Hash the referenced code; when the hash changes, flag the doc. This is what Swimm and the open-source `drift` do... its advantage is that the doc needs to declare nothing" [discussion.Q+A.md:17].
2. **Bad/weak:** Only works where the doc actually quotes code [discussion.Q+A.md:21].
3. **File/module/schema paths cited:** No specific internal file paths given beyond the general "AST fingerprint via XxHash3" mechanism description and the `drift check` command.
4. **Verdict:** ADOPTED — explicitly cited as the mechanism to build for documentation-drift detection ("Build the version that works instead: bind a doc to a specific function, fingerprint that code, flag the doc when the fingerprint changes. That is what the two shipping tools in this space actually do") [02-recommendations-second-opinion.md:149].

---

## pytest-testmon (tarpas/pytest-testmon)

1. **Good:** Uses Coverage.py to record which lines each test actually executed, diffs against changes, and re-runs only affected tests — CONFIRMED [r6/20:107-111].
4. **Verdict:** Cited as a confirming capability example (dependency-aware selective re-run); no explicit adopt/reject action recorded for pogan-mem specifically.

---

## engRAM

1. **Good:** "AEAD-encrypted embeddings, hybrid recall, per-record crypto-shred deletion, and hash-chained audit log" for AI agent memory — the only surveyed open-source tool that ships per-record crypto-shredding (encrypting each client's private material under its own key so destroying the key makes it unreadable everywhere at once) [r6/20:51; 00-master-capability-list.md:107, 312]. **Reversed claim, tracked across rounds:** an earlier draft of 02-recommendations-second-opinion.md said "No memory product surveyed does this" — round 6 found engRAM contradicts that as a general market claim, though it's unclear whether engRAM was within the original survey's scope; 00-master-capability-list.md was already correctly worded ("a thin gap rather than an empty one"), and 02's contradicting sentence was flagged as a BLOCKER needing a fix in round 7's sign-off review [00-review-history.md:47, 56; r7/22-final-accuracy-signoff.md:17-23].
3. **File/module/schema paths cited:** No file paths given; no GitHub URL/owner given in the documents read (only the tool name "engRAM").
4. **Verdict:** ADOPTED (conceptually, as day-one requirement) — pogan-mem's design commits to per-client crypto-shredding "on day one" because "cryptographic erasure only counts if the data was encrypted before storage" [02-recommendations-second-opinion.md:41, 190], citing engRAM as the existence proof the pattern is real and buildable, even though it remains a niche tool.

---

## sqlite-vec (asg017/sqlite-vec) and better-sqlite3-multiple-ciphers (m4heshd/better-sqlite3-multiple-ciphers)

1. **What was studied:** An empirical spike (not a narrative "good/bad" survey) testing whether `sqlite-vec` loads and works inside a `better-sqlite3-multiple-ciphers` (chacha20-encrypted SQLite) build, across 5 test cases [spec/reports/r4-spike-sqlite-vec-cipher.md]. Versions used: Node v22.22.3, `better-sqlite3-multiple-ciphers` 12.11.1, `sqlite-vec` 0.1.9, underlying SQLite 3.53.2. Prebuilt binaries only, no compile-from-source needed (`sqlite-vec-linux-x64` npm package's `vec0.so`).
2. **Good:** All 5 cases PASS: plain-file KNN correct; encrypted-file KNN correct with `PRAGMA key`, reopening without the key fails loudly (`SQLITE_NOTADB`), not silently; in-memory 1,000-vector/384-dim build in 18.59ms with ~0.744ms average KNN query time; `vec0` + `fts5` + WAL + `busy_timeout` all coexist in one encrypted connection with no symbol clash. No extension-loading refusal, no cipher incompatibility observed anywhere.
3. **Bad/weak:** None found — the only errors hit were the spike author's own API-usage mistakes (passing a raw `ArrayBuffer` instead of `Buffer.from(...)`; passing a JS `number` instead of `BigInt` for a `vec0` rowid), not defects in the library combination.
4. **File/module/schema paths cited:** No internal source file paths from either external repo were cited (only npm package/version names and the prebuilt `vec0.so` binary).
5. **Verdict:** ADOPTED — confirms pogan-mem's spec choice of "one native module, one process" (`better-sqlite3-multiple-ciphers` + `sqlite-vec` in the same encrypted connection), unblocking the step-0 spike gate in the spec.

---

## Other repos named only in passing (no file paths, thin/no good-bad detail, not separately adopted/rejected)

- **Letta** — "sleep-time compute" grounded in a real paper (arXiv:2504.13171, Berkeley team behind Letta) [r6/20:89-93]; also cited as scoring 74.0% on LoCoMo with gpt-4o-mini using plain text blocks, no graph [r2/B-2026-evidence.md:45]. No file paths given.
- **Graphiti** and **cognee** — repeatedly cited as the graph-based/hybrid systems that lose to mem0/RAG baselines in the Wolff & Bennati benchmark (see wolffbe/dmas-memory above) and are described as needing "temporal traversal or typed relationship queries" to justify their graph use [r2/B-2026-evidence.md:45]. No file paths given for either.
- **Devin** (docs.devin.ai) — "the closest existing product match to a human-gated memory": proposes a rule, won't save without explicit human edit-then-save or dismiss — CONFIRMED against Devin's own docs [r6/20:83-87]. Not an open-source repo (vendor product, no GitHub URL).
- **Cloudflare Agent Memory** — four-way ingest split (Facts/Events/Instructions/Tasks), an 8-point verifier (identity, timing, org context, etc.), deterministic (non-AI) date arithmetic — all CONFIRMED against Cloudflare's own docs/blog [r6/20:77-81]. Not an open-source repo (vendor product).
- **HippoRAG 2** (OSU/UIUC academic project) — "the best graph method anyone has independently benchmarked," using a PageRank-style approach; wins 3 of 4 cells against Basic RAG and MS-GraphRAG in one independent benchmark, loses to plain RAG on Natural Questions single-hop [r2/C-claim-audit.md:138-151; r6/20:159-163]. No GitHub file paths given (paper-only entity in these documents).
- **Swimm** — proprietary (no GitHub repo/URL given); auto-detects when linked code changes, marks docs "Review required" — functionally confirmed but the "fingerprint" mechanism terminology couldn't be independently verified against Swimm's own docs [r6/20:101-105].

---

## Summary note on the ECC / claude-mem addendum (02-recommendations-second-opinion.md lines 201-206)

These two repos are the subject of a dedicated round-1 report (`r1/05-ecc-claudemem.md`) which is OUT OF SCOPE for this extraction (r1 excluded per task brief). However, the addendum text itself lives inside 02-recommendations-second-opinion.md, which IS in scope, so its content is reported here from that file directly. Repo identities found via grep in other in-scope files: **ECC = `github.com/affaan-m/ECC`**, **claude-mem = `github.com/thedotmack/claude-mem`** (both citations found in r1/05 and r1/07, not fetched/read since r1 is out of scope — flagging the URLs only as provenance, not as verified-in-scope content).

- **ECC — ADOPTED parts:** deterministic capture through hooks rather than skills (ECC's own docs say hooks fire 100% of the time vs. skills' 50-80%); secret-scrubbing before disk; session-start injection with hard character caps + visible truncation marker + code-enforced quality floor; project-scoped memory keyed to git-remote identity; human-triggered promotion of a project lesson to global; worktree-aware pre-compaction summaries; every threshold exposed as a tunable knob; running background AI through the local `claude` binary to ride the subscription instead of a metered key [02-recommendations-second-opinion.md:203].
- **ECC — REJECTED parts:** the background observer that mines every tool call for patterns (ECC itself ships it off by default, called "fragile where enabled"); the confidence-update-and-decay scheme ("lives only inside a prompt the model may or may not follow... nothing in ECC's deterministic code ever recalculates a confidence number"); automatic decay generally; "GateGuard's" (ECC's own sub-feature) re-grep-the-world-before-every-first-edit *implementation* — its *policy* survives but is served by a precomputed spine instead — and the other ~280 bundled skills (not memory) [02-recommendations-second-opinion.md:203]. GateGuard itself is separately described in 00-master-capability-list.md:73 as "Works today, and genuinely clever" as a policy/concept even while its literal re-grep-per-edit implementation is not reused.
- **claude-mem — ADOPTED parts:** single-file SQLite store with FTS5 keyword search built in; graduated cheapest-first retrieval menu (know already → fetch by ID → structural outline → full read as last resort); relevance scoring favoring edited-not-glanced files and focused not sweeping changes; recency check that stops surfacing notes older than a file's last modification (kept as display heuristic only, never grounds for deletion); quota-aware brake watching subscription rolling-usage windows; the files-read/files-modified-as-separate-lists data-model detail [02-recommendations-second-opinion.md:205].
- **claude-mem — REJECTED parts:** the separate background worker process + separate Chroma vector-database process (blamed for "hundreds" of orphaned processes and a ~$2,089/month billing incident via an auth-fallback path — pogan-mem instead runs vectors in-process via sqlite-vec); the compress-everything AI-summarization-of-every-tool-call capture model; paid cloud sync (uploads full prompt text, collides with client confidentiality); telemetry-on-by-default. Noted gap: "claude-mem has no self-improvement loop at all — nothing turns a repeated failure into a changed behaviour" [02-recommendations-second-opinion.md:205; 02-recommendations-second-opinion.md:152, 147].
