---
title: obra/episodic-memory
type: wiki
status: research-fact
created: 2026-08-21
updated: 2026-08-21
sources: [live `gh api` calls against github.com/obra/episodic-memory, HEAD tree; README.md; CLAUDE.md; .claude-plugin/plugin.json; .codex-plugin/plugin.json; .mcp.json; package.json; hooks/hooks.json; agents/search-conversations.md; skills/remembering-conversations/SKILL.md; src/paths.ts; src/db.ts; src/mcp-server.ts; src/embeddings.ts; src/summarizer.ts; src/indexer.ts; src/parser.ts]
---

# obra/episodic-memory

## 1. Identity (verified 2026-08-21)

| Field | Value |
|---|---|
| URL | github.com/obra/episodic-memory |
| License | MIT |
| Stars | 469 |
| Last push | 2026-05-21T21:30:30Z |
| Created | 2025-10-17T17:38:13Z |
| Open issues | 58 |
| Language | TypeScript |
| Author | Jesse Vincent (jesse@fsck.com) |
| Latest version tag in package.json | 1.4.2 |

## 2. What it does

- Indexes past Claude Code and Codex conversation transcripts and makes them searchable by meaning, not just keyword. (README.md)
- Runs entirely as a plugin: a `SessionStart` hook syncs new conversations in the background, an MCP server exposes `search` and `read` tools, and a dispatched subagent does the searching so the main agent's context stays small. (hooks/hooks.json, src/mcp-server.ts, agents/search-conversations.md)
- A skill (`remembering-conversations`) tells the agent to search past conversations before saying "I don't know" or treating a topic as new. (skills/remembering-conversations/SKILL.md)
- Conversations can opt out of indexing with an inline marker string; the file is still archived, just not searchable. (README.md)

## 3. How it works

- **Capture**: a `SessionStart` hook (matcher `startup|resume|clear`) runs `episodic-memory.js sync --background` — no other hook events are registered. (hooks/hooks.json)
- **Parse**: reads Claude Code JSONL transcripts (`user`/`assistant` message blocks, tool_use/tool_result blocks, cwd, git branch, model, session ID) or Codex rollout JSONL (`session_meta`/`turn_context`/`response_item` events), auto-detected per file. Pairs each user turn with the following assistant turn(s) into one "exchange." (src/parser.ts)
- **Store**: default DB path is `~/.config/superpowers/conversation-index/db.sqlite` (overridable via `EPISODIC_MEMORY_DB_PATH`/`TEST_DB_PATH`; the parent dir comes from `PERSONAL_SUPERPOWERS_DIR`/`XDG_CONFIG_HOME`/`~/.config/superpowers`). SQLite tables: `exchanges` (id, project, timestamp, user_message, assistant_message, archive_path, line_start/end, embedding, harness, session_id, cwd, git_branch, model, thinking metadata, embedding_version), `tool_calls` (FK to exchanges, `ON DELETE CASCADE`), and a `vec0` virtual table `vec_exchanges` (id, `embedding FLOAT[384]`) via the `sqlite-vec` extension. Raw transcript files are also copied verbatim into a conversation archive directory. (src/paths.ts, src/db.ts)
- **Embed**: local model `Xenova/bge-small-en-v1.5` (384-dim, q8 quantized) run through `@huggingface/transformers` (Transformers.js) — no external API call. Passage/document text is embedded unmodified; queries get a BGE-specific prefix (`"Represent this sentence for searching relevant passages: "`) prepended first. Text is truncated to 2000 chars before embedding. (src/embeddings.ts)
- **Search**: MCP tools are `search` (single string for vector/text/both search, or an array of 2-5 strings for multi-concept AND search; filters on date/project/session/git branch; returns markdown or JSON with project, date, snippet, similarity, file path) and `read` (returns a full conversation as markdown, given an absolute file path, with optional line range). (src/mcp-server.ts)
- **Summarize**: `summarizeConversation()` calls a model to write a 2-4 sentence factual summary per conversation, stored as a sibling `-summary.txt` file. For Claude-harness conversations it calls the Claude Agent SDK's `query()` (default model `haiku`, fallback `sonnet`, overridable via `EPISODIC_MEMORY_API_MODEL*` env vars); it can resume the original session for context. For Codex-harness conversations it forks the original session via `codex app-server`'s `thread/fork` (ephemeral, read-only sandbox) and asks that fork to summarize itself. Long conversations (>15 exchanges) are chunked into groups of 8, summarized separately, then synthesized into one summary. A reentrancy guard env var (`EPISODIC_MEMORY_SUMMARIZER_GUARD`) stops the SDK-spawned subprocess from re-triggering the same `SessionStart` sync hook. (src/summarizer.ts, CLAUDE.md)

