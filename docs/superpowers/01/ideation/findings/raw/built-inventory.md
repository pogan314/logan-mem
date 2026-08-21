---
title: Raw — inventory of what the old code actually implements
type: ideation
status: ideation
created: "2026-08-21 13:07 CDT"
updated: "2026-08-21 13:07 CDT"
version: "01"
sources: [../pogan-toolkit/src read 2026-08-21, /home/ubuntu/.pogan/memory read 2026-08-21]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions.

# pogan-toolkit — built-system inventory (code-verified)

Repo: `/home/ubuntu/projects/org/pogan-toolkit`. Every claim below is from source read directly (file:line cited); no claim is taken from README.md, HOW-IT-WORKS.html, spec/, or plans/ — those are known (per repo's own recent commits) to overstate what the code does.

## 1. Size

- `src/` excluding tests: 97 files, 33,945 lines.
- `src/` including tests: 194 files, 80,336 lines (i.e. tests are ~46,391 lines, more than the source itself).
- Biggest 15 non-test source files:

| Lines | File |
|---|---|
| 2856 | src/core/doctorChecks.ts |
| 1425 | src/plugin/hooks/sessionStart.ts |
| 1257 | src/core/evalRig.ts |
| 1197 | src/core/distill.ts |
| 1154 | src/plugin/mcp/writeTools.ts |
| 1082 | src/core/index/build.ts |
| 936 | src/cli/commands/capture.ts |
| 934 | src/cli/commands/publish.ts |
| 897 | src/core/index/query.ts |
| 874 | src/core/session.ts |
| 819 | src/plugin/hooks/postToolUse.ts |
| 704 | src/plugin/mcp/server.ts |
| 684 | src/cli/commands/ratify.ts |
| 594 | src/cli/commands/key.ts |
| 593 | src/core/reportWeek.ts |

## 2. CLI surface (`src/cli/main.ts`)

31 register calls wire in the following top-level commands (subcommands noted):

- `pogan init` / `pogan uninstall` (src/cli/commands/init.ts) — first-time store bootstrap: creates the personal/org/shared repo trees, writes config.json, installs git hooks + harness hooks/MCP registration into `~/.claude/settings.json`. `uninstall` reverses the harness registration.
- `pogan enable` / `pogan disable` / `pogan project set` (project.ts) — per-project registration/deregistration in the registry, and slug/rename management.
- `pogan spine build|modules|html`, `pogan doc-pairs` (spine.ts) — builds the per-project architecture "spine" SQLite db (tree-sitter parse of files/symbols/deps/edges) and renders an HTML dependency graph.
- `pogan in <text>` (capture.ts) — quick capture: writes an *undistilled draft* into `inbox/`, no kind classification yet; requires later `pogan inbox` to file it.
- `pogan log <kind> <title>` (capture.ts) — direct human-authored record write (decision/fact/failed/recipe) straight into the real store, not inbox.
- `pogan inbox [--limit] [--reject-all --reason]` (capture.ts) — the human review/triage step: accept-and-file (spawns an editor), or batch-reject, every queued inbox draft (both `pogan in` drafts and distiller-submitted drafts).
- `pogan client new` (client.ts) — mints a new encrypted "owned" client identity (age keypair) for a private/unshared project.
- `pogan deactivate <id> --reason` / `pogan reactivate <id>` (deactivate.ts) — retires/restores a record's `active` state. One of spec's five TTY-gated "human kill switch" commands.
- `pogan pin <id> [--unpin]` (pin.ts) — flips the human-only `pinned` lifecycle flag; the only path that can ever set it (MCP write tools explicitly refuse the field).
- `pogan index rebuild` (index.ts) — rebuilds a world's SQLite retrieval index from the on-disk record files.
- `pogan eval` / `pogan eval add` (eval.ts) — replays a committed fixture answer-key through the real retrieval surfaces and reports spec §14 metrics; `add` grows that answer key. Runs against `spec/fixtures/eval-store/`, never the live store.
- `pogan publish` (publish.ts) — triage over recently-captured records: promote to project/user layer, queue a promotion/link proposal into shared (never writes shared/ itself), or report never-retrieved records.
- `pogan ratify` (ratify.ts) — the sole writer of `shared/`: consumes the pending-ratify queue, opens a PR against pogan-shared inside a throwaway clone.
- `pogan reconfirm <id>` (reconfirm.ts) — appends a `reconfirmed` event to the confirming member's own event log; never edits the target record.
- `pogan report --week` (report.ts) — computes the week-gate artifact (spec §4b's 7 clauses) from events + index, read-only.
- `pogan doctor` (doctor.ts) — runs the full check registry (src/core/doctorChecks.ts, 2856 lines) and prints a pass/fail table; the store's diagnostic catch-all.
- `pogan key push|rotate|list [--all] [--verify]` (key.ts) — SSH-based per-client key distribution/rotation/audit across every registered host.
- `pogan host retire <id> --attestation` (host.ts) — retires one host across every client that lists it.
- `pogan encrypt-project` (encryptProject.ts) — retro-encrypts an existing plaintext project's history by replaying it under a fresh owned/encrypted identity.
- `pogan shred <client>` (shred.ts) — remote single-pass overwrite (`dd`+`rm`, not `shred(1)`) of a departed client's keys/transcripts over SSH; produces a per-file destruction report.
- `pogan member offboard <user>` / `pogan member return <name>` / `pogan member add` (member.ts) — org membership lifecycle: manifest PR removing hosts, per-host shred, folder moves to `_departed`.
- `pogan restore` / `pogan store move <path>` (restore.ts) — disaster recovery (re-clone all three repos + rebuild the index on a machine that lost `~/.pogan`) and whole-store relocation.
- `pogan store move` — see above (registerStoreMove, same file).
- `pogan tally` (misc.ts) — usage/metric rollup from events/index.
- `pogan move` (misc.ts) — record-level scope move (`git mv` + placement re-derivation) across layers/projects.
- `pogan search <query>` (misc.ts) — CLI-side ad hoc query against `memSearch` (same retrieval pipeline MCP's `mem_search` uses).
- `pogan glossary add <term> <categories…>` / `pogan categories edit` (glossary.ts) — edits the shared category-graph glossary via a PR against pogan-shared.
- `pogan upgrade` (upgrade.ts) — the batch on-disk schema-migration runner (one registry entry today: a same-version 1→1 canonicalization rewrite).
- `pogan distill extract [--no-model] [--budget]` / `distill discard <run>` / `distill submit <draft-file>` (distill.ts) — the transcript-mining pipeline: `extract` reads Claude Code transcript JSONL and produces local draft files (heuristic or model-assisted), `discard` drops a run, `submit` is "the sole write path into a member's inbox" for distilled drafts (still requiring `pogan inbox` review before becoming a live record).
- `pogan export <id> [--out path]` (export.ts) — read-only: renders one record as a skill-shaped markdown scaffold to stdout/file; explicitly never calls any store-write function.

## 3. MCP surface (`src/plugin/mcp/`)

Server: `src/plugin/mcp/server.ts` (704 lines), name `pogan`, per-session stdio server, mints its own session id (`server-<ULID>`) because `${CLAUDE_SESSION_ID}` does not expand in a plugin MCP server's env (server.ts:6-14). 9 tools total, split across `readTools.ts` and `writeTools.ts`:

Read tools (readTools.ts):
- `mem_search(query, k?, kind[]?, layer[]?, min_score?, since?, until?)` — ranked search over this team's memory; returns ids/titles/scores only (readTools.ts:74).
- `mem_get(id)` — full record body by ULID; banners "DEACTIVATED"/"SUPERSEDED" when applicable (readTools.ts:168).
- `failed_before(anchors? | query)` — cheap title-only check for a prior recorded failure on a file/symbol/topic (readTools.ts:243).
- `spine_card()` — the deterministic architecture card for the cwd project (entry points, clusters, deps, tests) (readTools.ts:279).
- `spine_lookup(symbol | path)` — resolves one symbol or file against the parsed spine db (readTools.ts:305).

Write tools (writeTools.ts), capped at `WRITE_CAP = 100` writes/session (writeTools.ts:72):
- `mem_log(kind, title, body, scope?, anchors?, source?, recurs?, origin?, prior_hash?, instructed?)` — creates one record. Always `author: 'agent'`, `provenance: 'agent-written'`, state `active` — **lands live and searchable immediately, no staging/review step** (writeTools.ts:568, write logic ~633-690). Refuses `scope: 'shared'` and any `pinned` argument by name.
- `mem_supersede(old, new, reason, instructed?)` — marks `old.superseded_by = new`; forward-only, refuses if already superseded (writeTools.ts:823).
- `mem_propose(id, proposal: promote|deactivate|link, reason, to_layer?, to?, link_kind?)` — writes a proposal annotation onto the frontmatter; "proposals decide nothing — a human reviews them" (writeTools.ts:929).
- `mem_elevate(id)` — loads one record as a binding constraint for the rest of the run; gated by three independent checks: provenance must not be `agent-written`, origin must not be `external`, state must not be `deactivated` (writeTools.ts:1100-1131).

## 4. Hooks

Harness hooks, registered two different ways:

- **hooks/hooks.json** (plugin-provided; capped at a 1.5s total SessionStart/PostToolUse budget per Claude Code docs, per sessionEnd.ts's own comment at src/plugin/sessionEnd.ts:5-14):
  - `SessionStart` (matcher `startup|resume|fork|clear`, async, 120s) → `sessionAsync.js` (src/plugin/hooks/sessionAsync.ts) — pulls remotes, rebuilds indexes, pushes queued commits; every arm degrades to a logged-outcome event rather than throwing.
  - `SessionStart` (matcher `compact`, 2s) → `sessionStart.js`'s cached-replay arm.
  - `UserPromptSubmit` (10s) → `userPromptSubmit.js` (src/plugin/hooks/userPromptSubmit.ts) — FTS-only gate (bm25/path/symbol/glossary triggers) that injects at most a few matching records into the prompt; "hooks are FTS-only by contract" (no vector arm here even when an embedder is present).
  - `PostToolUse` (matcher `Read|Edit|Write|Bash`, 5s) and `PostToolUseFailure` (matcher `Bash`, 5s) → both point at `postToolUse.js` (src/plugin/hooks/postToolUse.ts) — first-touch anchor injection per (agent, file), a manifest-dependency clause (Edit/Write to package.json etc. injects matching glossary categories), and unconditionally appends the touched path to a per-session file the next prompt's gate reads.
- **Registered directly in `~/.claude/settings.json`** by `pogan init`/`restore` (not via hooks.json, because — per src/cli/commands/init.ts:41-51 and src/core/settings.ts's own comments — "a plugin's SessionStart/SessionEnd never fired at all" under some harness invocation modes):
  - `SessionStart` → `dist/plugin/hooks/sessionStart.js` (the blocking arm — builds and injects the actual context block).
  - `SessionEnd` (timeout 30) → `dist/plugin/sessionEnd.js` (src/plugin/sessionEnd.ts) — pushes any pending commits, stages orphaned MCP-server event files, folds captured events.
  - MCP server itself is registered under `~/.claude.json`'s user-scope `mcpServers`, not a shipped `.mcp.json` — same "plugin registration doesn't reliably fire" rationale.

Git hooks (`hooks/` at repo root, installed into `<store>/hooks/` by `installHooks`, config.ts):
- `pre-commit` → `gateCheck.js --staged` — allowlist-path refusal, `gitleaks protect --staged`, eval-fixture-ULID refusal. Blocking.
- `pre-push` → `gateCheck.js --push` — same scans over the whole pushed commit range. Blocking.
- `commit-msg` → `gateCheck.js --commit-msg "$1"` — message-shape + client-vocabulary warnings, explicitly **never blocks** (`|| true` even if node itself fails, per hooks/commit-msg's own comment).

## 5. Storage model

Three separate git repos under `<store_root>` (default `~/.pogan`): `memory/personal`, `memory/org`, `memory/shared`, plus non-repo `cache/`, `hooks/`, `config.json`.

**Record file**: one Markdown file per record, YAML frontmatter + body, ULID filename. Frontmatter schema (`src/core/record.ts:71-` `FrontmatterSchema`): `id` (ULID), `kind` (decision|fact|failed|recipe), `schema_version` (1|2), `title`, `created` (ISO datetime, normalized on read/write), `author` (agent|human), `provenance` (human-decided|human-approved|agent-written), `state` (active|deactivated), `deactivated{at,reason}|null`, `superseded_by`, `scope`, `anchors[]` (repo+path+symbol, or repo+path, or category, or everywhere — record.ts:8-13), `born_in{repo,path,session,agent,model,effort,task?}`, `origin` (internal|external), `links[]` (typed edges: supersedes/caused-by/governs/see-also/extends), `proposed{links,promote,deactivate}`, `derived_from[]`, plus optional `source`, `verified`, `embedding` (base64 float vector), `embed_model`, `pinned`.

**Directory layout** (derived from `src/core/placement.ts`'s allowlist regexes, which are also what the pre-commit/pre-push git gate enforces):
- Plaintext trees (personal + org repos): `user/{decisions,facts,failed,recipes,inbox}/<ULID>.md`; `members/<user>/{...same kinds}/<ULID>.md` (plus `_departed/<user>/...`); `projects/<slug>/{project.json, <kinds>/<ULID>.md, members/<user>/<kinds>/<ULID>.md}`; `registry/<ULID>/*.json`; `events/<yyyy-mm>/<file>.jsonl` and `events/archive/<yyyy-mm>.jsonl`; `eval/answer-key.jsonl`.
- Owned/encrypted tree (also inside personal/org repos): `clients/<ULID owner>/<ULID project>/(project.json | <kinds>/<ULID>.age | members/<user>/<kinds>/<ULID>.age)` — everything but `project.json` is age-encrypted ciphertext.
- Shared repo: `standards/<ULID>.md`, `categories/{categories.md, glossary.json}`, `keys-manifest.json`.

**SQLite index schema** (`src/core/index/build.ts:106-133`, `attachSchema`, one db per world):
```
records(id PK, kind, title, created, author, provenance, state, superseded_by, scope, origin,
        world, layer, project, member, encrypted, anchors_json, body, ratified_by,
        proposed_promote_to, proposals_open)
records_kind_state INDEX (kind, state)
fts VIRTUAL TABLE USING fts5(title, body, content='records', content_rowid='rowid',
        tokenize='unicode61 remove_diacritics 2')
links(ulid PK, src, dst, kind)
counters(id, name, value, PK(id,name))
meta(key PK, value)
pending_embed(id PK, queued_at, attempts, last_error)
bad_file_ids(id PK, reason)
vec VIRTUAL TABLE USING vec0(id PK, embedding float[384])
contradiction_candidates(a, b, kind, cosine, PK(a,b))
```
A separate per-project "spine" db (`src/core/spine/db.ts:21-26`): `files(path PK, lang, one_liner, hash, mtime, size)`, `symbols(id PK, path, name, kind, line)` + name index, `edges(src_path, dst_path, kind)`, `deps(name, version, manifest)`, `meta(key PK, value)`.

## 6. Retrieval (`src/core/index/query.ts`, `memSearch`, lines 736-897)

Pipeline, in order: (1) per-partition FTS5 list (bm25, cut at `CANDIDATE_LIMIT=20` per partition) + one global vector list (`vecSearch`, cosine, same cut) **only if an embedder is loaded**; (2) Reciprocal Rank Fusion (`rrfFuse`, k=60) across all lists, then multiply each fused score by a fixed per-layer weight (`LAYER_WEIGHTS`: project 1.0, project-member 0.97, shared 0.94, member 0.91, user 0.88); (3) graph expand over the fused top 10 seeds, up to 2 hops, 0.5 hop decay, walking asserted `links` rows plus (if a category graph was supplied) one-hop category neighbourhoods, capped at 0.999× the lowest direct hit's score so a graph-walked record never outranks a direct one; (4) optional cross-encoder rerank of the top 10, only if `config.reranker === 'on'` AND the reranker weights are actually present on disk AND remaining time budget allows it (else `skipped-config`/`skipped-missing-model`/`skipped-budget`/`skipped-error`); (5) hard precedence override between directly-conflicting records, then optional `min_score`/`since`/`until` filtering, then cut to `k` (default 10).

What's ON by default vs gated:
- **FTS/BM25**: always on (fts5, `unicode61 remove_diacritics 2` tokenizer).
- **Vector search**: gated behind `config.models === 'present'`, which every `loadEmbedder()` call site checks first (server.ts:478, capture.ts:342, distill.ts:96, misc.ts:493) — and **`pogan init`/`pogan restore` both hard-code `models: 'absent'`** (src/cli/commands/init.ts:231, src/cli/commands/restore.ts:184) with the comment "nothing downloads models yet — plan 03's embed task flips this." The only writer of `config.models` is a doctor check (`doctorChecks.ts:1961-1979`, `modelsFlag`) that *re-verifies against disk and rewrites the flag* — it never triggers a download. No CLI command downloads the ONNX weights. Net effect: on a fresh install, vector search (and everything gated on it: recurrence detection, distill dedup) stays permanently off unless a human manually places `Xenova/bge-small-en-v1.5` ONNX weights under `cache/models/` outside any pogan command.
- **Reranker**: `config.reranker` defaults `'off'` at both init and restore (init.ts:232, restore.ts:184); `bge-reranker-base` is, per its own header comment (embed.ts:22), "downloaded lazily on first use, shipped off." Same no-download-path problem as the embedder.
- **Graph expand / precedence override / layer weights**: always on, no flag.
- Hooks' own retrieval (`userPromptSubmit.ts`'s gate) is FTS-only unconditionally, by contract, even when an embedder happens to be loaded elsewhere.

## 7. Capture — every code path that can create a record

| Path | Where | Live immediately? | Human review? |
|---|---|---|---|
| `pogan log` | capture.ts | Yes, direct to store | Author is human by definition (CLI, human-typed) |
| `pogan in <text>` | capture.ts:511 | No — lands in `inbox/` with no kind (routed by `placement.kind = null`) | Yes — needs `pogan inbox` to accept/edit/file it |
| `mem_log` (MCP) | writeTools.ts:568 | **Yes — writes `state: active` straight to its real kind directory** | **No** — `author:'agent'`, `provenance:'agent-written'`, immediately searchable/retrievable |
| `mem_supersede` (MCP) | writeTools.ts:823 | Yes, modifies existing record in place | No |
| `distill extract` → `distill submit` | distill.ts | No — `extract` only produces local draft files (from Claude Code transcript JSONL, heuristic or model-assisted); `submit` is described as "the sole write path into a member's inbox" | Yes — filed drafts still need `pogan inbox` |
| Automatic failure-nudge draft | src/core/nudgeDraft.ts:174 (`kind: null`, comment "inbox") | No — routes to inbox/ same as `pogan in` | Yes — needs `pogan inbox` |
| Recurrence-triggered promotion proposal | src/core/recurrenceCapture.ts | N/A — this doesn't create a record, it stamps `proposed_promote_to` onto the record just captured by mem_log/pogan log | Proposal only — still needs `pogan publish`/`pogan ratify` to act |
| `pogan pin` / `deactivate` / `reactivate` | pin.ts, deactivate.ts | Modify existing records only, not creation | Human-only (TTY-gated CLI) |

Bottom line: **MCP's `mem_log`/`mem_supersede` are the only paths that put a fully "live" record into the searchable store with zero human gate** — every other creation path (`pogan in`, the distiller, the failure-nudge) is explicitly staged into `inbox/` pending `pogan inbox`.

## 8. Dead / gated-off / unreachable code (specific findings)

1. **Vector search has no bootstrap path in the shipped product.** `config.models` starts `'absent'` on every fresh `pogan init` (init.ts:231) and every `pogan restore` (restore.ts:184). Every single call site that would load the embedder checks `cfg.models === 'present'` *before* calling `loadEmbedder` (server.ts:478, capture.ts:342, distill.ts:96, misc.ts:493) — so `loadEmbedder`'s own `pipeline()` call (embed.ts:190, which would trigger `@huggingface/transformers`' auto-download) is never reached from a fresh install. The only writer of `config.models` is `doctorChecks.ts`'s `modelsFlag` check (lines 1961-1979), which only *reads* disk state and rewrites the config flag to match — it never fetches anything. There is no `--download` flag or equivalent command anywhere in the CLI (`grep -rn "download"` across src/cli turns up nothing). Net: the vector arm of `memSearch`, embedder-gated dedup in distill.ts, and `checkRecurrence`'s embedding comparison are all unreachable on a stock install unless an operator manually drops ONNX weights on disk outside the tool entirely. (The live store at `~/.pogan` on this machine does have `models: "present"` and embeddings on some records — so this was bootstrapped by hand at some point, not by the shipped code.)
2. **Reranker ships off and has the identical no-bootstrap problem** — `config.reranker` defaults `'off'` (init.ts:232, restore.ts:184) and the model, per embed.ts:22, is "shipped off" with no download path either. Confirmed dead in the actual dogfood store too: `~/.pogan/config.json` shows `"reranker": "off"`.
3. **`WRITE_CAP = 100`** (writeTools.ts:72) — a hard per-session cap on MCP writes; exercised only by heavy agent sessions, effectively unreachable in ordinary single-task usage.
4. **`pogan eval`** operates exclusively against a committed fixture store (`spec/fixtures/eval-store/`), never the live store (eval.ts:1-3) — a real capability, but structurally never touches or reflects the actual `~/.pogan` data a user has.
5. **`contradiction_candidates` table** (build.ts:134) is written at index/write time but only read back through `pogan publish`'s own surfacing path (publish.ts) — there is no MCP tool or dedicated CLI command to browse contradictions directly; it's a side effect of one command's triage view, not a first-class surface.

## Live store state (`~/.pogan/memory/`, read-only inspection)

Config on this host: `models: "present"`, `reranker: "off"`, `flush_mode: "verified"`, `first_touch: "on"`, `host_id: "ec2-lg-main"`.

Record counts (actual `.md` files, excluding `.git/` internals, glossary/manifest infra files):
- **personal**: 4 records — `user/facts/` (1), `user/recipes/` (1), `projects/pogan-live-check/facts/` (2, both `state: deactivated`).
- **org**: 30 records — `members/lgerard42/facts/` (1, active) + `members/lgerard42/inbox/` (29, all undistilled drafts awaiting `pogan inbox` review — none filed yet).
- **shared**: 1 record — `standards/` (1 recipe, ratified).
- Plus non-record infra: `shared/categories/categories.md`, `shared/categories/glossary.json`, `shared/keys-manifest.json`.

Read 6 actual bodies. Two clusters, very different in character:
- **The 6 filed/live records** (personal + org facts/recipes + the 1 shared standard) are almost entirely bootstrap/meta notes about the tool's own setup — "the pogan store was initialized on host X," "how to join a second machine," "this host is member Y's primary" — plus two deactivated agent-written scratch records explicitly tagged `reason: live-gate scratch`. These read as installation-log boilerplate, not project knowledge a team would actually want recalled later.
- **The 29 org inbox drafts** (unreviewed, produced by a `distill-mine-trial` run against a real Claude Code transcript) are substantially more useful: specific, verified technical lessons with real command output and root causes (e.g. "`gh pr create` has no `--json` flag — parse the URL from stdout," "accepting a GitHub org invite grants base read, not repo write," "a green stub suite proves nothing — a live-credential release-gate check surfaced 4 real bugs in one day"). These carry populated `embedding`/`embed_model` fields, confirming vector indexing was actually exercised at some point on this host. None of the 29 have been triaged into the real store yet.

Overall read: the store's *filed* content is thin and mostly self-referential (junk/boilerplate about pogan itself), while its *un-triaged* content (still sitting in inbox, never reviewed) is where the actually useful, specific engineering knowledge lives.
