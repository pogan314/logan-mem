---
title: Research extract — repos studied in round-1 reports
type: wiki
status: research-fact
created: 2026-08-21
updated: 2026-08-21
sources: [the old repo's research reports under ../pogan-toolkit/docs/superpowers/brainstorming/ (reports/r1 … r9), read 2026-08-21, docs/ai-agent-memory-unified-report.md]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions. The per-repo mechanisms, weaknesses, and file paths are external facts (file paths re-verified live in `../seed-repos/` for the seed repos only); every "adoption status" / "verdict" line is the OLD project's history and binds nothing here.

# External repos/products studied in round-1 research (pogan-toolkit brainstorming)

Source files (all under `docs/superpowers/brainstorming/`):
- `reports/r1/01-graph-engineering.md` (01)
- `reports/r1/02-repo-spine.md` (02)
- `reports/r1/03-memory-products.md` (03)
- `reports/r1/04-claude-code-memory.md` (04)
- `reports/r1/05-ecc-claudemem.md` (05)
- `reports/r1/06-self-improving-loops.md` (06)
- `reports/r1/07-context-harness-engineering.md` (07)
- `reports/r1/08-graph-storage.md` (08)
- `00-master-capability-list.md` (ML)
- `reports/03-audit-of-master-list.md` (AUDIT)
- `reports/04-fix-verification.md` (FIXVER)

Depth key used below: **DEEP** = source code/docs read directly, multiple claims, own subsection. **NAMED** = mentioned by name with 1-3 specific claims but not deeply explored. **PASSING** = named once, no real detail (excluded from most entries below unless notable).

---

## Zep / Graphiti (github.com/getzep/graphiti; product: getzep.com) — DEEP

1. **Repo**: `getzep/graphiti` (engine); product Zep, hosted-only (Community Edition deprecated).
2. **GOOD**: Only system with real, shipped **bi-temporal** modeling — 4 timestamps per edge: `created_at`/`expired_at` (system time) + `valid_at`/`invalid_at` (real-world time) (01:17,30). Old fact marked "expired," reworded as history ("Maria used to work as...") instead of deleted (01:17,133 ML). Correctly orders facts fed out of chronological order using extracted real-world dates, not ingestion order — documented divorce/marriage example (01:32-33). Zep's `memory.get()` returns a synthesized `memory.context` string plus raw messages plus rated facts in one call (03:100-101). Lets a developer filter retrieved facts by a numeric rating (03:64-65). Users get an automatic private graph; Groups exist for org/shared memory (03:167).
3. **BAD**: The two "real-world" timestamps (`valid_at`/`invalid_at`) are **populated by an LLM prompt at ingestion time**, not mechanically — so "works out which is newer" partly rests on model judgment (01:30; ML §5 flags this explicitly as inconsistent with praising Cloudflare for avoiding the same pattern). No self-hosted path exists anymore — Community Edition deprecated, code moved to `legacy/`, repo README says it's "not Zep's product" (03: table row Zep). Convenience call `memory.get()` is user-scoped only and cannot pull from a Group graph — no automatic "project + shared user" composition (03:167). Publicly disputed LoCoMo benchmark vs Mem0: Zep claimed 84%, later issued two different "corrected" numbers (58.44% / 75.14%) that disagree with each other (01:135,167). Costs meaningfully more than Mem0 per independent KTH study: 3.96%-32% more tokens, 40.2% higher AWS Fargate TCO, 77-104% more CPU, 5,268-6,283% more network bandwidth, with accuracy difference not statistically significant (p>0.05) (01:147-148). Graphiti used 1.68-2.25x the tokens of Mem0 in one informal single-tester test (01:150).
4. **File paths / internals cited**: None specific inside the graphiti repo beyond the field names `created_at`, `expired_at`, `valid_at`, `invalid_at` on edges (01:17,30). No module/file paths given.
5. **Adoption status**: PARTIALLY — the *design idea* (bi-temporal edges, supersession-not-deletion) is explicitly adopted conceptually into pogan-mem's own decision model ("status only moves forward, nothing silently overwritten" — 08:200, ML §"Decisions" and "Know when something stopped being true"), but Graphiti/Zep itself is REJECTED as infrastructure (no self-hosted path, cost, LLM-guessed timestamps). ML explicitly separates "the mechanism exists" from "adopt the product."

---

## Mem0 (github.com/mem0ai/mem0; product mem0.ai) — DEEP

1. **Repo**: `mem0ai/mem0`. Also **Mem0's OpenMemory MCP** (separate sub-product) and **CaviraOSS/OpenMemory** (an unrelated third-party fork/product with the same name — see separate entry below, do not conflate).
2. **GOOD**: Pulls candidate facts from conversation with one LLM call (03:16). ADD/UPDATE/DELETE/NOOP loop — LLM classifies each new fact against similar existing memories (03:34-35, 186). Fuses vector similarity + BM25 keyword + entity-match signals into one ranked score (03:62-63). p95 response time 1.44s vs 17.1s naive full-context baseline, >90% token reduction (self-reported, LOCOMO benchmark) (03:231). Observer/background daemon pattern reused by ECC by shelling out to local CLI (see ECC entry) rather than paid key — not a Mem0 feature itself but ECC's pattern rides on the same "no paid key" idea. Ships a free/local path via Ollama swap for both LLM and embeddings (03: table row Mem0).
3. **BAD**: **Removed external graph-store support entirely in v3 rewrite** (Neo4j/Memgraph/Kuzu/Apache AGE/Neptune all dropped), replaced with vector-based "entity linking" with no typed edges and no temporal validity model (01:38-39). This is described as "PROVEN... a vendor walking back the graph approach in production." Old graph mode had multi-year GitHub issue trail (write failures, `reset()` not cleaning Neo4j data, multi-tenant isolation bugs) (01:39). Ablation: graph-augmented variant only "marginal" improvement over base vector config, ran ~3x slower, ~2x tokens (01:153-154; ML corrected figures: 2x tokens for 68.44% vs 66.88%, losing both single-hop and multi-hop sub-scores — ML §4, FIXVER item 5, sourced via third-party practitioner analysis, NOT Mem0's own publication). **Silent autonomous overwrite with no human review** — ADD/UPDATE/DELETE/NOOP decided by the LLM alone, synchronously, no confirmation, no diff, and the paper has "no discussion anywhere... of a human review step" (03:186, ML explicitly: "Violates the no-silent-rewrite rule"). **Mem0's OpenMemory MCP shipped with zero authentication on any endpoint** — any caller could read/modify/delete any other user's memories by supplying a different `user_id`; also SSRF hole (redirect to cloud-metadata endpoint) and plaintext API key storage/exposure (03:246, ML §8 — now has CVE numbers CVE-2026-59705/59706 per ML, status unconfirmed/no advisory). OpenMemory MCP official quickstart hardcodes `OPENAI_API_KEY` — not free-by-default (03: table row). Open GitHub issue #5730: extraction prompt tuned for high recall with no supported way to dial down, since prior override mechanism was removed (03:252). Publicly disputed LoCoMo claims vs Zep, and Letta's benchmarking post says Mem0 "did not respond to a request for clarification" (03:234-235).
4. **File paths / internals cited**: None given as source-code file paths in Mem0's own repo (the deep-dive was against its arXiv paper 2504.19413 and its own docs/migration guide `docs.mem0.ai/migration/oss-v2-to-v3`, not repo file paths).
5. **Adoption status**: REJECTED as infrastructure — explicitly named as violating pogan-mem's "no silent rewrite" rule (03:186, ML). Its *idea menu items* (ADD/UPDATE/DELETE/NOOP style extraction, fused scoring) are cited as "operations menu" entries but the final design (ML "Recommendations") does NOT adopt LLM-driven auto-apply of graph links — explicitly refused ("Auto-applied AI links of any confidence" — ML §"What I would refuse to build").

---

## Letta (formerly MemGPT) (github.com/letta-ai/letta) — DEEP

1. **Repo**: `letta-ai/letta`.
2. **GOOD**: **Sleep-time compute** — a separate background agent (can use a stronger/slower model) reorganizes/consolidates memory during idle time, fully non-blocking; grounded in a real paper (Letta + UC Berkeley) and cross-checked against a real test file `tests/integration_test_sleeptime_agent.py` (03:94-95, ML "wild ideas"). Memory-block sharing primitive — a block created independently of any agent can be attached to multiple agents (03:171). Filesystem-plus-grep beat its own (now-removed) graph mode on a published benchmark ("Is a Filesystem All You Need?") — 74.0% vs Mem0-graph's 68.5% (01:117-118, 03:235).
3. **BAD**: **Silent autonomous overwrite, no human review, confirmed from source** — every memory-editing function (`core_memory_append`, `core_memory_replace`, `rethink_memory`, `memory_replace`, `memory_insert`, `memory_rethink`, `memory_apply_patch` in `letta/functions/function_sets/base.py`) writes the instant the agent calls it; the only check is a correctness check, never a permission check (03:121, 187-188, ML explicitly names this "Violates the no-silent-rewrite rule"). **No built-in deduplication for its out-of-context (archival) vector store at all** — open feature request GitHub issue #3116 — same fact stored 4 separately-worded ways over time with no consolidation tool, unlike core (in-context) memory which does have a rewrite tool (03:58, 248). Token estimator calibrated for OpenAI's tokenizer runs 2-5x low against local models (Qwen/Mistral via Ollama/LM Studio); documented "death spiral" case (issue #3288) — 8 failed retries over 4 hours, no circuit breaker, required manual DB fix (03:247). No built-in project/org hierarchy at all (03:171). One user report (issue #2318) that self-hosted web UI redirected to a cloud signup even in "local" mode (03: table row Letta). Voyager (a different project, but discussed alongside Letta's memory-block precedent) is the closest analog to "promote memory to skill" and runs with **zero human review** — cited as the clearest cautionary precedent for why Letta's un-gated writes matter (06:115).
4. **File paths / internals cited**: `letta/functions/function_sets/base.py` (03:121,188) — contains `core_memory_append`, `core_memory_replace`, `rethink_memory`, `memory_replace`, `memory_insert`, `memory_rethink`, `memory_apply_patch`. Test file `tests/integration_test_sleeptime_agent.py` (03:95).
5. **Adoption status**: REJECTED for its self-editing memory mechanics ("Violates the no-silent-rewrite rule" — explicit). Sleep-time compute is cited as the closest real precedent for an idea (background consolidation) but not marked adopted into the final design; the final "what I would refuse to build" list bans exactly this shape (auto-executing agent-driven memory promotion).

---

## LangMem (github.com/langchain-ai/langmem) — DEEP, largely ADOPTED as a pattern

1. **Repo**: `langchain-ai/langmem`.
2. **GOOD**: **The one clear exception among developer tools — append-only by default.** `enable_deletes` defaults to **False**; even when turned on, the LLM only returns a `RemoveDoc` **recommendation** object, never an in-place mutation — the developer's own code decides what "remove" means (03:37,191-192, ML explicitly: "Works today (LangMem's approach)" as the one system NOT violating the no-silent-rewrite rule). Lets developer choose synchronous ("hot path", `create_manage_memory_tool`) vs deferred/background ("background" mode, `create_memory_store_manager` + `ReflectionExecutor`) extraction (03:25-26). `ReflectionExecutor` debounces — defers extraction until conversation activity settles, keeping one pending consolidation per conversation, cancel-and-reschedule on new activity (03:109-110, ML: "consolidation step shows the model existing memory records and lets it insert/patch/remove in one structured call with a bounded number of rounds"). Rewrites the agent's own system prompt over time from accumulated feedback — documented example: `"You are a helpful assistant."` → multi-point instruction set (03:91, ML "wild ideas"). "Collections" (many docs) vs "Profiles" (one schema-based record patched in place) as two configurable modes (03:158-159). Names its own explicit fork between "growing collection with history" vs "single always-current overwritten document" memory shapes — ML calls this "the only project found that names this fork explicitly" (ML §"Split memory into distinct kinds").
3. **BAD**: Exact debounce/delay parameter name for `ReflectionExecutor` not confirmed against docs (03:110). Docs describe ranking memories by importance and recency, but the shipped code sorts by raw similarity only — "a fresh instance of docs promising more than the code does" (ML §7). Own docs explicitly warn that extracting on every message is expensive — recommend deferring (03:253). LangMem "has a live report of memory writes vanishing entirely because a data-conversion error was logged at debug level instead of raised" (ML "traps" section) — one of three independent implementations losing data behind a false success signal.
4. **File paths / internals cited**: No repo file paths given — described via function/API names: `create_manage_memory_tool`, `create_memory_store_manager`, `ReflectionExecutor`, `extract_semantic_memories` guide, `RemoveDoc` object (03:26,36-37).
5. **Adoption status**: **ADOPTED (partially, as a pattern)** — its append-only-by-default / recommendation-not-execution shape is explicitly the model pogan-mem's own supersession/no-silent-rewrite rule follows ("This is the closest thing found in this whole survey to Cody's own supersession model" — 03:192). The overwrite-vs-collection memory-shape distinction is explicitly adopted into the final design (ML §"Split memory into distinct kinds"). The debounce/consolidation batching idea is cited approvingly (ML §7) though not explicitly committed to the build order.

---

## Cognee (github.com/topoteretes/cognee) — DEEP

1. **Repo**: `topoteretes/cognee`.
2. **GOOD**: `cognify()` pipeline: 2 LLM calls per chunk (entity/relationship extraction + summarization), chunks 1,024-8,192 tokens (03:18). Content-hash-based skip-if-already-ingested — "Second call is a no-op because the record is already linked to the dataset" (03:50, quoted directly). Dedup never crosses tenant/ownership boundaries (03:50, 168). 16 distinct query modes over its graph including a `CODING_RULES` mode "aimed specifically at coding-rule-style retrieval" (03:141). Dedicated `SUMMARIES` search type over precomputed chunk summaries, separate from graph facts (03:105). "Dataset" is a first-class object with ID/name/owner/permissions — closest thing to genuine project-level scoping with real access control (03:168). "Memify" stage — automated post-ingestion pruning of stale nodes / reweighting of frequently-used edges (01:73-74).
3. **BAD**: Contradiction detection is an **explicit opt-in pipeline step, off by default** — `cognify()`'s optional 7th step is literally called "detect contradictions" but doesn't run unless enabled; "the default pipeline doesn't check for conflicts at all" (03:39). **Sharp local-free gotcha**: both LLM *and* embedding provider must be switched to a local option together — configuring only one silently falls back the other to OpenAI; documented in Cognee's own local-setup guide (03: table row Cognee, ML "traps": "Cognee silently falls back to a paid provider if you configure only one of its two AI settings"). Several open GitHub issues: hangs on macOS with local OpenAI-compatible endpoints, unrelated tokenizer requirement firing even when unused, chunk sizes silently ~2x too large for local Ollama models (03:249). No explicit "org" tier above the dataset/owner level (03:168).
4. **File paths / internals cited**: No repo file paths given; described via API/pipeline-stage names — `cognify()`, `add()`, `search()`, `forget()` (its "convenience API" delete call, 03:80), the "detect contradictions" 7th pipeline step, `SUMMARIES` and `CODING_RULES` search types.
5. **Adoption status**: PARTIALLY — the *idea* of a project-scoped "dataset" with owner/permissions is a data point pogan-mem's own project/user/org layering draws on (03:168, ML "Work as a team" section), but Cognee itself is not adopted; its default-off contradiction detection and local/paid-fallback trap are cited as cautionary, and model-extracted graphs generally (which Cognee does) are explicitly REJECTED in the final design ("Model-extracted knowledge graphs from prose, built in the background with nobody reviewing them" — ML refuse list).

---

## Memobase (github.com/memodb-io/memobase) — DEEP

1. **Repo**: `memodb-io/memobase`.
2. **GOOD**: Batches extraction — flush triggers at ~1,024 buffered tokens or 1 hour idle, trading freshness for fewer LLM calls (03:24). Extracts directly into named profile fields with "flexible" (LLM can add new sub-topics) vs "strict" (fixed schema) modes, plus an on-by-default auto-validation pass filtering low-quality extractions (03:157). Reading an already-built profile: <100ms; full semantic/event search: 500-1000ms (self-reported, 03:233).
3. **BAD**: `overwrite_user_profiles` config key strongly implies in-place overwrite with **no version history** — inferred, not an explicit documented behavior (03:41,193). Closed GitHub issue #109 shows this exact overwrite path "often failed to update"/"worked only intermittently" at one point — real production reliability history (03:41,193). No documented contradiction-resolution page. Self-published LOCOMO-style benchmark: overall 75.78% accuracy but notably weak 46.88% on multi-hop questions (03:233). Last code push ~7 months before research — momentum/maintenance concern (03: table row / traps section). No sharing/hierarchy/org concept anywhere — "project" here means an API-key/tenancy boundary, "closer to a Stripe account than a folder" (03:174).
4. **File paths / internals cited**: No repo file paths; only the config key name `overwrite_user_profiles`.
5. **Adoption status**: REJECTED as infrastructure — cited mainly as a cautionary example of undocumented/unreliable overwrite behavior.

---

## Redis Agent Memory Server (github.com/redis/agent-memory-server) — DEEP (cloned and read directly)

1. **Repo**: `redis/agent-memory-server`.
2. **GOOD**: Exact-hash dedup for identical text + separate near-duplicate vector-similarity merge with an LLM, run as background compaction on a configurable interval (03:48). TTL expiry on session/working memory (03:75, table row).
3. **BAD**: **No documented contradiction-resolution logic at all** — only near-duplicate merging; silent on what happens when two memories actively disagree (03:43). **Open-source implementation recently moved into an unmaintained `V0/` folder**, marked "no longer actively maintained" — Redis is steering users toward a paid managed product ("Iris") instead (03: table row, ML "traps": "a vendor actively steering its own community away from the free path"). No real latency number for memory-specific operations (extraction/promotion/search) found anywhere despite active searching (03:236).
4. **File paths / internals cited**: `V0/README.md` deprecation notice (03:260, sources list) — the only concrete internal path cited.
5. **Adoption status**: REJECTED — cited as a data point about vendor abandonment of the free/self-hosted path, not adopted.

---

## CaviraOSS/OpenMemory (github.com/CaviraOSS/OpenMemory) — DEEP (distinct from Mem0's OpenMemory MCP — do not conflate)

1. **Repo**: `CaviraOSS/OpenMemory`.
2. **GOOD**: Has a "synthetic fallback" embedding mode plus native Ollama support — genuinely free-by-default (03: table row).
3. **BAD**: **Worst failure mode found in the entire memory-products survey**: open, unresolved GitHub issue #192 — a 64-bit simhash-based dedup hash intended for collision detection actually carries only ~11 bits of real entropy due to a bitwise-operation bug; on the reporting user's real store of 4,841 memories, **11.7% of genuinely distinct new memories silently landed on an already-used hash and were dropped**, returning `deduplicated: true` with no error — indistinguishable from success, and gets worse as the store grows (03:56,245, ML "traps" — though ML's own traps section adds the caveat "those specific figures could not be traced back to a primary source" per an audit, so the mechanism is real but the exact numbers are flagged unconfirmed in ML). Separately, `delete_all` doesn't clear its in-memory query cache — deleted memories remain retrievable until a restart (issue #186) (03:82,246). README says project is "currently being rewritten, expect breaking changes and potential bugs" (03: table row).
4. **File paths / internals cited**: No file paths; GitHub issue numbers #192, #186, #180, #197, #196, #195, #173, #147, #188 (03:260 sources list).
5. **Adoption status**: REJECTED — used purely as a cautionary failure-mode example.

---

## basic-memory (github.com/basicmachines-co/basic-memory) — DEEP, largely ADOPTED as a design pattern

1. **Repo**: `basicmachines-co/basic-memory`.
2. **GOOD**: Memory is literally human-editable Markdown files with structured "Observations" and wikilink-style "Relations" — **no LLM call in the base path, no API key needed for core operations** (03:28, ML "Keeps the memory as ordinary markdown files... already the existing design's choice"). Graph derived **live** from wikilinks written directly in the markdown — no separate database at all; `build_context` tool explicitly walks the wikilink graph (03:145, 01... actually 08 not this — corrected: cited in 03:145). Multiple named project "vaults" independently routable (`basic-memory project add research ~/research`) (03:169). Core read/write/search/graph-traversal needs **zero network calls**; optional semantic search defaults to a local embedding model (FastEmbed) — "the closest thing to 'free by construction' in this whole survey" (03: table row).
3. **BAD**: No cross-project shared layer at all — each vault is its own island unless manually linked (03:169). File-is-the-database design creates its own corruption risk: confirmed bug (issue #171) — renaming a note through the tool stripped the `.md` extension, breaking the file for other programs (Obsidian, Finder) (03:250). Broad multi-client compatibility claim (Claude, ChatGPT, Codex, Gemini, Cursor, VS Code, Obsidian) is CLAIMED from docs-site marketing copy, not independently confirmed beyond the MCP tool surface itself (03:134).
4. **File paths / internals cited**: Command example `basic-memory project add research ~/research` (03:169); MCP tool name `build_context` (03:145). No internal source file paths given.
5. **Adoption status**: **ADOPTED as a foundational design pattern** — the final design's core storage choice ("add-only markdown files... plain text in a repository you own, readable by a person, greppable" — ML §"What happens if this project itself gets abandoned", ML §"Storage: SQLite, a few small files") directly mirrors basic-memory's philosophy: markdown-as-truth, disposable/rebuildable derived index. ML explicitly credits: "basic-memory and Karpathy's 'LLM wiki' pattern both do this, and it's already the existing design's choice" (ML §3).

---

## Zilliz's memsearch (github.com/zilliztech/memsearch) — DEEP, ADOPTED as a design pattern

1. **Repo**: `zilliztech/memsearch`.
2. **GOOD**: SHA-256 content hashing for cheap exact-match chunk dedup, skips re-embedding unchanged content (03:52). Distills repeated workflows into a separate named "skill" file distinct from raw episodic notes, alongside daily journal + project/user summaries (03:93). Claude Code plugin installs in two commands (`/plugin marketplace add zilliztech/memsearch` then `/plugin install memsearch`), auto-captures conversation turns, exposes `/memory-recall` slash command; explicitly unifies memory across **Claude Code, OpenCode, and Codex CLI** (03:132, FIXVER item 11 confirms this exact 3-tool list live on the README, also now lists OpenClaw as a 4th). **Treats its fast vector index as a disposable, rebuildable cache over Markdown files that remain the actual source of truth** — "delete the index and nothing is lost, because it regenerates from the files" (03:146-147, explicitly flagged as "a genuinely useful design pattern for pogan-mem's own 'database spine' idea"). Default embeddings local ONNX (bge-m3), default vector storage "Milvus Lite" (embedded, zero-server) — free by default (03: table row).
3. **BAD**: Not marked as violating any specific rule in the reports; framed positively throughout. No deep negative findings recorded beyond it being CLAIMED (not independently verified) that it's a deliberate cross-tool sync feature vs. just "one local server, several front ends" (ML §10: "Everything else stops at 'one local server, several front ends.'").
4. **File paths / internals cited**: No internal repo file paths; only the CLI/plugin install commands and `/memory-recall` slash command name.
5. **Adoption status**: **ADOPTED as a design pattern** — the "index is a disposable cache, markdown is truth" idea is directly cited as reusable for pogan-mem's own spine/memory index design (03:147) and echoed in the final storage decision (ML: SQLite index attaches to markdown at query time, index is derived/rebuildable, markdown is the only irreplaceable part).

---

## Cloudflare Agent Memory (private beta, June 2026) — DEEP, largely ADOPTED as design ideas

1. **Repo/Product**: Cloudflare Agent Memory (blog.cloudflare.com/introducing-agent-memory) — not open source, private beta, no public repo.
2. **GOOD**: Two-pass extractor (general summary pass + targeted concrete-value pass) plus an **8-check verifier** against the source transcript before storing (entity identity, temporal accuracy, organizational context) — "more rigorous than any other product researched here" (03:19-20, ML: "Checks every extracted memory against the original conversation before storing it... More rigorous than anything else found"). Four-way classification at ingest — stable facts / dated events / procedures / short-lived tasks — "No other product in this survey does this four-way classification at ingest time" (03:21-22, ML §3). Content-addressed IDs derived from hash of session+speaker+content — re-ingesting the same transcript twice is idempotent by construction (03:54). Runs 5 parallel retrieval methods (exact key, full-text, raw-message fallback, vector, HyDE) fused via Reciprocal Rank Fusion, weighting fact-key matches highest — "the most elaborate retrieval-fusion design found in this survey" (03:66-67). **Uses regex/arithmetic instead of an LLM for date-relative questions**, because LLMs are unreliable at date arithmetic — explicitly called "a genuinely good idea worth stealing on its own" (03:68-69, ML: "A small, obviously correct idea worth stealing on its own. Works today").
3. **BAD**: Pricing/cost model unknown — private beta, waitlist-gated (03: table row).
4. **File paths / internals cited**: None (not open source; sourced from launch blog post only).
5. **Adoption status**: **ADOPTED as design ideas** — the date-arithmetic-not-LLM trick and the multi-check verifier pattern are both flagged as directly worth stealing, though as a closed product Cloudflare Agent Memory itself is not usable/adoptable as infrastructure.

---

## Anthropic's memory tool (Claude API, `memory_20250818`) — DEEP

1. **Repo/Product**: Not a GitHub repo — an Anthropic API tool type, `platform.claude.com/docs`.
2. **GOOD**: Text-editor-style tool (view/create/str_replace/insert/delete/rename) over a virtual `/memories` path (03:122-123, 04:39). Anthropic's companion pattern for multi-day coding runs: an "initializer" session sets up a progress log + feature checklist (failing/passing per item); later sessions read that file instead of re-exploring; git commits serve as rollback (ML §6, cites 4 named failure modes to watch for: declaring done when not done, leaving known bugs, marking complete because it compiles not because tested, re-discovering project structure every session).
3. **BAD**: **No built-in review step at all** — "Anthropic explicitly does not host or review the writes — this is entirely a developer responsibility" (03:123, 04:39-41, 06:48). Requires validating every path yourself against path-traversal attacks — a malicious path like `/memories/../../secrets.env` can escape the sandbox if not validated (04:41, 06:48). Requires the **paid Claude API** to run at all — violates "no paid API keys" constraint by definition, though the tool itself has no separate fee beyond ordinary token usage (03: table row). 84% token savings / 39% performance improvement figure (combined with context editing) is CLAIMED — Anthropic's own benchmark, not independently reproduced, and could not be directly re-confirmed on the live docs page in one research pass (03:43, 07:51).
4. **File paths / internals cited**: The tool's own six commands: `view`, `create`, `str_replace`, `insert`, `delete`, `rename`; virtual path prefix `/memories/...` (04:39,173).
5. **Adoption status**: PARTIALLY — the *pattern* (progress-log + checklist for long runs) is explicitly recommended for pogan-mem to "borrow the pattern and implement the file handling ourselves inside Claude Code's own tools" since the actual tool requires paid API access pogan-mem can't use (ML §6).

---

## Anthropic's Claude.ai consumer memory — DEEP

1. **Product**: Claude.ai's built-in "memories" feature (not a repo).
2. **GOOD**: **"The one genuine exception in this entire survey" for provenance/correction** — exposes a visible, human-readable "memory summary" the user can directly inspect and correct by chatting ("update the summary at any time by chatting with Claude") — "the only product found anywhere in this survey where correction is a built-in, first-class user-facing feature" (03:195-196, ML §"seven things nobody has built" item 1 cites this as an existing partial exception). Scoped per Claude Project, pitched by Anthropic explicitly as a privacy boundary (03:172-173).
3. **BAD**: "24-hour refresh, four categories" framing is CLAIMED — came through a secondary summary, could not be independently re-verified against Anthropic's primary text (03:114). No user-level memory spanning multiple projects (03:172).
4. **File paths / internals cited**: N/A (consumer product, no code access).
5. **Adoption status**: PARTIALLY — cited as the strongest real precedent for human-correctable memory, feeding directly into the "gap #1: proof a human decided something" analysis in ML, but not itself adoptable (closed consumer product).

---

## Anthropic Managed Agents "memory store" — DEEP

1. **Product**: Managed Agents (newer Anthropic product, server-hosted).
2. **GOOD**: Workspace-scoped text-file store (cap 100KB/file, 2,000 files/store), **versions every write immutably**, lets you redact a past version for compliance, background "dreaming session" that consolidates fragmented memories (04:45, ML §10: "every write versioned immutably, old versions redactable for compliance, and a background 'dreaming' pass that consolidates fragmented memories. The design is worth studying even though the product bills through the API").
3. **BAD**: **Bills through the Claude API, not a Claude subscription** — using it as-is would violate pogan-mem's "no paid API keys" constraint (04:45).
4. **File paths / internals cited**: None (closed product).
5. **Adoption status**: PARTIALLY — "Worth studying the *design*... even though the product itself isn't usable under the subscription-only constraint" (04:45) — explicitly a design-only adoption, not infrastructure adoption.

---

## Anthropic's Claude Code (memory system, hooks, skills, plugins, subagents, MCP) — DEEP, heavily ADOPTED as the harness foundation

Not a third-party product but Anthropic's own tool; documented extensively in 04, 06, 07. Key facts (selected, not exhaustive since this is the base harness rather than a "competitor"):
- CLAUDE.md: four-scope stacking (org-managed / personal `~/.claude/CLAUDE.md` / project `./CLAUDE.md` / `CLAUDE.local.md`), concatenated not overridden — cited as the working precedent for pogan-mem's own layering (04:9, ML §8).
- Auto memory (`~/.claude/projects/<project>/memory/`, `MEMORY.md` + topic files) — machine-local only, never synced (04:19-21). **Hard write-cap bug**: only first 200 lines/25KB of index loads; write past that silently drops content on next load even though the write itself "succeeded" (04:23, ML "traps": explicitly one of the document's headline failure-mode examples, though ML notes Claude Code does return an error telling Claude to shorten the file).
- CLAUDE.md is "soft guidance, not enforcement" — only a `PreToolUse` hook can hard-block (04:31). No provenance field distinguishing human-confirmed vs AI-inferred content anywhere (04:157, ML gap #1).
- Hooks: `additionalContext` capped at 10,000 chars; exit code 2 is the only blocking signal (exit 1 is non-blocking, "easy to get backwards"); `UserPromptSubmit` 30s default timeout, silently discards on timeout; `SessionEnd` 1.5s default budget (04:71-85, ML §7 — all independently reverified against live docs per AUDIT, confirmed accurate).
- Skills: progressive disclosure — ~100 tokens per description always loaded, full body (recommended <5,000 tokens) loads only on invocation; skill listing does NOT reload after `/compact` (04:93-99, 07:28-29).
- Plugins: self-contained directories shipping skills/subagents/hooks/MCP/LSP together, namespaced (`/plugin-name:skill-name`) to avoid collisions (04:105-109).
- Subagents: fresh isolated context by default (except `/fork`); built-in `Explore` agent is read-only, skips CLAUDE.md/git status for cheap research (04:127-129).
- `/run-skill-generator` and `/verify`: `/verify` "edits its own recorded recipe file, but only when a run actually failed or missed a step... Anthropic already had and fixed" an earlier broader version that caused merge conflicts — cited as "the most directly relevant precedent in this entire report" for narrow, failure-triggered self-editing (06:33-34, ML §6, verified accurate in AUDIT).
- Boris Cherny quote (verified verbatim, 1 Feb 2026): Claude Code shipped a local vector database over the codebase and then **removed it** — "agentic search generally works better" (ML numbers table; this is a real, verified finding, not a rumor).
- **Adoption status**: The Claude Code harness (hooks, skills, subagents, plugins, memory scoping) is **ADOPTED as the delivery mechanism** pogan-mem is built on top of — not a "competitor" being adopted/rejected but the substrate itself.

---

## Claude Code's `#` quick-memory shortcut — regression, noted

Removed in Claude Code v2.0.70 ("Removed # shortcut for quick memory entry (tell Claude to edit your CLAUDE.md instead)") — flagged as a capability regression worth knowing since older tutorials/training data describe it as current (04:27).

---

## OpenAI Codex (contrast, not adopted as infra) — NAMED/DEEP-ish

1. **Product**: OpenAI Codex CLI.
2. **GOOD**: `AGENTS.md` equivalent to CLAUDE.md, same closer-file-wins concatenation logic, checks for `AGENTS.override.md` at every level (04:139). Separate auto-memory (`~/.codex/memories/`), explicitly skips short-lived sessions, redacts secrets, and its own docs warn "treat memories as a helpful recall layer, not as the only source for rules" (04:141). Documented, user-initiated `/import` pulls a full Claude Code setup (AGENTS.md-equivalent, settings, skills, MCP config, hooks, subagents, and recent chat sessions from last 30 days) — confirmed manual/user-triggered, not silent background scraping (04:143).
3. **BAD**: Hard 32KB byte cap (`project_doc_max_bytes`) on how much of the base instructions file loads per run — no equivalent of Claude Code's "load in full regardless of length" (04:139).
4. **File paths/internals**: `~/.codex/AGENTS.md`, `~/.codex/memories/`, `AGENTS.override.md`.
5. **Adoption status**: Not adopted (contrast/reference only) — no design decision in pogan-mem is attributed to Codex specifically.

---

## Cursor (contrast) — NAMED/DEEP-ish

1. **Product**: Cursor (cursor.com).
2. **GOOD**: `.cursor/rules/*.mdc` with four trigger modes (always-apply, glob auto-attach, agent-selected via description, manual `@`-mention); reads plain `AGENTS.md` too (04:149). Token budget guidance: always-apply rules recommended under ~200 words each, ~2,000 tokens combined; auto-attached rules can run up to 500 lines since conditional (07: table §9).
3. **BAD**: Rules do NOT persist across separate completions by default — re-injected each time, unlike Claude Code's compaction-survival design (04:151). Its separate "Memories" feature could not be confirmed in current official docs — redirected to a generic landing page; community forum reports describe it as unstable/disappearing after updates (04:153, 06: Family F last bullet — flagged as an open question, not a settled negative).
4. **File paths/internals**: `.cursor/rules/*.mdc`.
5. **Adoption status**: Not adopted — contrast/reference only.

---

## Cline "Memory Bank" pattern (+ forks: `alioshr/memory-bank-mcp`, Roo Code) — NAMED

1. **Repo**: Cline itself (not the memory-bank-mcp repo); `alioshr/memory-bank-mcp` (916 stars, MCP-portable reimplementation); Roo Code (a Cline fork, now archived).
2. **GOOD**: Six markdown files (`projectbrief.md`, `productContext.md`, `systemPatterns.md`, `techContext.md`, `activeContext.md`, `progress.md`) the system prompt instructs the agent to read in full before every task — "brute-force full reload as a freshness guarantee" (02:68, ML §7: "the interesting part is the policy, not the format").
3. **BAD**: No structural enforcement that the agent actually updates the files — depends entirely on the model following instructions; Cline's own docs flag staleness as a real risk (02:68, ML: "if nobody says 'update memory bank' the files silently rot"). Roo Code (the well-known fork with its own memory-bank variant) is archived/no-longer-maintained as of 2026-05-15 (02:69, verified via GitHub API).
4. **File paths / internals cited**: The six named markdown filenames above (02:68).
5. **Adoption status**: NOT adopted as infrastructure but explicitly cited as "the simple end of the spectrum our design deliberately moves beyond" (ML §7) — a deliberate contrast point, not a source of borrowed mechanism.

---

## AGENTS.md standard (Linux Foundation-stewarded) — NAMED

1. **Repo/standard**: agents.md, cross-vendor.
2. **GOOD**: Vendor-neutral single project-instructions file standard, supported by 20+ tools (Codex, Jules, Gemini CLI, Copilot, Cursor, VS Code, Zed, Aider), claimed 60,000+ projects using it (02:70).
3. **BAD**: Staleness is entirely manual — "treat it as living documentation," no automatic check (02:70). 60,000+ figure is CLAIMED (self-reported).
4. **File paths**: N/A (a file-naming convention, not a tool).
5. **Adoption status**: Referenced/supported (Claude Code reads it via `@AGENTS.md` import) but not itself a "product" being adopted/rejected for pogan-mem's design.

---

# Repo-spine / codebase-map tools (report 02)

## codebase-memory-mcp — DEEP, heavily cited (though ultimately treated cautiously — see audit)

1. **Repo**: Not explicitly given a GitHub org/user in the r1 text itself (referred to throughout only as `codebase-memory-mcp`); its academic paper is arXiv 2603.27277, "Codebase-Memory: Tree-Sitter-Based Knowledge Graphs for LLM Code Exploration via MCP" (Vogel, Meyer-Eschenbach, Kohler, Grünewald, Balzer, 28 March 2026), which links `github.com/DeusData/codebase-memory-mcp` (per FIXVER item 2 — this is where the actual org/repo name surfaces: **DeusData/codebase-memory-mcp**).
2. **GOOD**: `detect_changes` tool reads git diff, resolves to exact functions/classes changed, does one graph traversal outward tagging hits CRITICAL/HIGH/MEDIUM/LOW by hop distance, outbound and inbound modes (02:7,33). `search_graph` tool: BM25 ranking with camelCase-aware tokenizing (02:25); `semantic_query` mode uses local vector embeddings (`nomic-embed-code`, compiled into the binary, no API key) (02:26). Finds dead code via Cypher-style query (`WHERE NOT EXISTS { (f)<-[:CALLS]<-() }`) in ~150ms (02:37). "Cross-repo intelligence" mode pattern-matches HTTP/gRPC/protobuf routes across separately-indexed repos (02:36). "Hybrid LSP" layer reimplements pieces of tsserver/pyright/gopls/rust-analyzer logic for 9-11 language families (02:38). Ships `PreToolUse` hook augmenting Grep/Glob calls with graph symbols, fail-open (02:65, source cited: `src/cli/hook_augment.c`, `src/mcp/mcp.c`). `PostToolUse` hook on `Read` flags files with parsing gaps (02:66). Documents 43 supported agent surfaces with per-agent hook mechanisms (02:67). Writes zstd-compressed SQLite snapshot to `.codebase-memory/graph.db.zst` (8-13x smaller than raw), auto-adds `.gitattributes` `merge=ours` line (02:58). Own benchmark: indexed Linux kernel (28M LOC, 75,000 files) in 3 minutes on Apple M3 Pro, 4.81M nodes/7.72M edges; Django in ~6s; query <1ms; 5-typical-questions cost ~3,400 tokens vs ~412,000 tokens reading files (02:13-14,23). Runs 100% locally with **zero telemetry**, confirmed by reading source, not just marketing (02:91).
3. **BAD**: Per-language accuracy varies sharply — 17 of 35 languages score 90%+, 16 score 75-89%, 2 under 75% (OCaml 72%, Haskell 62%) — because Rust traits, Scala/Ruby/C# runtime-resolved interfaces, and Haskell/OCaml typeclass dispatch aren't traced as calls at all (02:7). **The document's own audit (AUDIT/FIXVER) found this tool's own numbers are the tool author's own paper/README benchmark, "not independently reproduced" — the master list's summary table originally mislabeled it "Independent paper" TWICE, which the audit calls out as the single clearest case of the document's own "honesty tags" being compromised** (AUDIT C2, FIXVER item 2). The paper's own abstract (surfaced only at FIXVER's own primary-source check, "an asymmetry nobody has flagged") reports the tool's answer *quality* is actually **worse** than a plain file-exploring agent: "83% answer quality versus 92% for a file-exploration agent, at ten times fewer tokens and 2.1 times fewer tool calls" — cheaper but less accurate (FIXVER, closing paragraph). Doesn't explicitly document rebase behavior for its `detect_changes` `base_branch`/`since` comparison (02:15,54 — flagged as untested, not confirmed broken).
4. **File paths / internals cited**: `.codebase-memory/graph.db.zst`, `.gitattributes` `merge=ours`, `src/cli/hook_augment.c`, `src/mcp/mcp.c`, `docs/BENCHMARK.md` (02:7, referenced as the source of the 35-language accuracy table).
5. **Adoption status**: PARTIALLY — cited constantly as "the reference implementation" for the spine idea and many of its individual mechanisms (hooks, snapshot format, BM25+semantic search) are described approvingly, but the final design explicitly builds "the thinnest spine you can get away with" rather than this tool's full feature set, and the audit specifically flags its benchmark credibility as compromised (both mislabeled as independent, and its own paper shows a quality tradeoff the document never surfaced until the final verification pass).

---

## Aider (repo map / `repomap.py`) — DEEP, ADOPTED

1. **Repo**: Aider (not given a full org/repo string in text, referred to as "Aider").
2. **GOOD**: Repo map uses tree-sitter to extract function/class *signatures* (not prose) — zero LLM cost (02:11). Ranks with binary search to fit a fixed 1,000-token budget (02:28, ML §1: "the closest thing to a reference implementation of the spine idea — worth reading `repomap.py` before writing our own"). Uses PageRank's "personalization" mechanism — boosts files already open in conversation by 50x, identifiers the user typed by 10x (02:29,43).
3. **BAD**: Caches rankings keyed on file modification time; **on very large repositories it disables itself entirely rather than degrading gracefully** — "a size ceiling to plan around" (ML §1, explicit gotcha from reading the actual source). No aider-published benchmark comparing "with repo map" vs "without" was found despite specifically looking (02:29).
4. **File paths / internals cited**: `repomap.py` (02:28, ML §1 — explicitly named as worth reading directly).
5. **Adoption status**: **ADOPTED as a reference implementation** — "reading the actual source confirms it is the closest thing to a reference implementation of the spine idea" (ML §1); the PageRank-importance-flow idea is separately proposed as reusable for ranking which files most need documentation (ML §10 "wild ideas").

---

## Serena — DEEP

1. **Repo**: Serena (LSP-wrapping tool; org/repo string not given, referred to as "Serena").
2. **GOOD**: Wraps real language servers across 40+ languages — "the same accuracy an IDE gives a human" (02:27, ML §1). "Symbol overview" tool returns a file's outline via the language server (02:11,42). No persistent index at all — every query goes to a live server, so nothing can go stale in the code-map sense (02:15,53). **Maintainer states on record that Serena sends no data anywhere** (ML §1) — though see BAD below, this is contradicted by direct source reading.
3. **BAD**: **Pings home on every startup** with OS/version/config info, confirmed by reading the exact function `_send_usage_info` in `agent.py`, sending to `https://oraios-software.de/serena_usage.php` (params `os`, `dashboard`, `version`, `backend`, `context`); opt-out via `SERENA_USAGE_REPORTING=false` env var or auto-skipped when `CI`/`GITHUB_ACTIONS` detected (02:92, explicitly flagged as "a real, specific reason to avoid Serena on any machine handling client-confidential code without setting that environment variable first" — this directly contradicts the "sends no data anywhere" marketing claim ML repeats elsewhere, worth noting as an internal inconsistency in the source documents themselves). Slower per-session startup while building its own in-memory understanding (02:15,27).
4. **File paths / internals cited**: `agent.py` function `_send_usage_info` (02:92).
5. **Adoption status**: NOT adopted as infrastructure — cited as the accuracy benchmark ("IDE-grade") but pogan-mem's design goes with deterministic parsing (tree-sitter) rather than a live LSP wrapper, and the telemetry finding is flagged as a client-confidentiality risk.

---

## ast-grep — NAMED, DEEP-ish

1. **Repo**: ast-grep.
2. **GOOD**: Matches code by parsed structure, e.g. `console.log($ARG)` regardless of formatting/line breaks (02:24, ML §1). Documents an 11x self-speedup from its own optimization work (10.8s → 0.975s) (02:79).
3. **BAD**: No independent/head-to-head benchmark against grep/ripgrep from the project itself — only a self-speedup number (02:13,79). Does not persist any index between runs — re-walks the file tree fresh every call, "a fast building block... not a memory system on its own" (02:79).
4. **File paths / internals cited**: None.
5. **Adoption status**: Not directly adopted as a tool but the structural-matching *concept* is acknowledged as complementary to grep in the final design ("grep stays exactly where it is" — ML §"Retrieval").

---

## codegraph (github.com/colbymchenry/codegraph) — DEEP

1. **Repo**: `colbymchenry/codegraph`, 63,605 stars (verified live via GitHub API), MIT.
2. **GOOD**: Native OS-level file watcher (inotify/FSEvents/ReadDirectoryChangesW), debounced default 2s, sub-second re-sync on multi-thousand-file repos (02:15,49). Runs a reconciliation/"catch-up" pass on every server restart specifically to handle changes made while not running — e.g. after `git pull` or branch switch (02:15,50, ML §1). Persists to local SQLite file `.codegraph/codegraph.db` (02:17). Fully local, no API key ever required for its core graph (02:87).
3. **BAD**: Collects anonymous usage telemetry by default (commands/languages used, never code/paths per its own README) — opt-out via env var or `DO_NOT_TRACK=1` (02:87,93, ML §1: "a real, if minor, phone-home behavior worth knowing before using it anywhere client-confidential"). Nothing confirms `.codegraph/codegraph.db` is designed/documented for git-committing specifically — its freshness story is built entirely around the live watcher instead (02:17, unconfirmed either way).
4. **File paths / internals cited**: `.codegraph/codegraph.db`.
5. **Adoption status**: PARTIALLY — the "live watcher + catch-up reconciliation on restart" mechanism is cited approvingly and used as one of the two named strategies for "how does a tool stay fresh" (02:15), but not committed to in the build order (which favors deterministic on-demand parsing over a persistent watcher daemon — final design explicitly refuses "any process that outlives the session").

---

## Graphify (Graphify-Labs/graphify) — DEEP, extensively studied, ultimately NOT adopted as infra

1. **Repo**: `Graphify-Labs/graphify`. Star counts reported inconsistently across the two reports that checked it live: 99,093 (01:64), 99,073 (02:86) — both "verified live via GitHub API" on 2026-07-30, created April 2026.
2. **GOOD**: `--code-only` mode runs local tree-sitter AST parsing (~40 languages) with **zero LLM calls** — deterministic, correct, zero-cost for code specifically (01:64, 02:3 "Skips the paid-extraction step entirely for code"). Already an installed skill in this exact Claude Code environment ("graphify" skill). Self-reported benchmarks: LOCOMO QA accuracy 45.3% vs Mem0's 27.3%; LongMemEval-S 76% tied with plain dense vector retrieval; code-fact coverage on a 1M-line codebase rising from 70.8% to 82.0% when added as a tool; hybrid-RRF fusion scored 43.3% vs plain dense vector 41.3% vs BM25 31.3%, though pure graph mode edged out hybrid slightly at 45.3% on this one dataset (01:64,114). Methodology more rigorous than most vendor benchmarks — LLM-judge cross-checked by a second blind judge with 90.6% agreement, Cohen's kappa 0.81 (01:64). Writes plain `graphify-out/graph.json` file into the project — a normal committable artifact, though not marketed as a dedicated team-sync feature (02:60,86). Offers a git post-commit hook as an alternative to a live watcher — explicitly *not* a live watcher by design, can go stale between commits (02:15,52).
3. **BAD**: Its own numbers show it is not universally best — a competing tool "supermemory" beat it on raw LOCOMO QA accuracy (49.7% vs 45.3%) even though graphify won on recall (01:64). Non-code path (PDFs, images, video) requires either a paid API key (Anthropic/OpenAI/Gemini/etc.), a free local Ollama model, or riding on a coding-assistant subscription — "a real, documented exception to 'fully free'" (02:86,94). "Claim about the semantic/prose layer running on a Claude Code subscription... was not independently verified against Anthropic's own documentation this session" (01:197-198).
4. **File paths / internals cited**: `graphify-out/graph.json` (02:60,86); `--code-only` CLI flag (01:64,101).
5. **Adoption status**: **REJECTED as infrastructure for the graph layer** despite being praised extensively — the final design explicitly commits to a hand-seeded, twenty-category graph with zero LLM extraction rather than any tool (including graphify) that does model-based entity extraction from prose; graphify's zero-cost code-parsing mode is acknowledged as "directly actionable" but the actual build order (ML) uses tree-sitter directly rather than adopting graphify as a dependency.

---

## GitHub's stack-graphs — NAMED, DEEP-ish

1. **Repo**: GitHub `stack-graphs`.
2. **GOOD**: True scope-aware name resolution without needing a compiler — code doesn't even need to compile; can incrementally re-analyze just the changed file (02:7,77).
3. **BAD**: Only ever shipped rule definitions for 4 languages (Java, JavaScript, Python, TypeScript) after several years of work. **Archived by GitHub in September 2025 with no named successor** (02:7,77, verified by reading the live archived repo).
4. **File paths / internals cited**: None specific.
5. **Adoption status**: REJECTED — cited as a cautionary example of a promising rigorous approach that got abandoned by its own vendor.

---

## Swimm (commercial) and fiberplane/drift — NAMED

1. **Repo**: `fiberplane/drift` (open-source, 125 stars); Swimm (commercial, no repo).
2. **GOOD**: Both fingerprint the exact code a doc references at write time and flag the doc when the fingerprint changes later — "solved" half of the docs-drift problem (02:9, ML §2).
3. **BAD**: Neither attempts the harder "prose claim no longer true" half.
4. **File paths / internals cited**: None.
5. **Adoption status**: Not adopted (referenced as evidence the doc-drift problem splits into a solved and unsolved half).

---

## `ryanwaits/drift` and `jbrockSTL/doc-drift` — NAMED (barely studied)

1. **Repos**: `ryanwaits/drift` (4 stars), `jbrockSTL/doc-drift` (0 stars), both days-to-weeks old.
2. **GOOD/BAD**: Attempting prose-level doc-drift detection ("code changed, does the doc's prose still hold"); essentially unproven, CONCEPT status (02:9).
3. No file paths given.
4. Adoption status: Not adopted — cited only to show the gap is unsolved, not as candidates.

---

## Cognition's DeepWiki — NAMED

1. **Product**: Cognition's DeepWiki (built by the Devin team).
2. **GOOD**: The one product that generates true LLM-written repo documentation at scale, conversationally queryable (02:11,44).
3. **BAD**: Hosted SaaS; refresh mechanism and cost model could not be confirmed from public pages — "CLAIMED, low confidence" (02:11,44).
4. No file paths given (closed product).
5. Adoption status: Not adopted.

---

## pytest-testmon — NAMED, DEEP-ish

1. **GOOD**: Records exactly which lines each test touched via `coverage.py`, re-runs only tests whose recorded lines changed (02:35, ML §2).
2. **BAD**: If a code path wasn't exercised when coverage data was built, the tool has no idea any test depends on it — silent gap (02:35).
3. Adoption status: Referenced as a real mechanism for the "which tests need to run" question; not committed to in the build order but cited as a working technique.

---

## Sourcegraph SCIP / LSIF — NAMED, DEEP-ish

1. **GOOD**: Portable, static, shareable index format; 4-5x smaller and ~3x faster to process than LSIF, one 10x CI speedup case (`scip-typescript`) (02:13,17,76).
2. **BAD**: Standard workflow uploads to a Sourcegraph server rather than committing to git — "commit it yourself" is an extension, not the documented primary path (02:61,76). Coverage limited to 8 languages/groups with "generally available" indexers.
3. Adoption status: Not adopted — reference data point on index-format design only.

---

## Meta's Glean — NAMED

1. **GOOD**: ~1ms simple queries, few ms complex ones, per Meta's own engineering blog (02:13,78).
2. **BAD**: "Genuinely heavyweight" — needs both Haskell and C++ toolchains, distributed/replicated at Meta's scale — "enterprise/platform infrastructure, not something a solo developer sets up in an afternoon" (02:78).
3. Adoption status: Not adopted — scale reference only.

---

## Codanna (bartolli/codanna) — NAMED

1. **Repo**: `bartolli/codanna`, 712 stars, Apache-2.0.
2. **GOOD**: "The closest local, credible alternative to codebase-memory-mcp's exact shape (structural graph plus local semantic search, no API key)" (02:83).
3. Not independently deep-dived on capability accuracy — "flagged from prior team research as 'more modest, more credible claims' with a smaller community as the tradeoff."
4. No file paths given.
5. Adoption status: Not adopted — named alternative only.

---

## mcp-language-server (isaacphi/mcp-language-server) — NAMED

1. **Repo**: `isaacphi/mcp-language-server`, 1,573 stars, BSD-3.
2. **GOOD**: "The smallest-footprint option: a thin wrapper that gives an agent exact LSP navigation and nothing else, with zero index to ever go stale" (02:84). No persistent index — every query hits a live language server (02:15,53).
3. No file paths given.
4. Adoption status: Not adopted as infra — cited as the "no index to go stale" strategy category alongside Serena.

---

## claude-context (zilliztech/claude-context) — NAMED

1. **Repo**: `zilliztech/claude-context`, 12,215 stars, MIT.
2. **GOOD**: "A fuzzy semantic-search-first option built on a real vector database, for finding code that's conceptually similar even with no shared vocabulary" (02:85).
3. **BAD**: Needs a vector database and an embedding provider (cloud or API key) for its flagship setup — "a real tension with a no-paid-API-key constraint unless run against a local embedding model" (02:85).
4. Adoption status: Not adopted.

---

# Graph-engineering / GraphRAG family (report 01, 08)

## Microsoft GraphRAG (microsoft/graphrag) — DEEP

1. **Repo**: `microsoft/graphrag`.
2. **GOOD**: Community-hierarchy "Global Search" — partitions graph into nested clusters via Leiden algorithm, each cluster pre-summarized; own paper reports root-level summaries needing 97% fewer tokens than reading source directly while winning 72%/62% comprehensiveness/diversity vs plain vector RAG (vendor-run, LLM-as-judge) (01:20, ML §4). **Cheap grammar-based ("fast"/NLP noun-phrase) extraction mode** — no LLM judgment involved for entity extraction, in open-source GraphRAG since v2.0.0 (25 Feb 2025), still maintained as of v3.1.1 (18 July 2026), verified live directly by reading `factory.py` and `query/structured_search/` on `main` (ML §"Make the graph small", FIXVER item 7 — verified at primary source).
3. **BAD**: Entity/relationship extraction is ~75% of total indexing cost per Microsoft's own docs (01:48). One practitioner case study: indexing a 5GB legal dataset cost $33,000 in early 2024 vs "a few dollars" for plain vector indexing — anecdotal, not a controlled benchmark (01:48-49). Entity resolution **not implemented** — maintainers confirmed on GitHub discussion #778 (01:55). Bug #1718: when the same name appears with two different inferred types, dedup **silently drops all but the first node** — real data loss (01:55). Hallucinated entity/relationship descriptions reported (issue #1543) (01:57). Ground-truth-graded independent benchmark (GraphRAG-Bench, ICLR 2026, arXiv 2506.02404) found it scored **13.4% lower accuracy than vanilla vector RAG on Natural Questions** — though FIXVER later flags this specific "13.4% worse" figure as **a misattribution — the paper it cites never ran that benchmark** (ML §"honest counter-evidence" self-corrects this). Fast/NLP mode still spends model calls on community summaries (cheaper, not free); the query-time half of the LazyGraphRAG design never shipped in the open-source package (ML, correcting M3 from AUDIT).
4. **File paths / internals cited**: `factory.py`, `query/structured_search/` directory containing `basic_search`, `drift_search`, `global_search`, `local_search` (FIXVER item 7).
5. **Adoption status**: REJECTED for the extraction approach generally (model-based entity/relation extraction from prose is explicitly refused in the final design), though its cheap grammar-based extraction mode is named as "the evidence-backed direction to look" IF the hand-seeded graph ever proves too small — a contingent, not-yet-triggered adoption (ML §"Make the graph small": "If the hand-seeded graph ever proves too small... the evidence-backed direction to look is Microsoft's cheap grammar-based extraction mode").

---

## LightRAG — DEEP

1. **GOOD**: Default "mix" mode fuses entity/relationship matches with vector-similar chunks (01:108). vs Microsoft GraphRAG on a legal-domain test: 83.6%/86.4% comprehensiveness/diversity vs GraphRAG's 48.4%/26.4% (vendor-run, 01:174).
2. **BAD**: Exact-string-match-only entity merging (open feature request #1323, unshipped) (01:55). Recommends 32B+ parameter local models for decent extraction; below that, zero entities/edges silently produced (issue #30), traced partly to Ollama's default 8k context window truncating LightRAG's required 32k prompts (01:60-61). Unresolved issue: only using one of eight available GPUs per extraction request, 5+ months with no maintainer response (01:96).
3. File paths: none given.
4. Adoption status: REJECTED — used as a cautionary example of local-model extraction failure modes.

---

## LazyGraphRAG (Microsoft Research) — NAMED, DEEP-ish

1. **GOOD**: Swaps LLM entity extraction for plain noun-phrase extraction, pushing expensive reasoning to query time — indexing cost "identical to plain vector RAG," ~0.1% of full GraphRAG's indexing cost; comparable quality to GraphRAG Global Search at >700x lower query cost (vendor benchmark, LLM-as-judge, 100 synthetic queries, 5,590 AP articles) (01:51-52).
2. **BAD**: Whether it shipped into the open-source `microsoft/graphrag` repo (vs staying in Azure-only products) "was not confirmed as of this research" (01:52) — later research (ML, FIXVER item 7) established the *fast/NLP* extraction mode DID ship in open source since v2.0.0, but "the query-time half of the design never shipped in the open-source package at all."
3. Adoption status: Contingent/not adopted (see GraphRAG entry above — same family).

---

## fast-graphrag (Circlemind) — NAMED

1. **GOOD**: Self-run benchmark vs Microsoft GraphRAG on 2WikiMultiHopQA: 96%/95% vs GraphRAG's 75%/68% (two query sets) (01:173).
2. Not otherwise deep-dived; vendor self-run, no independent check noted.
3. Adoption status: Not adopted, name-referenced only.

---

## nano-graphrag — NAMED

1. **GOOD**: Proof that a minimal from-scratch GraphRAG reimplementation is only ~1,000 lines of code (01:98).
2. Adoption status: Not adopted — cited only to support feasibility of a lightweight custom extraction pipeline.

---

## HippoRAG / HippoRAG 2 (OSU NLP Group) — DEEP, most-praised "graph wins" case, still NOT adopted as infra

1. **GOOD**: **The one graph technique with genuine, corroborated independent wins.** Personalized PageRank spreads relevance outward from seed nodes (01:124-125). Own paper: MuSiQue Recall@5 69.7%→74.7%, 2WikiMultiHopQA Recall@5 76.5%→90.4% vs strong pure-embedding baseline (01:14). Average F1 59.8% vs NV-Embed-v2 baseline's 57.0% and HippoRAG 1's own 53.1% (01:168). Two separate outside benchmark papers (GraphRAG-Bench ecosystem; arXiv 2604.09666 "Do We Still Need GraphRAG?") both independently found it the strongest or near-strongest performer, including beating Microsoft's own community-clustering approach — "This is the one 'graph beats vector' claim in this whole space that wasn't just winning in its own paper" (01:125). Has an explicit synonym-resolution/entity-resolution step built in (01:55,194).
2. **BAD**: **Loses to plain vector search on simple single-fact questions** — 61.03 vs 64.78 F1 on Natural Questions per Han et al. (ML §4, AUDIT M1 — "not universally better than the simpler alternative"). One of the two outside corroborations was later found to be "weaker than the earlier pass suggested" — about the GraphRAG category generally under amortization conditions, not HippoRAG 2 specifically (AUDIT M1, FIXVER item 6 confirms this narrowing is accurate). Underperforms plain embedding retrieval on HotpotQA specifically, since most HotpotQA questions are answerable from two closely-related passages (01:126).
3. File paths: none given (research paper, no shipped repo emphasized).
4. Adoption status: **NOT adopted as infrastructure** despite being the single strongest evidence-based case for graph retrieval found in the whole research pass — the final design explicitly refuses model-extracted graphs and PageRank-style traversal machinery in favor of a small hand-seeded 20-category graph; HippoRAG 2's mechanism is cited as evidence "graphs can work" in the abstract but not built.

---

## Cognee's "memify" — see Cognee entry above (already covered).

## A-MEM (Rutgers/AIOS Foundation, NeurIPS 2025, arXiv 2502.12110) — NAMED, DEEP-ish

1. **GOOD**: Zettelkasten-modeled — new note automatically links to and can retroactively edit older related notes; documented support for running fully locally via Ollama, no paid API key (01:70-71).
2. **BAD**: Own README points elsewhere for "reproducing the paper's results" — research-grade, not maintained product. Self-reported numbers not independently reproduced; a later paper (D-MEM, arXiv 2603.14597) beats it on the same LoCoMo benchmark (37.4% vs 35.9% F1) — "A-MEM's absolute numbers are already middling by 2026 standards" (01:71).
3. Adoption status: NOT adopted — CONCEPT-leaning-CLAIMED status, no mechanism directly borrowed.

---

## MemOS / MemTensor (github: MemTensor/MemOS) — DEEP-ish

1. **GOOD**: Structured as a graph, inspectable/editable by design (not opaque embeddings). Fully local deployment path (`docker compose up` or self-hosted, plain SQLite, no API key) (01:77). Organizes memory as **three inspectable layers** — raw traces, learned patterns, settled higher-level knowledge — revisable from plain-language correction rather than only appending (ML §10). 10,000 stars, pushed within a day of last check (ML §10).
2. **BAD**: Benchmark claims (35.24% token savings; one integration improving task completion 36.63%→50.87%; LoCoMo 88.83/LongMemEval 89.20 on an unclear scale) are vendor-reported, scoring scale not confirmed (01:77). "Every headline number it reports comes from a benchmark suite it built itself and ran on its own product against fourteen competitors, so the layered design is the takeaway, not the scores" (ML §10).
3. File paths: `docker compose up` deployment path only (01:77).
4. Adoption status: PARTIALLY — the three-layer (raw/pattern/settled-knowledge) organizational idea is presented as a design worth noting in the "wild ideas" section, but not committed to the actual build order.

---

## engRAM — NAMED (from master list only, not deeply covered in the r1 files read)

1. **GOOD**: One open-source tool that ships **per-record crypto-shredding** (encrypting each client's material under its own key, so discarding the key makes it unreadable everywhere at once — every backup, every synced copy, every machine) for agent memory. Cited as "a thin gap rather than an empty one" — the only product found anywhere with this (ML §3, ML "seven things nobody has built" item 3: "The thinnest of the seven: engRAM already ships per-record crypto-shredding for agent memory").
2. **BAD**: None specified beyond "It remains absent from every product you would otherwise adopt" (i.e., it's a real but obscure/niche tool).
3. No file paths or org/repo string given anywhere in the documents read.
4. Adoption status: PARTIALLY — the crypto-shredding *concept* is directly adopted into pogan-mem's own client-data-destruction design (decision 2, decision 12 in ML), citing engRAM as prior art/existence proof, though engRAM itself is not used as a dependency.

---

## OWASP Agent Memory Guard — NAMED, DEEP-ish

1. **GOOD**: Incubator project shipping as real installable middleware; stands guard in front of every memory write, can allow/redact/quarantine/block; policies for protected-key tampering, oversized writes, personal data, and an agent caught re-poisoning its own memory in a loop; "the only project found anywhere that treats the memory store itself as an attack surface" (ML §8).
2. **BAD**: 92.5% detection figure is self-reported, not independently verified; at 108 stars it is early (ML §8).
3. No file paths given.
4. Adoption status: PARTIALLY — cited as validating the need for write-path defenses (poisoning/untrusted-content handling), which pogan-mem's own design addresses, though OWASP's specific middleware is not adopted as a dependency.

---

# ECC and claude-mem (report 05) — both DEEP, cherry-picked for ideas, NOT adopted wholesale

## ECC (github.com/affaan-m/ECC) — DEEP

1. **Repo**: `affaan-m/ECC`. MIT. 236,185 stars / 1,222 real subscribers (~193:1 ratio, flagged as untrustworthy) (05:11,95).
2. **GOOD**: `PreToolUse`/`PostToolUse` hooks on `*` matcher watch every tool call deterministically — 100% guaranteed to fire, replacing an earlier "skill"-based mechanism that only fired 50-80% of the time based on model judgment (05:15,51). Secret-scrubbing regex filter before anything touches disk (05:16). Project identity detection in priority order (override var → git remote URL → local path) survives moving a repo (05:18). Background Haiku observer runs by shelling out to the local `claude` CLI binary — **rides the existing subscription instead of a metered key**, verified in `observer-loop.sh` (`claude --model haiku --print`) — "a real, verified example of 'no paid API keys' compliance... isn't something the README calls out" (05:35, ML §6: "It's invisible unless you read the shell script — the README never mentions it. Works today, and it's the pattern that makes 'no paid API keys' achievable"). Injects up to 6 highest-confidence "instinct" notes at session start, capped 8,000 chars total with explicit truncation marker if cut (05:22-23). Confidence floor 0.7 for auto-injection (hard-coded constant, `DEFAULT_INSTINCT_CONFIDENCE_THRESHOLD`) (05:24). Promotion rule (project→global) requires 2+ projects AND avg confidence ≥0.8 — **hard numeric code-enforced constants**, `PROMOTE_MIN_PROJECTS=2`, `PROMOTE_CONFIDENCE_THRESHOLD=0.8` (05:32,69-71). **"GateGuard"** — blocks first edit to a file in a session, forces the agent to investigate (grep for importers, check schema) before proceeding — "a cheap, real-time substitute for the pre-computed dependency graph... makes the agent rediscover the graph on the spot, every time" (05:34,77, ML §2: "It's a cheap substitute for a maintained map... Works today, and genuinely clever"). Worktree-aware pre-compaction summaries — matches summary to the correct git worktree so 3 checkouts of the same repo don't cross-contaminate (05:29,79).
3. **BAD**: **Documentation describes a confidence-update scheme (+0.05/confirm, -0.1/contradict, -0.02/week decay) that the deterministic code never actually implements** — the entire scoring behavior lives only in natural-language instructions given to the background Haiku model; reading `instinct-cli.py` end to end shows the CLI never recalculates confidence, only reads whatever's already in the file — "freezes permanently if that background process doesn't run" (05:69, ML "traps": explicitly cited as "Documentation describes features the code doesn't have"). Background observer/distillation daemon is **off by default** and fragile on Windows/macOS — several open, unresolved issues: #2626 (Windows stat-field bug breaks every write/read), #2489 (daemon doesn't survive on native Windows), #2600 (hook wrapper echoes entire stdin payload back to stdout — potential leak), #2371 (ranking not stack-aware), #2463 (skill-health dashboard stub, always reports 0 runs), #2431 (installer silently skips 79 curated skills due to stale manifest), #2428 (ESET flags a shipped skill as "GenAISkill.IC Trojan," likely false positive) (05:85-91). Past fixed but notable: #42 command-injection vuln, #2165 API tokens leaking into process lists/shell history (05:92). Separate paid commercial "ECC Tools team plans" running alongside the free MIT collection under the same brand (05:93). Installing ECC installs all 283 bundled skills by default, most unrelated to memory — no clean "install just the memory part" (05:94). Instinct-to-skill generation (`/evolve --generate`) is **plain string concatenation, not an AI synthesis step** — "best described... as 'auto-generated outline from your notes,' not 'the AI wrote you a new skill'" (05:31,73).
4. **File paths / internals cited**: `skills/continuous-learning-v2/hooks/observe.sh`, `scripts/hooks/session-start.js` (`DEFAULT_MAX_INJECTED_INSTINCTS`, `DEFAULT_SESSION_START_CONTEXT_MAX_CHARS`, `DEFAULT_INSTINCT_CONFIDENCE_THRESHOLD`), `scripts/hooks/pre-compact.js`, `skills/continuous-learning-v2/agents/observer.md`, `instinct-cli.py` (including `_generate_evolved()`, `promote --dry-run`), `observer-loop.sh`, `_registry_lock()`, env vars `ECC_SESSION_START_MAX_CHARS`, `ECC_MAX_INJECTED_INSTINCTS`, `ECC_INSTINCT_CONFIDENCE_THRESHOLD`, `ECC_OBSERVER_SIGNAL_EVERY_N`, `ECC_HOOK_PROFILE`, `ECC_DISABLED_HOOKS`. Data model: `observations.jsonl` (raw log) and per-instinct YAML-frontmatter-plus-markdown files with fields `id`, `trigger`, `confidence`, `domain`, `source`, `scope`, `project_id`/`project_name` (05:44-47).
5. **Adoption status**: PARTIALLY — GateGuard's "force investigation instead of storing a map" pattern is explicitly cited as a cheap alternative worth knowing (ML §2), and the "ride the subscription via the local CLI binary" pattern is explicitly adopted as *the* mechanism that makes "no paid API keys" achievable (ML §6). But ECC's automatic un-gated promotion logic, its confidence-scoring machinery (which is unenforced/fake), and its skill-generation-by-templating are not adopted; the final design explicitly keeps promotion human-gated and refuses automatic execution-promotion.

---

## claude-mem (github.com/thedotmack/claude-mem) — DEEP

1. **Repo**: `thedotmack/claude-mem`. Apache-2.0. 89,074 stars / 280 real subscribers (~318:1 ratio, flagged as untrustworthy) (05:103,188).
2. **GOOD**: Hook fires on every tool use, returns in <20ms (self-measured p50/p95/p99), actual AI summarization runs in a **separate background worker process** so it never blocks typing (05:107-108). **"File Read Gate"** — intercepts a `Read` call on a file with prior notes and adds a timeline of past work as supplementary context; skips files under 1,500 bytes; ranks notes (edited > read, focused change > broad sweep); skips entirely if file modified more recently than newest note; deduplicates to one entry per session (05:110-114,153-163, ML §7). **Graduated cheapest-first retrieval menu**, dogfooded across 3 subsystems (session-start injection, Read gate, search): recognize you know enough (free) → fetch by ID (~300 tokens) → structural outline/function body via tree-sitter AST tool (~400-2,000 tokens) → full file read as last resort (05:115,167, ML §7: "a stronger design signal than any single feature"). Tree-sitter structural map across **24 programming languages** (05:116, v12.0.0 changelog). Falls back from semantic (vector) to keyword search automatically if vector DB unreachable (05:118). Deduplicates via content hash (05:119). **Quota-aware safety brake** — watches actual rolling Claude subscription usage windows (5h, plus 7-day Opus/Sonnet windows) and aborts background compression before blowing the plan limit — exact thresholds: 5h at 95%, 7-day Opus at 93%, 7-day Sonnet at 92% (05:126, ML §6: "directly answers the 'won't this cost me a fortune' worry"). Unusually well-engineered, unusually honestly documented telemetry design — strict allow-list (not block-list), separate "allow then redact" pipeline for free-text error messages, explicit published "one-way door" caveat (05:120-124,171). Telemetry rollup cut projected monthly bill from ~$7,700 to ~$10 while keeping the same aggregate shape (self-reported) (05:123).
3. **BAD**: **Documentation-vs-code mismatch confirmed by reading the actual handler**: public docs (`docs/public/file-read-gate.mdx`) still describe a **deny-and-block** flow ("Read blocked: This file has prior observations..."), matching how the feature launched (v12.0.0 changelog explicitly said it "uses `permissionDecision` deny"), but the **current source code allows the read and appends the timeline alongside the full file content** — `permissionDecision: 'allow'` in `src/cli/handlers/file-context.ts` — "the real current-version savings are smaller than the headline 75-97% number implies" (05:161-163, ML §7: "one correction: the documentation describes it blocking the read, but the shipped code allows the read and appends the timeline, so the advertised savings are overstated"). **Historical incident, now fixed**: worker used to silently bill a user's Claude Max subscription at retail per-token rates whenever no explicit API key was configured — one user reported **~$2,089 drained in a single month** before tracing the cause (issue #2698, closed/fixed v13.3.0) — "the single most on-point historical cautionary tale for Cody's 'no paid API keys' rule" (05:181, ML §6: "It exists because of a real incident where the tool quietly drained about $2,089 in a month"). Multiple open resource-leak issues: #3404 (worker never reaps headless subprocesses — one user reported 203 accumulated over 4 days, ~45GB RAM), #3413/#3382 (orphaned `chroma-mcp` instances, hundreds of orphans, ~2GB leaked, described as "silent" since orphans page out to near-zero), #3450 (killing worker can leave port bound via inherited socket handle — 296+ stuck processes reported), #3406 (on macOS, worker spawned by desktop app can't reach login keychain — observations silently discarded, no visible error) (05:177-180, ML "traps" cites the 203-process/45GB and hundreds-orphan/2GB figures directly). Content-hash dedup only catches byte-for-byte identical text — near-duplicates still pile up (issue #3038/#3163) (05:185). Open bug: empty-but-technically-successful semantic search fails to correctly fall back to keyword search, silently returning "nothing found" (issue #3361) (05:186). Paid "cmem.ai Pro" cloud-sync add-on uploads **full observation text and prompts** to a hosted server — "a direct collision with any client-confidentiality requirement" (05:124,183). Telemetry on by default — needs a manual opt-out command or `DO_NOT_TRACK=1` on every machine (05:184). **No promotion or self-modification step at all** — "purely a recall system... claude-mem does not attempt that part" (05:151).
4. **File paths / internals cited**: `src/cli/handlers/file-context.ts` (the corrected file-read-gate behavior), `docs/public/file-read-gate.mdx` (the stale docs page), v12.0.0 changelog. Data model: `observations` table (fields: `id`, `memory_session_id`/`project`, `kind`, `type`, `title`/`subtitle`, `text`/`narrative`, `facts`, `concepts`, `files_read`/`files_modified`, `content_hash`, `created_at`/`created_at_epoch`, `discovery_tokens`, `agent_type`/`agent_id`, `generated_by_model`, `relevance_count`, `metadata`); separate `session_summaries` table (`request`, `investigated`, `learned`, `completed`, `next_steps`, `files_read`/`files_edited`, `notes`). Local per-user **SQLite** database with built-in **FTS5** index plus optional separate **Chroma** vector database (05:131-137).
5. **Adoption status**: PARTIALLY — the graduated cheapest-first retrieval ladder and the quota-aware usage-guard pattern are explicitly cited as worth stealing/studying (ML §6, §7), and its tree-sitter-based structural navigation is called "a lightweight working version of Cody's own 'database spine' idea already shipped and benchmarked" (05:169). But the tool as a whole is not adopted — no promotion/self-improvement mechanism exists in it at all, its docs-vs-code mismatch is used as a cautionary example, and the master list explicitly corrects the File Read Gate's advertised savings downward based on this finding.

---

# Self-improving-loop research (report 06) — mostly research papers/CONCEPT, a few shipping tools

## Reflexion (Shinn et al. 2023, arXiv 2303.11366) — DEEP, RESEARCH ONLY (no repo adopted)

1. GOOD: AlfWorld task completion 72%→97% over 12 retries; HumanEval pass rate 80%→91%; one extra LLM call per failed attempt (06:14, ML §6).
2. BAD: Official GitHub repo (`noahshinn/reflexion`) has sparse commits, ships as example code not an installable library; no mainstream framework has an importable "Reflexion" module — hand-built pattern (06:14).
3. No file paths given beyond the repo name itself.
4. Adoption status: PARTIALLY — the "cheap version" is explicitly adopted: "after any failed test run, have the agent write two or three bullet points into a plain project-level `gotchas.md` before it retries" (06:184) — the *pattern* is adopted, the library/repo is not.

## Self-Refine (Madaan et al. 2023, arXiv 2303.17651) — DEEP, cautionary evidence, not adopted

1. GOOD: +~20pp average on 7 open-ended tasks (dialogue, code readability, constrained writing) (06:17).
2. BAD: Barely moved math reasoning (92.9%→93.1%); authors found 61% of remaining failures were the critique step giving wrong feedback — "the clearest evidence that self-critique without an outside check is unreliable" (06:17). Reinforced by Huang et al.'s negative result (below).
3. Adoption status: Used as evidence AGAINST unguarded self-critique; final design rule "only ever trigger on something outside the agent's own opinion" (ML §"Loop engineering") is directly built on this finding.

## Huang et al., "LLMs Cannot Self-Correct Reasoning Yet" (ICLR 2024, arXiv 2310.01798) — DEEP, the single most-trusted negative result in the whole corpus

1. GOOD (as evidence, not as a tool): With NO external check, GPT-4 self-correction *lowered* GSM8K accuracy 95.5%→89.0%; GPT-3.5 on CommonSenseQA collapsed 75.8%→38.1%. With an external ground-truth signal, the same mechanism reversed to a gain (75.9%→84.3% on GSM8K) (06:99, ML numbers table). Explicitly called "the most trustworthy single result in the research, precisely because it's a paper proving a popular technique doesn't work, not a vendor proving their own product does" (06:99,156, ML).
2. No repo — a research paper, not a tool.
3. Adoption status: **ADOPTED as the governing design rule** — "loop engineering: only ever trigger on something outside the agent's own opinion" and the three permitted lesson-write triggers (test failed / build broke / human corrected) are directly derived from this finding (ML §"Loop engineering").

## ExpeL (Zhao et al. 2023, arXiv 2308.10144) — DEEP, cautionary

1. GOOD: ALFWorld 40%→59%, HotpotQA 28%→39% vs plain ReAct baseline (06:20, ML numbers table).
2. BAD: Authors themselves found feeding raw per-episode reflections sometimes made results worse, "possibly due to reflections sometimes outputting hallucinations" contaminating the more permanent extracted rule (06:20,128 — cited as direct evidence for why a human gate matters before promoting a lesson to permanent).
3. Adoption status: Cited as safety evidence, not adopted as a tool.

## Voyager (Wang et al. 2023, arXiv 2305.16291) — DEEP, the key cautionary precedent for the human-gate design decision

1. GOOD: 3.3x more unique Minecraft items, reached tech-tree milestones 15.3x faster than prior best autonomous agent; reached a "diamond tool" milestone zero baselines reached (06:28, ML numbers table).
2. BAD: **Promotes a skill into its permanent library with zero human review** — a second copy of the same model alone judges "did this work" (06:28,115, ML: "Why the human gate matters, with evidence rather than intuition... the closest real system to 'promote a lesson into a permanent skill'... adds skills... forever with zero human review"). "This is the single clearest cautionary data point for why a human gate matters."
3. Adoption status: NOT adopted as a mechanism — used explicitly as the negative precedent justifying pogan-mem's human-gated promotion rule.

## Devin's "Knowledge" feature (docs.devin.ai) — DEEP, closest positive precedent adopted

1. GOOD: Devin **proposes** a knowledge item from chat feedback; a human reviews/edits/dismisses it; only the approved version becomes a standing instruction — "the closest real product match to Cody's exact design... the strongest existing evidence that 'human gate before a memory becomes an executing skill' is a real, shipped pattern" (06:37,153, ML: "Proposes a new standing rule from something you just corrected, but won't save it until a human approves. Works today").
2. No known BAD noted.
3. Adoption status: **ADOPTED as the governing precedent** for pogan-mem's human-gated promotion design.

## Claude Code's `/run-skill-generator` and `/verify` — see Claude Code entry above (already covered in detail; `/verify`'s disclosed merge-conflict incident is the single most cited "real disclosed production failure + fix" precedent for narrow, failure-only self-editing — ML §6, AUDIT confirms this).

## ACE — Agentic Context Engineering (Stanford/SambaNova, arXiv 2510.04618, ICLR 2026) — DEEP, mechanism adopted, tool not

1. GOOD: Names the failure "context collapse" (full-context rewrite gradually losing detail); fixes it via three roles — Generator/Reflector/Curator, merging only small "delta" edits rather than regenerating (06:45, ML §6). +10.6% average on agent benchmarks, +8.6% on finance; beat GEPA by 12.5 points on AppWorld using 75.1% fewer rollouts and 82.3% less adaptation time; matched IBM's CUGA production agent on average, beat it by 8.4 points on the harder split, using a smaller model (06:45, ML numbers table).
2. BAD: No public code repository found as of the check — CONCEPT as a shipping tool, self-reported by its own late-2025 authors (06:45).
3. Adoption status: **ADOPTED as a mechanism, not as a tool** — "never let one AI call rewrite the lessons file wholesale... the fix is structural: new entries are proposed, reviewed against what actually happened, and merged incrementally" is directly written into the final build order (ML §"Loop engineering": "ACE names the slow death... and the fix is structural"). The cheap version explicitly proposed: "a single markdown 'playbook' file the agent appends short, specific bullet deltas to... never a full rewrite" (06:188).

## Agent Workflow Memory (AWM) (Wang, Mao, Fried, Neubig, CMU, arXiv 2409.07429) — DEEP, mechanism cited approvingly, not committed to build order as a dependency

1. GOOD: WebArena +51.1% relative success vs prior best autonomous method, even beating a version seeded with human-written workflows by 7.9 points; Mind2Web +24.6% relative; advantage **grows** (8.9→14 absolute points) as test tasks diverge from training tasks — "the opposite of the usual worry that a lessons store only helps on near-duplicates" (06:67, ML §6, numbers table).
2. BAD: Real runnable code exists (`github.com/zorazrw/agent-workflow-memory`) but no evidence of production adoption; benchmarks are web-browsing tasks, not coding — "the numbers don't transfer directly, but the idea does" (06:67, ML).
3. Adoption status: PARTIALLY — the concept ("working recipes" as a fourth distinct memory kind alongside facts/decisions/failed-approaches) is explicitly adopted into the final memory-kind taxonomy (ML §"Split memory into distinct kinds": "A fourth kind belongs alongside them: working recipes... The evidence (Agent Workflow Memory, peer-reviewed) shows this kind of memory helps more, not less, as new tasks drift away").

## SWE-agent's "Demonstrations" feature — NAMED

1. GOOD: Human picks a good past trajectory, converts to editable file (`traj-to-demo`), hand-edits, verifies with `run-replay` before reuse — explicitly human-curated and offline (06:70).
2. Adoption status: Not adopted as a tool; cited as one more precedent for human-curated (not automatic) memory promotion.

## mini-swe-agent — NAMED, counter-example

1. GOOD/notable: A ~100-line coding agent scoring >74% on SWE-bench Verified with **no memory, no learning machinery at all** — "proves cross-run learning machinery is not a prerequisite for a strong coding agent" (06:73).
2. Adoption status: Not adopted; cited as a deliberate counter-example/sanity check against over-building memory machinery.

## Generative Agents / "Smallville" (Park et al., Stanford, arXiv 2304.03442) — NAMED

1. GOOD/notable: Scores every observation by recency+importance+relevance, periodically synthesizes higher-level "reflections." Widely reused conceptually elsewhere, though NOT a coding-agent paper (a Sims-style social simulation) (06:76).
2. BAD: Most expensive mechanism in the whole survey — 1 LLM call per observation plus ~3-4 more per reflection (which fires a few times per simulated day); evaluated via human "believability" rating, not an accuracy benchmark (06:76).
3. Adoption status: Not adopted — included only as a conceptually influential but expensive/off-domain reference.

## AutoGen (Microsoft) and CrewAI memory — NAMED

1. GOOD/notable: Both store past text in a vector DB (AutoGen: ChromaDB or Mem0 integration; CrewAI: ChromaDB short-term + SQLite long-term) and retrieve on later runs — "PROVEN as retrieval, CLAIMED as 'learning'" (06:81). CrewAI's own docs say memory "allows agents to learn from previous executions" but "the underlying mechanism is embeddings-plus-SQLite, which is doing less work than that sentence implies" (06:81).
2. Adoption status: Not adopted — cited as an example of retrieval being oversold as "learning."

## DSPy optimizers (Stanford NLP, dspy.ai, arXiv 2310.03714) — DEEP, explicitly REFUSED for pogan-mem

1. GOOD: 36,500+ GitHub stars, 4,600+ commits, reportedly >1M weekly PyPI downloads (includes CI traffic). MIPROv2 optimizer searches instruction phrasing/example selection against a scoring function you define. Reported gains 25%-65% over plain few-shot, 5-46% over human-expert prompts (own paper) (06:59).
2. BAD: A medium optimization run is commonly hundreds of LLM calls — cost gotcha the tool itself warns about (metric-gaming risk, docs recommend a held-out validation set) (06:59).
3. Adoption status: **EXPLICITLY REFUSED** — "Automated prompt optimisers, for day-to-day agency work... an optimiser needs a task with an automatically checkable score, and most of what this system's prompts do has none" (ML "What I would refuse to build").

## GEPA (Agrawal, Potts et al., Stanford, arXiv 2507.19457, ICLR 2026 Oral) — DEEP, mechanism/insight adopted, tool itself refused for day-to-day use

1. GOOD: Installable today (`pip install gepa`, github.com/gepa-ai/gepa, ~5,900 stars). Keeps a Pareto frontier of several prompt versions rather than one winner. Beats RL fine-tuning (GRPO) by 10% average, up to 20%, using up to 35x fewer rollouts; beats MIPROv2 by >10% (06:62, ML §10). **Explicitly provider-agnostic and documented to run against local models at near-zero marginal cost — corrects a widely-repeated assumption that it necessarily needs a paid API** (ML §10: "this is not an automatic conflict with the no-paid-keys rule... its own documentation — the project README, not the peer-reviewed paper — states it runs against local models at near-zero marginal cost"). Core insight — "diagnoses failures from the actual error traces and logs, not from a summary of them" — explicitly adopted as a write-path mechanic (ML §"Loop engineering": "Attach the raw failing output to the lesson... GEPA's central finding is that diagnosis from real execution traces beats diagnosis from summaries").
2. BAD: Still needs a task with an automatically checkable score to optimize against, which most day-to-day agency work doesn't have — this, not billing, is the real reason it's refused for general use (ML §10).
3. Adoption status: PARTIALLY — the "diagnose from raw traces, not summaries" principle is directly adopted into the build; the tool itself is refused for day-to-day prompt optimization (same refuse-list entry as DSPy) because of the checkable-score requirement, not cost.

## Anthropic's "context editing" (Claude API, beta) — NAMED, DEEP-ish

1. GOOD: Clears old tool-result content once a token threshold is crossed (100,000 tokens default, keeps 3 most recent tool uses) (06:51).
2. BAD: 84% token savings / 39% performance improvement figure could not be directly re-confirmed on the live docs page during the research pass — CLAIMED pending re-check (06:51).
3. Adoption status: Not adopted as infrastructure (a Claude API beta feature, not a Claude Code subscription feature) — flagged as a paid-API-key dependency to avoid, per the "no paid API keys" constraint note at the top of report 06.

## Darwin Gödel Machine and SICA — NAMED (self-modifying agents, evidence only)

1. GOOD: Both score every candidate change against an **external mechanical benchmark**, never the model's own opinion — Darwin Gödel Machine 20.0%→50.0%, SICA 17%→53% on coding benchmarks (both authors' own numbers, not independently rerun) (ML numbers table, "Stop repeating the same mistake" §6).
2. Adoption status: Cited as confirming evidence for the "external signal only" design rule; not adopted as tools.

---

# Context/harness engineering sources (report 07) — mostly practitioner blog posts and Anthropic's own published guidance, not third-party repos to adopt/reject in the usual sense

## Chroma "Context Rot" research (trychroma.com/research/context-rot) — DEEP, foundational evidence, heavily cited

1. GOOD (as evidence): Independent (Chroma sells a vector DB, so has a mild incentive to make long-context look bad — flagged as a light bias note) — tested 18 frontier models across ~194,480 calls; every model degraded as input length grew, well inside advertised context windows, on both a synthetic "repeat this word" task and a realistic conversational-QA benchmark (LongMemEval) (07:13-14, ML numbers table). This is repeatedly cited as **the single most important justification for not dumping the whole memory store into every session** (07:85, ML §7).
2. Adoption status: **ADOPTED as the foundational justification** for pogan-mem's just-in-time/progressive-disclosure retrieval design rather than always-loaded context.

## Anthropic engineering blog, "Effective context engineering for AI agents" (Sept 29, 2025) — DEEP, foundational

1. GOOD: Canonical framing — "attention budget" not token-count budget; just-in-time retrieval (pointer, not content) vs pre-loading; progressive disclosure; sub-agent isolation numbers (subagent summaries ~1,000-2,000 tokens; single agent ~4x tokens of a plain chat turn, multi-agent ~15x; 90.2% gain on an internal S&P-500-board-members eval, explicitly flagged as self-reported/not independently reproduced) (07:7-9,21-24,43-46).
2. BAD: Anthropic's own explicit warning that most coding work is a poor fit for multi-agent fan-out — "current models are 'not yet great at coordinating and delegating to other agents in real time'" (07:46, ML §7).
3. Adoption status: **ADOPTED** — directly shapes pogan-mem's retrieval-on-demand design and the caution against subagent fan-out for memory search as a default (ML §7).

## Liu et al., "Lost in the Middle" (arXiv 2307.03172, TACL 2024) — DEEP, foundational, peer-reviewed

1. GOOD: Established the "lost in the middle" effect — information at start/end of context read far more reliably than identical info in the middle. Peer-reviewed, "one of the most-cited and most-reproduced findings in this space" (07:15).
2. Adoption status: Cited as supporting evidence, not a tool.

## Manus engineering blog, "Context Engineering for AI Agents" (Yichao "Peak" Ji, 2025) — NAMED, DEEP-ish

1. GOOD: Token-level tool masking during generation instead of editing the tool list in prompt text, to protect a stable cached prefix — "described as a production technique in a shipping agent product, not a proposal" (07:24). Independently corroborates the prompt-cache-prefix-stability point ("keep your prompt prefix stable... even a single-token difference can invalidate the cache") (07:53).
2. Adoption status: The prefix-stability lesson is folded into the design rationale for append-only, stable-prefix memory files (07:53, ML §7).

## thedotmack/claude-mem — see full entry above under "ECC and claude-mem"; also referenced in report 07 for its progressive-disclosure/compressed-summary token-savings claim (~2,250 tokens saved per session start vs a naive MCP approach — CLAIMED, project's own figure) (07:31,79).

## Addy Osmani ("Agent Harness Engineering," personal blog) and Martin Fowler's site ("Harness engineering for coding agent users") — DEEP, foundational for harness-engineering framing

1. GOOD: Converging independent definition — "an agent equals the model plus the harness." Blocks destructive commands before they run (matches Claude Code's own permission/deny-rule system). "Success is silent, failures are verbose" — feed failing checks back automatically. Fowler's feedforward/feedback split — a harness with only one "keeps repeating the same mistakes" (feedback-only) or "encodes rules but never finds out whether they worked" (feedforward-only) (07:57-60). Recommends keeping AGENTS.md/CLAUDE.md under ~60 lines, each rule traced to a real past failure (07:61, table §9). Real number (with caveat): Terminal-Bench 2.0 results showing the same model (Opus 4.6) moving from ~30th percentile to top-5 purely from harness changes, no model change — CLAIMED, not independently verified against the leaderboard (07:63).
2. Adoption status: **ADOPTED as framing** for the harness-engineering discipline generally; the "advisory vs enforced" and "narrow, failure-triggered gate" design principles trace back to this framing plus Claude Code's own precedent.

## OpenHands' "condenser" — DEEP, mechanism explicitly cited as the strongest shipped answer found

1. GOOD: Summarizes only the older half of a long event log when a budget is about to be exceeded, leaving the recent half word-for-word — fires on both a soft trigger (approaching budget) and a hard trigger (actual context-window error). Detects when a summary would break the transcript's expected structure (e.g. orphaning an unanswered tool call) and skips a cycle rather than corrupt the run (ML §7: "the most concrete shipped answer found anywhere to 'how does a long autonomous run stay coherent'").
2. Adoption status: PARTIALLY — cited as the best real precedent for long-run context management; not explicitly committed to the build order as a dependency, but the design principle (summarize gradually, not cliff-edge) is echoed.

## txtai — NAMED (master list only)

1. GOOD: A single embedded Python package running keyword + vector + graph retrieval fully offline, no server, no key — "independent confirmation that the fused shape simply does not require heavyweight infrastructure" (ML §"Storage").
2. Adoption status: Cited as an existence proof for the storage design, not adopted as a dependency.

---

# Graph storage engines (report 08) — evaluated as candidate database engines, none "adopted" as the final pick (SQLite chosen instead)

## Kuzu (kuzudb/kuzu) and forks (Vela-Engineering/kuzu "Vela fork", Kineviz "bighorn") — DEEP

1. GOOD: No server process — links as a library like SQLite (08:9). Uses Cypher. Vela fork adds concurrent multi-writer support the original lacked, release dated March 2026 (08:13). Reported (Kuzu-affiliated study, not independent): 374x faster than Neo4j on a 100K-node/2.4M-edge multi-hop benchmark (0.009s vs 3.22s) (08:14).
2. BAD: **Company (Kùzu Inc.) archived the GitHub repo October 10, 2025**, "Kuzu is working on something new," final release v0.11.3 — verified directly against the live archived repo banner (08:11). Reporting (The Register, BigGo) ties the shutdown to an Apple acquisition Oct 9, 2025, surfaced only via a Feb 2026 EU Digital Markets Act filing — explicitly CLAIMED/journalism, not confirmed by either company (08:12, ML §"Storage" carries this hedge correctly per AUDIT m2/FIXVER item 12). "Picking Kuzu today... means picking one specific community fork and accepting that fork's maintainers as your dependency" (08:15).
3. File paths: N/A (a database engine, not application code with internal file paths cited).
4. Adoption status: **REJECTED as the storage engine** — "the obvious pick for a graph... was archived in October 2025... the risk it illustrates is the one to avoid: your foundation going read-only because of something that happens to somebody else" (ML §"Storage"). SQLite chosen instead.

## SQLite + sqlite-vec + FTS5 — DEEP, **ADOPTED as the final storage choice**

1. GOOD: Not a graph DB natively — nodes/edges as tables plus recursive CTEs. `sqlite-vec` adds vector similarity search (actively maintained, portable to WASM/mobile/Raspberry Pi, SIMD-accelerated) (08:23). FTS5 built-in keyword search (08:24). Single-digit-ms traversal at 2-3 hops with proper indexing (practitioner-reported, not independent benchmark) (08:22). Git-shareable with `textconv`/`gitsqlite`/`git-sqlite` tooling for readable diffs (08:26).
2. BAD: No purpose-built graph query language — hand-written recursive SQL, no built-in cycle protection, traversal performance degrades faster as hop count grows vs a purpose-built graph engine (08:20-21).
3. File paths: N/A (a database engine).
4. Adoption status: **ADOPTED as the final storage engine** — "the safer starting move is the boring one: SQLite... because it has no company that can disappear, is already a dependency of half the Python/Node ecosystem" (08:115, ML §"Storage": "SQLite gives you all three ways of finding things with no server at all").

## DuckDB + DuckPGQ — NAMED, DEEP-ish, not adopted

1. GOOD: SQL/PGQ graph-pattern syntax, bidirectional BFS via DuckDB's parallel vectorized engine (08:31).
2. BAD: DuckDB's own docs call DuckPGQ "still under active development" with "some features incomplete." HNSW vector index persistence to disk requires an experimental flag; DuckDB's own docs warn a crash with uncommitted changes while that flag is on "can end up with data loss or corruption of the index" and "we still recommend that you do not use this feature in production environments" (quoted directly, 08:32).
3. Adoption status: Not adopted — noted as a good fit only if already using DuckDB for analytics elsewhere.

## Neo4j Community Edition — NAMED, DEEP-ish, not adopted

1. GOOD: Reference Cypher implementation. GPLv3, genuinely free, no node/size cap on Community Edition itself (a commonly-repeated "100K nodes" limit is actually an AuraDB Free cloud-tier limit, not a Community Edition limit — corrected via Neo4j's own community forum) (08:38).
2. BAD: No clustering, single database only, cold backup only, no RBAC, slower "Slotted" runtime vs Enterprise's "Pipelined" runtime (~2x per Neo4j's own internal benchmarks) (08:38). Runs as a background JVM server process even in "local" use; store is a directory, not diff-friendly for git (08:40,42).
3. Adoption status: Not adopted.

## Memgraph — NAMED, not adopted

1. GOOD: Open-source Community Edition "free forever," in-memory, C++, Cypher-compatible, ACID; bundled MAGE library of 40+ graph algorithms (PageRank, community detection, link prediction) (08:46,48).
2. BAD: In-memory — practical size ceiling is RAM, not disk. Runs as a local server process, not embedded. "Up to 120x faster than Neo4j" is CLAIMED, not independently verified (08:47,50).
3. Adoption status: Not adopted.

## FalkorDB — NAMED, not adopted

1. GOOD: Redis-module graph DB using GraphBLAS sparse-matrix math instead of pointer-chasing — genuinely different engineering approach. Reported 36ms p50 vs 469ms for a comparable workload, 10-100x speedups claimed (company's own benchmark) (08:54,56).
2. BAD: SSPL v1 license — not OSI-approved, restricts offering the database itself as a hosted service to third parties (fine for local/internal use, relevant only if pogan-mem is ever resold hosted) (08:57). Requires Redis running — a background server process.
3. Adoption status: Not adopted.

## oxigraph (RDF/SPARQL) — NAMED, not adopted

1. GOOD: Rust implementation of RDF/SPARQL, built on RocksDB, genuinely embeddable, Python/JS/WASM bindings, actively maintained (updated July 26, 2026, directly checked) (08:62-63).
2. BAD: RDF's flat-triple model is heavier/more academic than a property graph — "more ceremony than pogan-mem needs." No native vector or full-text search bundled (08:64-65).
3. Adoption status: Not adopted.

## NetworkX — NAMED, not adopted as the persistent store

1. GOOD: Pure-Python in-memory graph library, well known, fine for prototyping/one-off analysis scripts.
2. BAD: ~55GB RAM for a 10M-node/100M-edge graph vs ~1.3GB for a purpose-built library (SNAP) doing the same — ~50x more memory per edge, since it stores everything as nested Python dicts (08:70). No durability, no query language, no concurrency, no git-shareability (08:72).
3. Adoption status: Not adopted as the store; useful only for occasional algorithm runs over data pulled from the real store.

## Cozo (CozoDB) — NAMED, not adopted

1. GOOD: Combines Datalog graph queries with built-in vector search (HNSW) and full-text search in one embedded engine — "explicitly positioned by its own creators as 'the hippocampus for AI'" (08:76).
2. BAD: Main source repo has had no code updates since 2024, though the issue tracker shows activity into 2025-2026 — "similar risk category to Kuzu" (08:77-78).
3. Adoption status: Not adopted — flagged as worth a hands-on trial but not a hard dependency, given maintenance risk.

## SurrealDB — NAMED, not adopted

1. GOOD: Multi-model (documents/graph/vectors/full-text/time-series) in one engine, can run fully embedded with RocksDB backend, no separate server required. BM25 keyword + cosine-similarity vector search designed to be blended in one query (08:82,84).
2. BAD: Business Source License 1.1 (BSL) — free for own use until a Jan 1, 2030 change date (then converts to Apache 2.0); restriction is only on offering SurrealDB itself as a competing hosted database service (08:85).
3. Adoption status: Not adopted.

## TypeDB — NAMED, not adopted

1. GOOD: MPL-2.0, genuinely open-source; full Java→Rust rewrite completed v3.0 Dec 2024, latest stable 3.7.2 shipped Dec 13, 2025 (08:89-90).
2. BAD: "The most conceptually different tool on this list" — a bespoke type system and query language (TypeQL), real learning-curve cost against a benefit (rule-based inference) that overlaps with what simpler engines plus app logic already give (08:91).
3. Adoption status: Not adopted.

## libSQL / Turso — NAMED, not adopted (but relevant to a future sync problem)

1. GOOD: SQLite-API-compatible fork adding native replication and "embedded replicas" (a local copy synced with a remote source, reads always local/fast even if writes eventually reach a shared server) (08:95-96).
2. Adoption status: Not adopted for the core graph decision, but explicitly "worth keeping in the back pocket for the cross-machine sync problem" if a live shared-replica need arises later (08:96).

---

## Miscellaneous single-paper citations (research-only, no shipping product, included for completeness since they were studied in depth)

- **"Reliable Graph-RAG for Codebases" (arXiv 2601.08773, Jan 2026)** — DEEP. The parser-vs-LLM-extraction cost/accuracy study on 3 real repos (Shopizer etc.) discussed extensively above under codebase-map tools; single-author preprint, no institution, 15 self-graded questions per repo, author-written baseline — figures corrected multiple times across ML/AUDIT/FIXVER (08:212, AUDIT C1, FIXVER item 1).
- **"Codebase-Memory" paper (arXiv 2603.27277, March 2026)** — see `codebase-memory-mcp` / `DeusData/codebase-memory-mcp` entry above; this IS that tool's own paper (FIXVER item 2 confirms authorship link).
- **GraphRepo** (Neo4j-based, mines git history + code structure) — NAMED, cross-check data point only: 52,897 nodes/127,837 edges for Hadoop, 74,856/213,368 for TensorFlow — independent confirmation of node-count order of magnitude, not adopted as a tool (08:230).
- **"Reliable Graph-RAG" companion "Repository Intelligence Graph" (arXiv 2601.10112, Jan 2026)** — DEEP. Build-and-test-only graph (not full code structure), averaged 20,692 bytes (~5,200 tokens) JSON, up to 60,076 bytes (~15,000 tokens) for the most complex repo; found 12.2% *relative* accuracy improvement and 53.9% task-time reduction across Claude Code/Cursor/Codex when injected into context (08:232). **Independence later found compromised**: its evaluation set includes MetaFFI, a project by the same two authors as the paper itself (FIXVER item 3, confirmed via Crossref) — corrects AUDIT M9's softer "may be the author's own" framing to a confirmed fact.
- **RAGSearch / "Do We Still Need GraphRAG?" (arXiv 2604.09666)** — DEEP as corroborating evidence for HippoRAG 2 (see that entry); also found graph-based retrieval has both higher recall (80-83% vs 76-79%) AND lower run-to-run variance (±0.18-0.36 vs ±0.61-1.03) than dense vector retrieval (01:128-129, ML §4).
- **Wolff & Bennati (KTH Royal Institute of Technology)** — DEEP, independent, no vendor affiliation, open-source code — Graphiti vs Mem0 cost/accuracy study; heavily revised across versions (v1: no significant accuracy difference; v4: Graphiti 56.03% vs Mem0 81.08%, a 25-point loss at p=0.002, "retrieval incompleteness") (01:147-148, AUDIT C3, FIXVER item 4 — the "current version is MORE damning of graph memory than the version originally quoted").
- **Penfield Labs LoCoMo benchmark audit** — DEEP, independent audit of the underlying LoCoMo benchmark itself (not any vendor's use of it): 99 score-corrupting errors across 1,540 questions (6.4% error rate), LLM-judge grading accepted 62.81% of deliberately wrong answers as correct (01:144-145, ML §9 — "arguably the single most important finding for interpreting every other number in this document").
- **Han et al. (arXiv 2502.11371), "When to use Graphs in RAG" (GraphRAG-Bench, arXiv 2506.05690/2506.02404)** — independent, ground-truth-graded; found Microsoft GraphRAG 13.4% lower accuracy than vanilla vector RAG on Natural Questions in the original relay, later found by FIXVER/ML to be **a misattribution — the cited paper never ran that benchmark** (01:23,171; ML §4 self-corrects: "a '13.4% worse' figure is a misattribution — the paper it cites never ran that benchmark"). Han et al. is also the source of the HippoRAG-2-loses-on-single-hop figure (61.03 vs 64.78).

---

## Notes on scope decisions made while extracting

- Repos that were only named once in passing with zero specific claims (e.g. brief mentions of "Kimi," "OpenClaw," "Zilliz Cloud," "Chroma"/"Milvus" purely as generic vector-DB infrastructure with no memory-specific logic of their own) are **excluded** from the per-repo breakdown above per the task's instruction to distinguish depth-of-study; Chroma and Milvus are explicitly noted in 03 as "not a memory product, just the database underneath one... has no extraction/fact logic of its own" (03: table rows Chroma, Milvus) and are not treated as memory-system case studies.
- No file paths were given anywhere in the source documents for: Zep/Graphiti, Mem0 (repo-internal paths), Memobase, CaviraOSS/OpenMemory, Redis Agent Memory Server (except the `V0/README.md` deprecation notice), Cognee, Cloudflare Agent Memory, most graph-database engines (they are evaluated as engines/products, not read as application source), and most research papers (Reflexion, Self-Refine, ExpeL, Voyager, AWM, ACE, GEPA, DSPy — paper-level citations, not repo file reads). Where the documents explicitly said "no file paths given" or gave none, this is noted per-entry above rather than invented.
- File paths ARE given (and quoted above) for: codebase-memory-mcp/DeusData (`src/cli/hook_augment.c`, `src/mcp/mcp.c`, `.codebase-memory/graph.db.zst`, `docs/BENCHMARK.md`), Aider (`repomap.py`), Serena (`agent.py`, function `_send_usage_info`), Graphify (`graphify-out/graph.json`, `--code-only` flag), codegraph (`.codegraph/codegraph.db`), ECC (extensive list, see ECC entry), claude-mem (extensive list, see claude-mem entry), Letta (`letta/functions/function_sets/base.py`, `tests/integration_test_sleeptime_agent.py`), Microsoft GraphRAG (`factory.py`, `query/structured_search/`), Cline (six named markdown filenames), Anthropic's memory tool (six named commands + `/memories/` path prefix), Claude Code (extensive: `CLAUDE.md`, `CLAUDE.local.md`, `~/.claude/projects/<project>/memory/`, `MEMORY.md`, `.claude/rules/`, hook event names, etc.).