## 4. Install surface

- Single-plugin install via `/plugin install episodic-memory@superpowers-marketplace` (Claude Code) or `codex plugin marketplace add` (Codex) — one plugin, not several. (README.md)
- `.claude-plugin/plugin.json` registers: one MCP server (`episodic-memory`, launched via `node cli/mcp-server-wrapper.js`) and one agent (`agents/search-conversations.md`). It does not list a `hooks` key itself.
- `.codex-plugin/plugin.json` (the Codex manifest) registers: `skills` (`./skills/`), `hooks` (`./hooks/hooks.json`), and `mcpServers` (`./.mcp.json`) together.
- `hooks/hooks.json` installs exactly one hook: `SessionStart` → `sync --background`.
- `skills/remembering-conversations/SKILL.md` is installed as a skill that instructs the agent when to dispatch the search subagent.

## 5. Size

- `src/` — 28 TypeScript files, 188,207 bytes total (~184 KB), computed from the GitHub tree API's blob `size` fields. (git/trees API, HEAD)
- Largest files: `src/show.ts` (32,344 bytes), `src/summarizer.ts` (21,685 bytes), `src/parser.ts` (16,415 bytes), `src/indexer.ts` (14,089 bytes), `src/search.ts` (14,311 bytes).
- `dist/` (committed, compiled+bundled output) is separate; `dist/mcp-server.js` alone is 1,017,711 bytes because it's an esbuild bundle of all runtime deps.

## 6. Activity and risk (verified 2026-08-21)

- Commits since 2026-05-23 (last 90 days): 0 — consistent with the last push being 2026-05-21T21:30:30Z, just before that window. (`gh api repos/obra/episodic-memory/commits?since=2026-05-23`)
- Open issues: 58.
- 5 most recent open issues by number:

| # | Title |
|---|---|
| 154 | fix: replace archive mounts with bounded rclone transport |
| 153 | search-conversations agent has invalid YAML frontmatter, and claude plugin validate reports green |
| 152 | sync path re-embeds every exchange of every copied transcript: #84's high-water mark was never ported to sync.ts, and copyIfNewer() never converges on live transcripts |
| 150 | fix(install): verify the native binding loads instead of trusting exit codes (#100, #105) |
| 149 | fix: stop summarizer subprocesses from launching the user's MCP servers |

- Flagged titles (data loss / git side effects / runaway-process language, quoted verbatim, not characterized further): **#152** "sync path re-embeds every exchange of every copied transcript: #84's high-water mark was never ported to sync.ts, and copyIfNewer() never converges on live transcripts"; **#149** "fix: stop summarizer subprocesses from launching the user's MCP servers".

## 7. What it does NOT do

- No curated facts/rules store — `exchanges`, `tool_calls`, and `vec_exchanges` are the only tables; there is no separate table for standing facts, preferences, or rules. (src/db.ts)
- No session-start injection of standing preferences into context — not found in the files I read; the only `SessionStart` hook runs `sync --background` and does not print additional context. (hooks/hooks.json)
- No write/append MCP tool — the MCP server exposes only `search` and `read`; there is no "remember this" or insert-style tool. (src/mcp-server.ts)
- No importance/tag/priority ranking on stored exchanges — not found in the `exchanges` schema I read; ranking at search time is by vector similarity or text match only. (src/db.ts, src/mcp-server.ts)
- No cloud/hosted embedding calls for search or storage — embeddings are local-only (Transformers.js); only summarization optionally calls out to a model. (src/embeddings.ts, README.md "What's Affected" table)

## 8. Ways it could fit logan-mem

- **Install as-is, alongside logan-mem** — add it as a second, independent plugin so logan-mem doesn't need to build transcript parsing, local embeddings, or a search MCP server; logan-mem stays focused on curated facts/rules while this repo covers raw-conversation recall.
- **Borrow the concept, not the code** — build logan-mem's own JSONL-transcript indexer + local-embedding + SQLite/`sqlite-vec` search path, shaped around logan-mem's own memory model instead of a generic "exchange," and its own summarization policy.
- **Vendor specific modules** — reuse `src/parser.ts` (transcript parsing for both Claude Code and Codex JSONL) and/or `src/embeddings.ts` (the local BGE embedding pipeline) inside logan-mem, while replacing the MCP surface, hook wiring, and summarizer with logan-mem's own.
