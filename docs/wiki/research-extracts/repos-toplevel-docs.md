---
title: Research extract — repos studied in the old repo's top-level docs
type: wiki
status: research-fact
created: 2026-08-21
updated: 2026-08-21
sources: [../pogan-toolkit/docs/ai-agent-memory-unified-report.md, ../pogan-toolkit/docs/ai-memory-and-context-ecosystem-gemini.html, ../pogan-toolkit/docs/reports/2026-08-07-compare-*.md, read 2026-08-21]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions. The per-repo mechanisms, weaknesses, and file paths are external facts (file paths re-verified live in `../seed-repos/` for the seed repos only); every "adoption status" / "verdict" line is the OLD project's history and binds nothing here.

# External repos studied in pogan-toolkit top-level docs

Scope read: `docs/ai-agent-memory-unified-report.md` (1057 lines, full), `docs/ai-memory-and-context-ecosystem-gemini.html` (grepped + read relevant sections), `docs/wiki/knowledge-retrieval-methods.md` (full — this is a vendor-neutral field guide, not a repo study; it names methods, not specific external repos being adopted/rejected, so it contributes no repo entries below), `docs/reports/2026-08-07-compare-babysitter-research.md` (full), `docs/reports/2026-08-07-compare-supermemory.md` (full). Also checked `docs/superpowers/plans/2026-08-02-pogan-mem-00-roadmap.md` and `docs/superpowers/spec/00-pogan-mem-spec.md` for external-repo mentions per instructions — **neither file mentions any external open-source repo**; the only `grep` hit in the spec is an unrelated git-identity-mixup incident note (line 170) with no bearing on this task.

**No "SUPPLIED BY THE USER" statement found anywhere in scope.** I grepped for "suppl-", "Logan gave/handed/provided/found", "seed repo", "starting point" across the unified report and the HTML file. The only "supplied" hit is in the spec file and is about git identity env vars, unrelated to repo sourcing. The closest thing to a sourcing note is that **ECC's adoption path was Logan's own decision** (D5, `docs/ai-agent-memory-unified-report.md:55-61`: "Sourcing (Logan's decision): we do not publicly fork ECC... Instead we clone ECC as a private downstream copy"), but this is Logan *deciding how to adopt* a tool the research found, not Logan *supplying* the repo to the research agents at project start. No document frames any repo as having been handed to the researchers as a starting point.

---

## 1. ECC (Continuous Learning v2 / "Every Commit Counts"?)

1. **Repo:** `github.com/affaan-m/ECC` (MIT license), as given at `ai-agent-memory-unified-report.md:197`, `:983`, `:987`, `:989`, and HTML `ecc-eco` card.
2. **GOOD (praised / borrowed):**
   - "Capture + distillation + instinct + harness base: deterministic secret-scrubbing hooks capture tool events; an opt-in Haiku 'observer' distills them into markdown/YAML 'instinct' files; `/evolve` turns instincts into skills." (`:197`)
   - Called "an open-source tool that already does a lot of the note-capturing" (`:876`, plain-language section).
   - "The survey gets this one wrong" — the docs correct an outside survey that called ECC "purely ephemeral": ECC "writes durable instinct files with confidence scores and a promotion path into skills via `/evolve`. Its memory persists; what it lacks is a query path." (HTML `ecc-eco` card, line 576)
3. **BAD / weak / rejected:**
   - **Supply-chain risk** — "a dominant single maintainer, an unsigned, unverified update path, machine-wide hooks and transcript access together form the xz-utils precondition profile." A malware clone impersonating the repo has been documented in the wild. (`:367`, `:895`)
   - `scripts/auto-update.js` is manually invoked (`git fetch` + `git pull --ff-only`) but "carries no signature, checksum, GPG or Sigstore verification of the pulled content — its only integrity check is that `package.json`'s name matches the official package." (`:59`)
   - **Set-once, non-decaying confidence** — "no code path anywhere updates an existing instinct's confidence — no decay function, no reinforcement." Meanwhile its own `SKILL.md` "explicitly documents confidence increasing on repeated observation and decreasing on correction or staleness — behaviour that does not exist." (`:312`, `:959`)
   - "ECC's session-start dump is not query-time retrieval at all — it is confidence-ranked selection... the field guide's named 'never-decaying self-score' failure mode." (`:216`)
   - ECC's hooks are global once enabled — fire in every session on the machine unless scoped. (`:311`)
   - Naming collision: ECC's `governance-capture` hook is actually a security logger, not related to pogan-mem's governance layer. (`:314`, `:983`)
   - Its retrieval method is rejected as a design pattern for pogan-mem's own provenance field: "D4 rejects that same self-scored-confidence pattern for provenance." (`:216`)
