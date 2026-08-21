---
title: What pogan-mem actually built
type: ideation
status: ideation
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
version: "01"
sources: [docs/superpowers/01/ideation/findings/raw/built-inventory.md]
---

# What pogan-mem actually built

Code-verified inventory of the old system (repo `/home/ubuntu/projects/org/pogan-toolkit`). Every fact below traces to a file:line the source inventory cites; none of it comes from README, HOW-IT-WORKS, spec, or plan docs.

## Size
- `src/` excluding tests: 97 files, 33,945 lines. Including tests: 194 files, 80,336 lines — tests are ~46,391 lines, more than the source itself.
- Biggest file: `src/core/doctorChecks.ts`, 2,856 lines (the diagnostic check registry).

## CLI commands (`src/cli/main.ts`, 31 register calls), grouped by purpose
- **Bootstrap/repair**: `init`, `uninstall`, `restore`, `store move`, `upgrade`
- **Project registration**: `enable`, `disable`, `project set`
- **Capture**: `in <text>` (undistilled draft to inbox), `log <kind> <title>` (human-authored, lands live), `distill extract|discard|submit` (transcript-mining pipeline)
- **Human review/triage**: `inbox` (accept/reject drafts), `publish` (promote/queue triage), `ratify` (sole writer of `shared/`, opens a PR)
- **Human-only lifecycle**: `deactivate`, `reactivate`, `pin`, `reconfirm`
- **Retrieval/reporting**: `search`, `index rebuild`, `tally`, `report --week`, `export`, `eval|eval add` (fixture-only, never touches the live store)
- **Spine (codebase catalog)**: `spine build|modules|html`, `doc-pairs`
- **Org/keys/security**: `client new`, `key push|rotate|list`, `host retire`, `encrypt-project`, `shred`, `member offboard|return|add`
- **Misc**: `move` (record scope move), `glossary add`/`categories edit`, `doctor` (~18-check registry)

## MCP tools (`src/plugin/mcp/server.ts`, name `pogan`) — 9 total

| Tool | What it does | Source |
|---|---|---|
| `mem_search` | Ranked search, returns ids/titles/scores only | readTools.ts:74 |
| `mem_get` | Full record body by ULID; banners DEACTIVATED/SUPERSEDED | readTools.ts:168 |
| `failed_before` | Title-only check for a prior recorded failure | readTools.ts:243 |
| `spine_card` | Deterministic architecture card for the cwd project | readTools.ts:279 |
| `spine_lookup` | Resolves one symbol/file against the spine db | readTools.ts:305 |
| `mem_log` | Creates a record, `state: active` immediately, no review step | writeTools.ts:568 |
| `mem_supersede` | Marks old record superseded by new; forward-only | writeTools.ts:823 |
| `mem_propose` | Writes a promote/deactivate/link proposal annotation; decides nothing | writeTools.ts:929 |
| `mem_elevate` | Loads a record as a binding constraint for the run; 3 gating checks | writeTools.ts:1100-1131 |

Write tools capped at `WRITE_CAP = 100` writes/session (writeTools.ts:72).

## Hooks — where they were registered