4. **File paths / module names cited inside ECC:**
   - `observations.jsonl` — deterministic capture hooks write here (`:983`)
   - instinct `.md`/`.yaml` files under `${XDG_DATA_HOME}/ecc-homunculus` (`:983`)
   - `/evolve`, `/instinct-*`, `/promote` — its slash commands (`:983`)
   - `skills/continuous-learning-v2/scripts/instinct-cli.py` — confidence set once, never updated; specifically `inst.get('confidence', 0.5)` (`:312`, `:989`)
   - `skills/continuous-learning-v2/SKILL.md` — documents (falsely) reinforcement/decay behavior (`:989`)
   - `scripts/auto-update.js` (`:59`, `:989`)
   - `ECC_HOOK_PROFILE`, `ECC_DISABLED_HOOKS` — env vars to scope hooks (`:311`, `:983`)
   - `ECC_INSTINCT_CONFIDENCE_THRESHOLD` (default 0.7) — injection threshold env var (`:312`)
   - Instinct frontmatter keys: `id`, `trigger`, `confidence`, `domain`, `source`, `scope`, `project_id`, `project_name` (`:284`, `:983`)
   - SessionStart injection capped ~8,000 characters (`:983`, `:310`)
5. **Adoption status:** **ADOPTED** — "Adopt" in the ADOPT/BUILD/STUDY table (`:197`): "Private downstream clone + own plugin, pinned/hash-verified, no auto-merge; sandbox-evaluated in Phase 0 (D5)." Sourcing decided by Logan (private clone, not a public fork, not a plugin-overlay). Note the walking skeleton (first build increment) explicitly contains **no ECC** — adoption depth is still gated by a Phase-0 sandboxed evaluation (`:61`, `:81`, `:876`).

---

## 2. codebase-memory-mcp

1. **Repo:** `github.com/DeusData/codebase-memory-mcp` (MIT license) — `ai-agent-memory-unified-report.md:198`, `:984`, `:987`.
2. **GOOD:**
   - "tree-sitter/LSP code graph; local semantic + graph + regex search, no API key." (`:198`)
   - "Real strengths, credited plainly: a zero-runtime static binary, a knowledge-graph artifact you can commit alongside source, and a genuinely local-first design with no API keys. On a very large codebase, an indexed graph earns its keep." (`:666`)
   - It is "the only one of the four compared [code-tool] systems with a graph," making it the right shape for a durable cross-rename anchor. (`:723`)
3. **BAD / weak / rejected:**
   - **Pre-1.0 maturity** — "still 0.x with a sizable open-issue count. Expect churn and breaking changes." (`:670`)
   - **Hype vs. verified** — "very fast star growth for a young repo and marketing-flavoured performance claims. Treat '99% fewer tokens' as a claim to confirm, not a fact." (`:671`)
   - **Install & scoping pattern** — "an installer that downloads a prebuilt native binary and auto-writes global agent config across many tools. That is a supply-chain surface and a machine-wide change." (`:672`) — explicitly likened to the xz-utils precondition profile D5 already applies to ECC (`:708`).
   - **Index staleness** — "a stored index drifts from the working tree as you edit; 'the graph said X' isn't always today's truth." (`:673`)
   - "'Project-level' is partial" — binary and index cache are machine-global, not a self-contained project dependency. (`:674`)
   - Its `adr_hint` "nudges the agent to dump rationale into its own scratchpad blob. House rule: ignore it; all rationale routes through `/log`." (`:315`, `:984`)
   - Its internal hook self-aborts silently: `CBM_HOOK_DEADLINE_MS` (default 2000ms) — "self-aborts and returns empty with no error anywhere — genuinely silent." (`:317`, `:984`)
4. **File paths / module names cited:**
   - 14 MCP tools (index / search / trace / query / architecture) (`:984`)
   - local `nomic-embed-code` embeddings model (`:984`, `:214`)
   - a PreToolUse hook injects code-map hits on Grep/Glob (`:984`)
   - a committable `.codebase-memory/graph.db.zst` snapshot (`:984`)
   - `CBM_HOOK_DEADLINE_MS` env var (`:984`, `:317`)
5. **Adoption status:** **PARTIALLY ADOPTED, then SHELVED for V1.** Listed as "Adopt (shelved)" in the ADOPT/BUILD/STUDY table (`:198`): "Cut from V1; re-adopt at Phase 2+ as the structural anchor for cross-rename `related:` edges." Note: at time of the 2026-07-30 ecosystem pass, the docs flag (OQ-27) that a *different* code-graph tool (codegraph, see below) is already running on the machine, undermining the "shelved until needed" framing — "the trigger fires... it is deployed and unaudited." (`:850`, `:806`)

---

## 3. claude-mem

1. **Repo:** `github.com/thedotmack/claude-mem` (Apache-2.0) — `:201`, `:985`, `:987`.
2. **GOOD (borrowed as design reference):**
   - "Mature auto-recall engine." "Steal its retrieval design (progressive disclosure, File Read Gate, content-hash dedup)." (`:201`)
   - Its progressive-disclosure ladder — `search` / `timeline` / `get_observations` tools — is explicitly the model for pogan-mem's own `memory_search` / `memory_timeline` / `memory_get` design (`:226-230`, `:985`).
   - The File Read Gate (`docs/public/file-read-gate.mdx`) is cited as the design source for pogan-mem's own "File Read Gate" hook (`:985`).
3. **BAD / weak / rejected:**
   - "Its Chroma subprocess is still load-bearing, and issue #3218 — orphaned `chroma-mcp` processes OOMing an 8 GB machine in about 20 minutes — is still open on the 13.12.x line." (HTML `claude-mem` card; `:201`)
   - Not zero-key: "No — Chroma subprocess architecture" (`:751`)
   - **How it actually fuses its two retrievers was long unconfirmed and turned out to be worse than expected**: reading `HybridSearchStrategy.ts`'s `intersectWithRanking()` and `SearchOrchestrator.ts` shows it does neither RRF nor separate disclosure layers, but an **intersection with vector-preserved ordering** — "it takes Chroma's vector-ranked IDs, keeps only those that also appear in the SQLite/keyword result set... An intersection is recall-destroying by construction — a record that only one retriever finds is discarded." (`:218`, `:988`) "Its own published architecture doc says only 'hybrid retrieval' and never describes this." (`:218`)
   - "the system we borrowed the progressive-disclosure ladder from does the fusion step worse than the RRF we already pre-committed to under OQ-25. That strengthens the OQ-25 disposition rather than changing it: do not copy claude-mem here." (`:218`)
   - Its "background compression also leans on the restricted subscription-billing path." (`:203`)
   - "Running claude-mem and ECC's observer means two hook-heavy systems both capturing sessions, both calling a background Haiku model, and both injecting at session start — redundant." (`:203`)
   - Frequently emits out-of-enum `type` values silently defaulted ("most of the store read as one type") — cited as the general gotcha "Don't trust an LLM-filled enum." (`:313`)
4. **File paths / module names cited:**
   - `src/services/worker/search/strategies/HybridSearchStrategy.ts` (function `intersectWithRanking()`) (`:218`, `:988`)
   - `src/services/worker/search/SearchOrchestrator.ts` (`:218`, `:988`)
   - `docs/public/file-read-gate.mdx` (`:985`)
   - Issues **#3181** (observation types mis-tagged as `bugfix`, closed) and **#3218** (chroma-mcp orphan leak, OOM, still open on 13.12.x) (`:201`, `:988`)
5. **Adoption status:** **STUDY ONLY / REJECTED to run.** "We do not run it live." (`:201`) OQ-1 disposition: "DECIDED — never." (`:824`) Explicitly "Study only, never run — unchanged." (`:751`)

---

## 4. Supermemory

1. **Repo:** `github.com/supermemoryai/supermemory` (MIT) — `:987`, and separately the subject of `docs/reports/2026-08-07-compare-supermemory.md` (its own dedicated comparison report).
2. **GOOD:**
   - From the unified report: "It clears our cost constraint — `npx supermemory local` runs single-process on one machine; the default embedding model `Xenova/bge-base-en-v1.5` runs on-device with no key; the docs explicitly support running end-to-end offline against Ollama, LM Studio or vLLM." (`:790`, HTML)
   - From `compare-supermemory.md`: "SMFS's core insight — agents already know `ls`, `cat`, `grep`, so make memory look like files" (comparison report, not this file — see `docs/reports/2026-08-07-compare-supermemory.md:9`, quoting docs page `apps/docs/concepts/how-it-works.mdx`).
   - `MemoryEntrySchema` described as "genuinely sophisticated": versioning fields (`version`, `isLatest`, `parentMemoryId`, `rootMemoryId`), typed relations (`updates | extends | derives`), provenance joins, lifecycle flags `isInference`/`isForgotten`/`isStatic`/`forgetAfter`/`forgetReason` (compare-supermemory.md:15).
   - Broadest agent-integration surface of any product surveyed: REST API (32 paths/41 ops), SDKs, hosted OAuth MCP server, the "Memory Router" (URL-prefix proxy), browser extension, ~25 framework integrations (compare-supermemory.md:39).