- **`hooks/hooks.json`** (plugin-provided, ≤1.5s SessionStart/PostToolUse budget, sessionEnd.ts:5-14): `SessionStart` (startup|resume|fork|clear, async 120s) → sessionAsync.ts; `SessionStart` (compact, 2s) → sessionStart.ts's cached-replay arm; `UserPromptSubmit` (10s) → userPromptSubmit.ts (FTS-only injection gate); `PostToolUse` (Read|Edit|Write|Bash, 5s) + `PostToolUseFailure` (Bash, 5s) → postToolUse.ts (first-touch anchor injection).
- **Registered directly in `/home/ubuntu/.claude/settings.json`** (outside the repo, per init.ts:41-51's own comment that "a plugin's SessionStart/SessionEnd never fired at all" under some invocation modes): `SessionStart` → `dist/plugin/hooks/sessionStart.js` (blocking arm, builds the injected context block); `SessionEnd` (30s) → `dist/plugin/sessionEnd.js` (pushes pending commits, folds events).
- **MCP server registration**: under `/home/ubuntu/.claude.json`'s user-scope `mcpServers`, not a shipped `.mcp.json` — same "plugin registration doesn't reliably fire" reasoning.
- **Git hooks**, installed into `/home/ubuntu/.pogan/hooks/`: `pre-commit`/`pre-push` → `gateCheck.js` (allowlist-path refusal, gitleaks, blocking); `commit-msg` → `gateCheck.js --commit-msg` (never blocks, `|| true`).

## Storage layout

- Store root: `/home/ubuntu/.pogan/` (default `~/.pogan`). Three separate git repos: `memory/personal`, `memory/org`, `memory/shared`, plus non-repo `cache/`, `hooks/`, `config.json`.
- One Markdown file per record: YAML frontmatter + body, ULID filename.
- 4 record kinds: `decision`, `fact`, `failed`, `recipe`.
- Frontmatter (record.ts:71-) holds: id, kind, schema_version, title, created, author (agent|human), provenance (human-decided|human-approved|agent-written), state (active|deactivated), deactivated{at,reason}, superseded_by, scope, anchors[] (repo+path+symbol, or category, or everywhere), born_in{repo,path,session,agent,model,effort,task?}, origin (internal|external), links[], proposed{}, derived_from[], plus optional source/verified/embedding/embed_model/pinned.

## Retrieval stack (`src/core/index/query.ts`, `memSearch`)

1. Per-partition FTS5 (bm25, cut at 20) + one global vector list, only if an embedder is loaded.
2. Reciprocal Rank Fusion (k=60), then multiply by fixed per-layer weights (project 1.0 down to user 0.88).
3. Graph expand over the fused top 10, up to 2 hops, 0.5 hop decay, capped below the lowest direct hit.
4. Optional cross-encoder rerank of the top 10 — only if config says on AND the model weights exist on disk AND the time budget allows.
5. Hard precedence override between conflicting records, then min_score/since/until filtering, cut to k (default 10).

## Capture paths — every way a record could be created

| Path | Where | Live immediately? | Needed human approval before the memory was live? |
|---|---|---|---|
| `pogan log` | capture.ts | Yes, direct to store | No — author is human by definition |
| `pogan in <text>` | capture.ts:511 | No, lands in `inbox/` | Yes — `pogan inbox` |
| `mem_log` (MCP) | writeTools.ts:568 | Yes, `state: active` | **No** |
| `mem_supersede` (MCP) | writeTools.ts:823 | Yes, modifies in place | No |
| `distill extract`→`submit` | distill.ts | No, drafts to inbox | Yes — `pogan inbox` |
| Automatic failure-nudge draft | nudgeDraft.ts:174 | No, routes to inbox | Yes — `pogan inbox` |
| Recurrence-triggered promotion proposal | recurrenceCapture.ts | N/A, stamps existing record | Still needs `pogan publish`/`ratify` |

Bottom line, in the inventory's own words: MCP's `mem_log`/`mem_supersede` were the only paths that put a fully live, searchable record into the store with zero human gate.

## Built but never used

- **Vector search**: `config.models` starts `'absent'` on every fresh `init`/`restore` (init.ts:231, restore.ts:184); no CLI command ever downloads the ONNX weights (`grep -rn "download"` across src/cli found nothing). Dead on a stock install.
- **Reranker**: `config.reranker` defaults `'off'` (init.ts:232, restore.ts:184), same no-download problem (embed.ts:22).
- **`WRITE_CAP = 100`**: a per-session MCP write cap, effectively unreachable in ordinary single-task usage.
- **`pogan eval`**: runs only against a committed fixture store (`spec/fixtures/eval-store/`), never the live store (eval.ts:1-3).
- **`contradiction_candidates` table**: written at index time (build.ts:134) but has no MCP tool or dedicated CLI command to browse it directly.