3. **BAD / weak / rejected:**
   - "supermemory's differentiating engine is closed. The MIT-licensed monorepo ... contains zero database schema, zero migration files, and zero extraction/search code (verified: no `CREATE TABLE`, `pgTable`, or `sqliteTable` anywhere in the clone)." (compare-supermemory.md:5)
   - No notion of human review — "a memory written for a shared space is live the moment it lands." (compare-supermemory.md:27)
   - Automatic contradiction resolution and auto-applied graph links (`isLatest` deciding which fact is current) rejected by pogan-mem as "the model silently rewrite the record" behavior it forbids by design (compare-babysitter-research.md:32; compare-supermemory.md:17).
   - Automatic decay/forgetting rejected outright (compare-babysitter-research.md:33).
   - "Telemetry is real and pervasive in the open code: PostHog in the web app, per-tool-call PostHog analytics in the MCP server (`apps/mcp/src/server/analytics.ts`), Sentry sourcemaps." (compare-supermemory.md:33)
   - CI "runs typecheck and lint only — it does not run the test suites (`.github/workflows/ci.yml`, verified)." (compare-supermemory.md:45)
   - "self-hosted" is a trap on the pricing page — free single-user OSS mode vs. paid Enterprise ($399/mo Scale tier); the free server is "still 0.0.x." (`:790`, HTML)
4. **File paths / module names cited (mostly from compare-supermemory.md, since that report goes deepest):**
   - `packages/validation/schemas.ts` (comment "bytea in DB"; `DocumentSchema`, `MemoryEntrySchema`, `MemoryDocumentSourceSchema`, `ConnectionSchema`, `OrganizationSettingsSchema`, `SpacesToMembersSchema`) — compare-supermemory.md:9,15,27,33
   - `apps/mcp/wrangler.jsonc` (Cloudflare Workers + Durable Objects config) — compare-supermemory.md:9
   - `apps/docs/concepts/how-it-works.mdx` (docs: "Temporal Vector-graph engine") — compare-supermemory.md:9
   - `apps/docs/self-hosting/configuration.mdx` — compare-supermemory.md:9
   - `apps/docs/connectors/` — compare-supermemory.md:9
   - `apps/docs/concepts/graph-memory.mdx` ("dreaming" background extraction) — compare-supermemory.md:15
   - `packages/tools/src/conversations-client.ts` (smart diffing on re-ingest) — compare-supermemory.md:15
   - `packages/validation/api.ts` (search endpoint params) — compare-supermemory.md:21
   - `apps/docs/concepts/user-profiles.mdx` (precomputed profile retrieval) — compare-supermemory.md:21
   - `apps/docs/self-hosting/local-vs-enterprise.mdx` — compare-supermemory.md:27
   - `apps/docs/overview/security.mdx` (SOC 2/GDPR/HIPAA claims) — compare-supermemory.md:33
   - `apps/mcp/src/server/analytics.ts` (PostHog per-tool-call telemetry) — compare-supermemory.md:33
   - `apps/mcp/src/server/tools/` (~15 MCP tools) — compare-supermemory.md:39
   - `packages/tools/src/openai/middleware.ts` (`withSupermemory` client wrapper) — compare-supermemory.md:39
   - `apps/browser-extension/entrypoints/content/` — compare-supermemory.md:39
   - `skills/supermemory/SKILL.md` — compare-supermemory.md:39
   - `.github/workflows/ci.yml` — compare-supermemory.md:45
   - `POST /v4/memories/forget-matching`, `POST /v4/search` — OpenAPI-spec-cited endpoints (compare-supermemory.md:15,21)
5. **Adoption status:** **STUDY ONLY.** Unified report ADOPT/BUILD/STUDY-equivalent posture: "Study only" (`:750`, HTML). compare-supermemory.md's final verdict: "these are not competitors; they are answers to different questions... The steal-worthy ideas are all retrieval-ergonomics accents (profiles, thresholds, pinning, an `extends` edge, external benchmarking), not architecture." Ideas explicitly flagged worth adopting (not yet acted on, "post-launch knob-tuning candidates at most"): precomputed profile injection, a search `threshold` param, `forget-matching`-style bulk lifecycle UX, `isStatic`-style pinning, diff-ingest idempotency pattern, running against supermemory's benchmark suite externally, an `extends` relation-edge type (compare-supermemory.md:70-77). Explicitly NOT to steal: LLM extraction/"dreaming", automatic decay curves, always-on LLM proxy (compare-supermemory.md:77).

---

## 5. Mem0

1. **Repo:** `github.com/mem0ai/mem0` (Apache-2.0) — `:987`.
2. **GOOD:**
   - "the closest external analogue to what pogan-mem's sidecar does, and worth studying carefully for that reason." (`:788`)
   - Retrieval fuses dense vector + BM25 + entity matching, scored with recency and importance — "study Mem0's hybrid scoring, since it does the fusion claude-mem does badly." (`:792`, HTML "Worth stealing")
   - Can run with zero paid keys — Ollama documented for both LLM and embedding provider. (`:788`)
3. **BAD / weak / rejected:**
   - Its `add()` mechanism runs two LLM calls: one to extract candidate facts, one to classify each **ADD / UPDATE / DELETE / NOOP** against nearest existing memories — "precisely the 'let the model silently rewrite the record' behaviour D1's append-only-plus-monotone-status model exists to forbid." (`:792`)
   - "neither offers signed, gate-minted provenance (D4), a precedence ladder, a taint boundary (D7), or crypto-shredding (D3)." (`:792`)
   - "the OSS library ships PostHog telemetry on by default, opt out with `MEM0_TELEMETRY=false`, and an open issue asks the maintainers to flip that to opt-in." (`:788`) — flagged as a live example of the telemetry problem in OQ-26 (`:849`).
   - A survey correction: the docs note the "Vector DB + Graph DB + Key-Value store" description commonly given for Mem0 is **stale** — graph memory is now built into the same vector store as a companion entity collection. (`:788`, `:961`)
4. **File paths / module names cited:** None given beyond the general mechanism description — no specific file paths, module names, or table names inside the Mem0 repo are quoted anywhere in scope. (The ADD/UPDATE/DELETE/NOOP mechanism is cited to an external arXiv paper, 2504.19413, not a repo file — `:987`.)
5. **Adoption status:** **REJECTED for adoption / STUDY only.** "Study, don't run." (`:749`) "So the posture is study, don't adopt." (`:792`)

---

## 6. Graphify

1. **Repo:** `Graphify-Labs/graphify` (Apache-2.0) — `:746`, `:767`, `:987`. PyPI package name is `graphifyy` (double-y), distinct from the GitHub org name.
2. **GOOD:**
   - "tree-sitter AST, no embeddings, no key" for code. (`:746`)
   - Deterministic graph, edges tagged `EXTRACTED`/`INFERRED`. (`:746`)
   - "Graphify's own published benchmarks show it beating vector systems decisively on recall@10 (0.497 against 0.048–0.149)." (`:776`)
   - "Claims no telemetry and keeps only a local query log at `~/.cache/graphify-queries.log`, disableable by env var." (`:781`, HTML)
3. **BAD / weak / rejected:**
   - Uses an LLM for **non-code** inputs (PDFs, images, video) — contradicts the "structural code intelligence rejects fuzzy vector math" framing the docs are correcting. (`:746`, `:776`)
   - Loses on LOCOMO QA accuracy (45.3% vs. Supermemory's 49.7%) and ties dense RAG on LongMemEval-S despite winning on recall@10. (`:776`)
   - PyPI package `graphifyy` (double-y) — "its own README warns that other `graphify*` PyPI names are unaffiliated squatters, which is a live typosquat hazard." (`:781`)
   - The local install on this machine is "roughly eighteen releases behind" upstream (v0.9.13 vs. v0.9.31 at time of writing). (`:772`, `:781`)
   - Staleness: "updates on demand or on git commit (`graphify hook install`), not in real time" — contrasted unfavorably with codegraph's continuous file-watching. (`:782`)
4. **File paths / module names cited:**
   - `/graphify` skill installed at `~/.claude/skills/graphify/` (`:772`, HTML — this is a **local install path on the pogan-toolkit machine**, not a path inside the Graphify repo itself)
   - local `graph.json` output file (HTML `graphify` card)
   - `~/.cache/graphify-queries.log` — local query log (`:781`, HTML)
   - `graphify hook install` — CLI command for git-commit-triggered updates (`:782`)
5. **Adoption status:** **NOT formally adopted by pogan-mem's design** (it predates/sits outside the ADOPT/BUILD/STUDY table), but **already installed and running** on the machine independently, which the docs treat as an open, undecided situation: "OQ-27 ... OPEN. ... Whatever is chosen, 8.5's pin-and-verify and egress steps must run against what is actually installed." (`:850`)

---

## 7. codegraph

1. **Repo:** `colbymchenry/codegraph` (MIT), npm package `@colbymchenry/codegraph` — `:747`, `:767`, `:987`.
2. **GOOD:**
   - "A native Rust kernel with compiled tree-sitter grammars parses ~20 languages, storing a single SQLite database per project with FTS5 search over symbols. Fully deterministic, no embeddings anywhere in the core." (HTML `codegraph` card)
   - Live daemon auto-syncing changed files in 5–50ms — "Its headline feature is real, not marketing." (`:771`)
   - Deterministic graph + FTS5 keyword retrieval, no key ever. (`:747`)
3. **BAD / weak / rejected:**
   - "codegraph collects anonymous usage telemetry by default... On this machine it is currently ON — `~/.codegraph/telemetry.json` reads `"enabled": true`... alongside an `update-check.json` that phones home for version checks." (`:780`) — this is flagged as an active, unresolved D5-rule violation, not a theoretical risk: "the first case of it being violated in practice rather than in theory." (`:807`)
   - "It also installs by curl-pipe-to-shell and writes MCP config into every detected agent's config file, which is machine-wide." (`:780`) — "the same pattern the fit guide flagged as concern C for codebase-memory-mcp." (`:809`)
   - Telemetry belongs to codegraph, not Serena — corrects an earlier draft's misattribution. (`:700`, `:710`)
4. **File paths / module names cited:**
   - `apps/ssm-clipper/.codegraph/` — holding a 9.6 MB `codegraph.db` (`:771`) — **note: this is a local project directory in the pogan-toolkit user's own repo tree where codegraph is running**, not a path inside the codegraph source repo itself.
   - `daemon.log` — live daemon log observed (`:771`)
   - `~/.codegraph/telemetry.json` (`"enabled": true`, `"consent_source": "default-notice"`) (`:780`)
   - `update-check.json` (`:780`)
   - `codegraph telemetry off`, env vars `CODEGRAPH_TELEMETRY=0`, `DO_NOT_TRACK=1` — opt-out mechanisms (`:780`, `:987`)
5. **Adoption status:** **NOT formally decided/adopted by pogan-mem's own ADOPT/BUILD/STUDY table**, but like Graphify, **already installed and running**, flagged as an open, undecided problem: "Plane 1 is not hypothetical — it is deployed and unaudited." (`:806`) OQ-26 (telemetry posture) and OQ-27 (audit-or-replace) both explicitly OPEN (`:849-850`).

---

## 8. LangGraph

1. **Repo:** `github.com/langchain-ai/langgraph` (MIT) — `:987`.
2. **GOOD:** "An MIT-licensed library for stateful, multi-actor agent graphs: nodes are steps, edges route conditionally, and state persists across the loop via checkpointing. It runs locally, free, no account." "The survey's description of it is accurate." (`:757`)
3. **BAD / weak / rejected:** Not disqualified on technical grounds — rejected because it's the wrong tool class: "pogan-mem is not a Python agent application... LangGraph solves a problem we do not have — we do not own the execution loop, the coding agent does." (`:763`)
4. **File paths cited:** None — no file paths inside the LangGraph repo are quoted anywhere in scope.
5. **Adoption status:** **REJECTED.** "Not adopted. We use harness primitives (skills, hooks, MCP), not a Python agent framework." (`:743`)

---

## 9. LangSmith

1. **Repo:** Only the client SDK is public/MIT (991★ per `:744`); "the platform itself has no public repo" (HTML). No specific github.com URL given for the SDK.
2. **GOOD:** "Tracing, debugging, evaluation and monitoring for LLM applications, including LLM-as-a-judge." (HTML)
3. **BAD / weak / rejected:** "it is closed-source SaaS. The free tier caps at 5,000 traces a month and the self-hosted option is Enterprise-only and paid. There is no free self-host path." (`:759`)
4. **File paths cited:** None.
5. **Adoption status:** **REJECTED on constraint.** "Our observability is OQ-18's local log + digest summary." (`:744`)

---

## 10. LangMem

1. **Repo:** `github.com/langchain-ai/langmem` (MIT) — `:987`.
2. **GOOD:** "LangMem's extraction primitives are storage-agnostic by its own README." (`:761`)
3. **BAD / weak / rejected:**
   - "`pyproject.toml` hard-pins `langgraph>=0.6.0` as a runtime dependency, and its memory tools sit on LangGraph's `BaseStore`. So you cannot use LangMem's memory tools without LangGraph." (HTML, `:745`)
   - "the documented default is OpenAI embeddings + Postgres" (`:745`) — not zero-key by default.
   - The docs note a source survey overstated the risk: "'Your memory architecture breaks if you leave the ecosystem' is too strong... The dependency claim is true; the catastrophe framing is not." (HTML)
4. **File paths cited:** `pyproject.toml` (the dependency pin) — `:761`, HTML. No deeper module paths given.
5. **Adoption status:** **REJECTED on coupling.** (`:745`)

---

## 11. Karpathy's "LLM Wiki"

1. **Repo:** Not a repo — a gist: `gist.github.com/karpathy/442a6bf555914893e9891c11519de94f` (file `llm-wiki.md`, 2026-04-04, 44,573 stars) — `:794`, `:987`. Explicitly "not software": "an idea file… designed to be copy pasted to your own LLM Agent."
2. **GOOD:**
   - "the architecture pogan-mem already chose." Pattern: raw sources stay immutable; an LLM maintains a wiki of interlinked markdown pages via three operations — ingest, query, lint. (`:794`)
   - Direct quote: "The wiki is just a git repo of markdown files. You get version history, branching, and collaboration for free." (`:794`)
   - "the lint pass as a first-class operation" is explicitly flagged as one thing to steal — a periodic pass hunting contradictions and orphaned pages, which pogan-mem's `/digest` doesn't currently do. (`:796`)
3. **BAD / weak:** "An LLM-maintained wiki with no notion of who authored a page and no untrusted-content quarantine is exactly the substrate D7's poisoning chain runs on" — i.e., it lacks pogan-mem's governed provenance (D4) and taint boundary (D7). (`:796`)
4. **File paths:** N/A — it's a single gist file, `llm-wiki.md`, not a repo with internal structure.
5. **Adoption status:** **ADOPTED (already the chosen architecture)**, plus one specific feature ("lint pass") flagged as worth stealing but not yet built. Attribution note: "the mechanism predates the gist — Cline's 'Memory Bank' already had an agent maintain a fixed set of markdown files as project memory." (`:798`)

---

## 12. Cline's "Memory Bank" (passing mention, not deeply studied)

1. **Reference:** `docs.cline.bot/best-practices/memory-bank` — cited only as prior-art attribution for Karpathy's pattern, not independently analyzed. (`:798`, `:987`)
2. **GOOD/BAD:** Not evaluated on its own — only credited as the predecessor mechanism ("an agent maintain a fixed set of markdown files as project memory").
3. **File paths:** None given.
4. **Adoption status:** Not applicable — mentioned only as an attribution footnote, not studied as a candidate.

---

## 13. Code-intelligence alternatives considered (Part 8.3 shortlist) — NOT deeply studied, comparison-table only

These four appear only in a comparison table (`:695-698`, HTML) with author/org names, not full `github.com/owner/repo` URLs, and get one line of characterization each — the docs do not go deeper than this table for any of them:

- **Serena** — "(Python · oraios)". LSP + editing, local, no telemetry, ~26.5k★, MIT. "the mature, exact all-rounder — LSP exactness means no hallucinated locations and no stale index — but it installs a language runtime and has edit tools live by default." (`:695`, `:700`) Earlier draft wrongly attributed telemetry to it; corrected — "Serena is clean on client machines... the telemetry belongs to codegraph." (`:710`)
- **Codanna** — "(Rust · bartolli)". Structural + local semantic, local/no key, ~700★, Apache-2.0. "the honest analog if you specifically want codebase-memory-mcp's shape... with more modest claims." Weakness: "Small community (bus-factor)." (`:696`, `:700`)
- **mcp-language-server** — "(Go · isaacphi)". Thin LSP wrapper, fully local, ~1.6k★, BSD-3. "the minimalist with the smallest blast radius." Weakness: "Not semantic; one language server per language." Judged wrong shape for pogan-mem's need regardless of its clean privacy profile, because it's stateless across renames. (`:697`, `:700`, `:706`)
- **claude-context** — "(TS · zilliztech)". Hybrid vector search, cloud/keys, ~12k★, MIT. "the strongest fuzzy recall and the highest privacy cost." Weakness: "Needs a vector DB + embeddings; local fork is unlicensed/stale." (`:698`, `:700`)

No file paths inside any of these four repos are given anywhere in scope. **Adoption status: none adopted** — all four are dormant alternatives noted in case codebase-memory-mcp is swapped out later (`:706`, `:717`).

---

## 14. a5c-ai/babysitter (indirectly studied, via its own supermemory-research directory)

1. **Repo:** `a5c-ai/babysitter` (sparse clone, read-only) — named as such at the top of `docs/reports/2026-08-07-compare-babysitter-research.md:1,3`. No full `github.com/...` URL string is given anywhere in scope; only the `owner/name` shorthand `a5c-ai/babysitter`.
2. **What's studied:** Not babysitter's memory system itself so much as **babysitter's own research output** (16 files under `docs/supermemory-research/`) about whether to adopt Supermemory. However, the report does describe babysitter's own in-house memory stack, "genty":
   - **GOOD:** Not much is said positively about genty itself — the report calls its retrieval "filter-only" and treats that as the gap genty's own research is trying to close (compare-babysitter-research.md:12,20).
   - **BAD:** "Their current pipeline is an LLM-extraction step writing entries to one local JSON file, deduplicated by Jaccard word-overlap... and retrieved by category/tag filters only — no semantic search at all." (compare-babysitter-research.md:12)
   - genty explicitly keeps orchestration state (`crossRunState.ts` — checkpoints, phase counters) OUT of its memory system, a call pogan-mem's own design agrees with. (compare-babysitter-research.md:24)
3. **File paths cited:** `crossRunState.ts` (compare-babysitter-research.md:24) is the only file path given inside babysitter/genty itself. The `docs/supermemory-research/` directory (containing `README.md`, `layer-analysis.md`, `integration-plan.md`, plus `raw/01`-`raw/13`) is babysitter's own research output, not genty's memory code.
4. **Adoption status:** Genty itself: **not evaluated for adoption** (it's the other project's internal tool, used here only as a comparison baseline). Babysitter's *conclusion* about adopting Supermemory ("yes, adopt") is explicitly **REJECTED as a template for pogan-mem** — see the Supermemory entry above; pogan-mem's spec is treated as having already "weighed and refused" nearly everything babysitter's research recommends adopting (compare-babysitter-research.md:62-70).

---

## Summary table

| Repo | GitHub URL given | Adoption status | File paths given? |
|---|---|---|---|
| ECC | `github.com/affaan-m/ECC` | ADOPTED (private clone, sandboxed eval pending) | Yes — several |
| codebase-memory-mcp | `github.com/DeusData/codebase-memory-mcp` | ADOPTED then SHELVED for V1 | Yes — several |
| claude-mem | `github.com/thedotmack/claude-mem` | STUDY ONLY / REJECTED to run | Yes — 2 TS files, 1 mdx, 2 issue numbers |
| Supermemory | `github.com/supermemoryai/supermemory` | STUDY ONLY | Yes — extensive (compare-supermemory.md) |
| Mem0 | `github.com/mem0ai/mem0` | STUDY ONLY / REJECTED to adopt | No file paths given |
| Graphify | `Graphify-Labs/graphify` (no full URL string) | Not decided (installed independently; OQ-27 open) | Only local-install paths, none inside the repo itself |
| codegraph | `colbymchenry/codegraph` (no full URL string) | Not decided (installed independently; OQ-26/27 open) | Only local-install paths, none inside the repo itself |
| LangGraph | `github.com/langchain-ai/langgraph` | REJECTED (wrong tool class) | No |
| LangSmith | no full URL (closed-source SaaS) | REJECTED on constraint | No |
| LangMem | `github.com/langchain-ai/langmem` | REJECTED on coupling | Only `pyproject.toml` named |
| Karpathy's LLM Wiki | `gist.github.com/karpathy/442a6bf555914893e9891c11519de94f` | ADOPTED (already the architecture) | N/A — single gist file |
| Cline's Memory Bank | `docs.cline.bot/best-practices/memory-bank` (docs page, not repo) | Not evaluated — attribution only | No |
| Serena / Codanna / mcp-language-server / claude-context | author/org only, no full URL | None adopted — dormant shortlist | No |
| a5c-ai/babysitter (genty) | `a5c-ai/babysitter` (shorthand only) | Not adopted — comparison baseline | 1 path (`crossRunState.ts`) |
