---
title: Spine v1 — design for the first logan-spine-mcp tweaks and the Claude Code plugin
type: spec
status: draft
created: "2026-08-21 16:36 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [spine/LOGAN-CHANGES.md decisions table; code reads of spine/src and spine/internal/lsm cited inline as file:line (two read-only mapping passes, 2026-08-21); Claude Code docs via claude-code-docs MCP (/en/hooks.mdx, /en/plugins-reference.mdx, /en/plugin-marketplaces.mdx) read 2026-08-21]
---

# Spine v1 design

## What this is

- Version 01 of logan-mem is **the spine**: a code map an agent queries instead of re-reading files. The engine is `logan-spine-mcp`, our renamed vendored copy of DeusData/codebase-memory-mcp v0.10.8 at `spine/` (see `spine/LOGAN-CHANGES.md`).
- This spec covers the first set of tweaks to that engine plus the thin Claude Code plugin that installs and enforces it. The memory system is a later, separate build.
- Guiding constraint from the owner: do not over-engineer. Every item below is the smallest change that makes the decided behaviour real. Where a simpler mechanism already exists upstream, we use it instead of adding one.

## Decisions this spec implements (already made, not re-opened)

| # | Decision | Source |
|---|---|---|
| A1 | Installer scope: keep upstream code; our wrapper always passes `--clients=claude`. | `spine/LOGAN-CHANGES.md` |
| A2 | Auto-index must work out of the box. | `spine/LOGAN-CHANGES.md` |
| A3 | Hook deadline 2000 ms → 3000 ms. | `spine/LOGAN-CHANGES.md` |
| A4 | A `PostToolUse` hook on Edit/Write reports missing docstrings in the file just changed; the same hook enforces B5. | `spine/LOGAN-CHANGES.md` |
| B5 | Capture a file's leading comment as the file-level docstring; expose it through the query tools. | `spine/LOGAN-CHANGES.md` |
| B6 | Markdown sections go into the semantic index; a `DOCUMENTS` edge links a section to the code symbols it names. | chat, 2026-08-21 |
| — | Coverage report: exported symbols only by default, flag for all. | chat, 2026-08-21 |

## Components

| Component | Where | Inside `spine/`? | Purpose |
|---|---|---|---|
| `docstrings` subcommand | `spine/src/cli/docstrings.c` (new), dispatch in `spine/src/main.c` | yes | Parse files with the existing single-file extractor and list the file header and definitions that have no docstring. Used by the hook and the coverage report. |
| File-level docstring (B5) | `spine/internal/lsm/extract_defs.c`, `spine/internal/lsm/lsm.h` | yes | Leading comment → `docstring` on the per-file Module node. |
| Markdown sections + `DOCUMENTS` edge (B6) | `spine/internal/lsm/extract_defs.c`, `spine/src/pipeline/pass_documents.c` (new), `spine/src/pipeline/pass_semantic_edges.c`, `spine/src/foundation/constants.h` | yes | Section body stored and embedded; edge from section to named symbols. |
| Hook deadline (A3) | `spine/src/cli/hook_augment.c:62` | yes | One constant. |
| Plugin `logan-spine` | `plugin/` at repo root | no | `hooks/hooks.json` (the A4 hook), `scripts/docstring-check.sh`, `scripts/docstring-coverage.sh`, `scripts/install.sh`, `.claude-plugin/plugin.json`. |
| Marketplace manifest | `.claude-plugin/marketplace.json` at repo root | no | Lets `claude plugin marketplace add <repo path>` find the plugin. |

## A1 + A2 — install wrapper

- `plugin/scripts/install.sh`, run from a clone of this repo on each machine (macOS/Linux; Windows is out of scope for v1):
  1. `spine/scripts/build.sh --version "$(git describe --tags)"` unless `--no-build` is passed and `spine/build/c/logan-spine-mcp` exists. The stamp reaches the binary through `-DLSM_VERSION` (`spine/scripts/build.sh:138` → `spine/src/main.c:97-98`), fixed by the rename correction recorded in `spine/LOGAN-CHANGES.md` row 2.
  2. Copy the binary to `~/.local/bin/logan-spine-mcp`; warn if that dir is not on `PATH`.
  3. `logan-spine-mcp install --clients=claude -y`. Verified writes (`spine/src/cli/cli.c:7568-7738`): skill `~/.claude/skills/logan-spine/SKILL.md`, three subagents under `~/.claude/agents/`, the MCP entry in `~/.claude.json`, three hook scripts under `~/.claude/hooks/`, and `PreToolUse` (`Grep|Glob`), `PostToolUse` (`Read`), `SessionStart`, `SubagentStart` entries in `~/.claude/settings.json` with a 5 s timeout each.
  4. `logan-spine-mcp config set auto_index true` (A2). Default is `false` (`spine/src/mcp/mcp.c:11619`, `spine/src/cli/cli.c:6752-6759`); `config set` persists to `~/.cache/logan-spine-mcp/_config.db` (`spine/src/cli/cli.c:6603-6612`). `auto_watch` already defaults to `true` (`spine/src/mcp/mcp.c:11501`). The code default is not changed.
  5. `claude plugin marketplace add <repo root>` then `claude plugin install logan-spine@logan-mem`. Both are documented non-interactive CLI commands (`/en/plugin-marketplaces.mdx:1069` and `:755`).
- `--dry-run` prints the steps and runs upstream's `install --dry-run` (`spine/src/cli/cli.c:10002`).
- Uninstall is upstream's `uninstall` plus `claude plugin uninstall logan-spine`. No custom uninstall script.

## A3 — hook deadline

- `spine/src/cli/hook_augment.c:62`: `HA_DEADLINE_DEFAULT_MS` 2000 → 3000. This is inside the 5 s outer timeout upstream writes into `settings.json` (`spine/src/cli/cli.c:4085`). No test asserts the literal 2000 (`spine/tests/test_cli.c:11601` sets its own value via env). Env override `LSM_HOOK_DEADLINE_MS` and the breadcrumb log at `~/.cache/logan-spine-mcp/logs/hook-augment-timeouts.log` (`hook_augment.c:100-124`) are untouched.

## `docstrings` subcommand (serves A4, B5 enforcement, and the coverage report)

- Invocation: `logan-spine-mcp docstrings [--all] [--json] <file>...`. Files only; the coverage script supplies the file list. Dispatched beside `cli`/`install` in `spine/src/main.c:1056-1075`.
- Per file: detect language with `lsm_language_for_filename` (`spine/src/discover/language.c:880`), run `lsm_extract_file` (`spine/internal/lsm/lsm.h:637`), then report:
  1. `file` — `LSMFileResult.file_docstring` (new field, B5) is NULL.
  2. one line per definition in `LSMFileResult.defs` whose `docstring` is NULL and whose label is Function, Method, or one of the type-like labels Class, Struct, Interface, Enum, Type, Trait (`spine/src/foundation/constants.h:108-110`). Default: only definitions with `is_exported` set (`spine/internal/lsm/lsm.h:217`). `--all` includes the rest. Plan step: confirm what `is_exported` means for Python (no export keyword) before relying on it; if it is always false there, Python falls back to "name does not start with `_`".
- Output: one line per finding, `path:line kind name` (kind is `file`, `function`, `class`, `method`, …). `--json` emits the same as a JSON array of `{path,line,kind,name}`. Exit 0 when nothing is missing, 1 when something is, 2 on a usage error or an unreadable file.
- Files whose language the extractor does not support are skipped silently. A file that parses but yields nothing is "complete".
- No indexing, no daemon, no database. It reads the file from disk, so it sees the just-written content the hook cares about.

## A4 — the enforcement hook (plugin)

- `plugin/hooks/hooks.json`: event `PostToolUse`, matcher `Edit|Write|MultiEdit`, command `"${CLAUDE_PLUGIN_ROOT}/scripts/docstring-check.sh"`, timeout 10.
- `docstring-check.sh`: read `tool_input.file_path` from the stdin JSON; if `logan-spine-mcp` is on `PATH` and the file exists, run `logan-spine-mcp docstrings "$file"`. On exit 1, print its output to stderr under one header line (`logan-spine: add docstrings before moving on:`) and exit 2, which surfaces the stderr to Claude next to the tool result (`/en/hooks.mdx:774`, `:842`). Otherwise exit 0 with no output.
- It nudges; it does not block. Blocking would need `PreToolUse` and a parse of content not yet on disk — not worth it.
- Non-goals: docstring quality, staleness. Presence only.

## Coverage report (plugin)

- `plugin/scripts/docstring-coverage.sh [--all] [dir]`: `git -C dir ls-files -z | xargs -0 logan-spine-mcp docstrings [--all]`, then a per-top-level-directory count of findings followed by the full list. Exported-only by default. Exit code passes through.

## B5 — file-level docstring

### Rule, per language

| Language | What counts as the file docstring |
|---|---|
| JS / TS / JSX / TSX (`LSM_LANG_JAVASCRIPT`, `_TYPESCRIPT`, `_TSX`) | The first top-level comment node if it contains `@file` or `@fileoverview`; otherwise the first top-level comment node that appears before any non-comment node **and** whose end row is at least two rows above the next node's start row (a blank line separates it, so a definition's own doc comment is not taken). |
| Python | The module docstring: root `module` → first named child is `expression_statement` → first named child is `string`/`concatenated_string` (the same shape `extract_python_docstring` checks on a function body at `spine/internal/lsm/extract_defs.c:1247-1267`, applied to `ctx->root`). |
| Go | The comment node that is the previous sibling of `package_clause`. |
| Every other language with `is_comment_node` kinds (`extract_defs.c:1210-1213`) | The generic JS rule without the `@file` check. |
| Markdown | Not applicable; B6 covers it. |

### Data

- `LSMFileResult` (`spine/internal/lsm/lsm.h:472-489`) gains `const char *file_docstring` (arena-owned, NULL when absent), set by a new `extract_file_docstring(LSMExtractCtx *ctx)` called once from `lsm_extract_definitions` (`extract_defs.c:7439`). Text goes through `extract_comment_text` (`extract_defs.c:1217`), so it is capped at `MAX_COMMENT_LEN` 500 like every other docstring.
- The value is stored on the **Module** definition that every file already emits (`extract_defs.c:7452-7463`, label `Module`, docstring currently never set). Setting `def.docstring` there means the existing `build_def_props` path (`spine/src/pipeline/pass_definitions.c:285`, and its duplicates in `pass_parallel.c:474` and `pipeline_incremental.c`) writes it with no change to any of the three kept-in-sync copies. The File node (`{"extension"}` only, `spine/src/pipeline/pipeline.c:654-658`) is not touched; `qualified_name` differs only by the extension (`spine/src/pipeline/fqn.c:113-143`), and `file_path` is identical, so a reader that wants the File node can find it.

### Exposure

- `search_graph` with `label:"Module"` and `fields:["docstring"]` returns it with no code change — extra fields are read generically from the properties JSON (`spine/src/mcp/mcp.c:3349-3361`).
- `semantic_query` (a parameter of `search_graph`, `mcp.c:442-447`) returns Module nodes once B6's corpus change lands (below); Module nodes are embedded only when they have a docstring.
- `get_code_snippet` already accepts any qualified name (`mcp.c:8734`, `spine/src/store/store.c:1849`).
- `get_architecture` is not changed. Its `file_tree` aspect reads only `file_path` (`store.c:6713`); adding descriptions there is a later nicety, not v1.

## B6 — Markdown in the semantic index and `DOCUMENTS` edges

### Today (verified)

- `.md`/`.mdx` map to `LSM_LANG_MARKDOWN` (`spine/src/discover/language.c:179-180`). Each file emits one Module node and one `Section` node per heading (`extract_defs.c:4008-4014` via `push_simple_class_def`, `:3828-3840`), with `start_line`/`end_line` equal to the heading's own span and no body text stored.
- Only the block grammar of tree-sitter-markdown is vendored (`spine/vendored/grammars/MANIFEST.md:143`): `code_span` and `link` are not AST nodes, so backticks and links must be found by scanning text.
- The semantic corpus collects `{"Function","Method"}` only (`spine/src/pipeline/pass_semantic_edges.c:957`), tokenizes `name`, `qualified_name`, `file_path` plus the `signature`, `return_type`, `docstring`, `param_names`, `param_types`, `decorators`, `bt` properties (`:465-498`) under `LSM_SEM_MAX_TOKENS` 512 (`spine/src/semantic/semantic.h:93`), and `lsm_store_vector_search` filters results to `LSM_SQL_CALLABLE_OR_TYPE_LABELS` (`spine/src/foundation/constants.h:108-110`, used at `store.c:8580-8587`). Semantic results carry `qn, label, file, score` only (`mcp.c:3194-3214`).
- No edge from a doc to code exists. Edge types are free strings; there is no allowlist (`store.c:264-274`).

### Change

1. **Section body as docstring.** In the Markdown branch of `extract_config_class_def` (`extract_defs.c:4008-4014`): set the Section def's `end_line` to the line before the next heading of equal or higher level (or EOF), and set `def.docstring` to the source text of that range, trimmed, truncated to 2,000 bytes on a UTF-8 boundary. This bypasses `MAX_COMMENT_LEN` deliberately — prose sections are longer than code comments and the 512-token embedding cap already bounds the cost.
2. **Corpus.** Add `"Section"` and `"Module"` to the label array at `pass_semantic_edges.c:957`; skip nodes whose properties lack a non-empty `docstring` so files without a header do not add empty vectors. Add `'Section','Module'` to `LSM_SQL_CALLABLE_OR_TYPE_LABELS`' use in `lsm_store_vector_search` (a second macro, `LSM_SQL_SEMANTIC_LABELS`, so the other users of the original macro are unaffected). `SEMANTICALLY_RELATED` edges will now also be emitted between sections and functions that score ≥ threshold; that is wanted, not suppressed.
3. **`DOCUMENTS` edges.** New pass `spine/src/pipeline/pass_documents.c`, run after definitions are registered (after `pass_definitions`/`pass_parallel` and before `pass_semantic_edges` in `spine/src/pipeline/pipeline.c`'s pass list). For each Section node in the graph buffer: scan the *full* section text (re-read from the file, not the truncated docstring) for `` `token` `` spans. For a token with no `/` and no file extension: `lsm_registry_find_by_name` (`spine/src/pipeline/registry.c:609`) on the last `.`/`::` segment; link to every match when there are ≤ 5, none otherwise. For a token that looks like a path (`/` or a known source extension): link to the File node whose `file_path` ends with it (`lsm_gbuf_find_by_name` on the basename, then suffix check). Emit with `lsm_gbuf_insert_edge(gbuf, section_id, target_id, "DOCUMENTS", "{}")` (`spine/src/graph_buffer/graph_buffer.h:124`).
4. **Query.** `trace_path` inbound on `DOCUMENTS` answers "which docs mention X"; `search_graph` `label:"Section"` with `fields:["docstring"]` returns section text. No new MCP tool.

### Known limitation, accepted for v1

- The `DOCUMENTS` pass runs in the full pipeline only. An incremental re-index of an edited Markdown file (what the watcher does) refreshes its Section nodes and docstrings but not its `DOCUMENTS` edges until the next full index. Noted in `spine/LOGAN-CHANGES.md`; revisit if it bites.

### Not in scope

- Links to URLs, images, or other Markdown files; prose matching without backticks; relabeling Section nodes; BM25 keyword search over section text (the keyword path excludes `Section` at `mcp.c:3028`, and semantic search is the stated channel).

## Error handling

- `docstrings`: an unreadable file is exit 2 with one stderr line. A file that fails to parse is reported as `path:0 error <reason>` and does not change the exit code of the other files.
- Hook: any failure inside the script (binary missing, non-zero exit other than 1) exits 0 with nothing on stdout or stderr. The hook must never make an edit look failed.
- B5/B6 extraction: a missing or odd comment yields NULL / no body / no edge. The index never fails because of a comment.

## Testing

- Extraction (`spine/tests/test_extraction.c`, suite `extraction`, helper `extract()` at `:78-82`): new tests for the JS `@file` rule, the JS blank-line rule, the Python module docstring, the Go package comment, and a Markdown section body with `end_line`. Registered already via `Makefile.lsm:514-522`.
- Pipeline (`spine/tests/test_pipeline.c`, helper `setup_lang_repo` at `:5693`): one end-to-end test asserting a Module node's `properties_json` contains `docstring`, and one asserting a `DOCUMENTS` edge from a Section to a function named in backticks, plus the ≤ 5 ambiguity rule.
- Semantic: one test in the semantic/pipeline suite asserting a Section node receives a `node_vectors` row and appears in `semantic_query` results.
- CLI (`spine/tests/test_cli.c`, suite `cli`): `docstrings` on three fixtures (JS with `@file`, Python module docstring, Go package comment) asserting output lines, `--all`, `--json`, and exit codes.
- Plugin scripts: a shell test under `plugin/tests/` that pipes a fake `PostToolUse` JSON into `docstring-check.sh` and asserts exit 2 plus stderr for a fixture missing docstrings, exit 0 for a complete one.
- End-to-end before calling v1 done: run `plugin/scripts/install.sh` on this machine, index this repo, `search_graph label:"Module" fields:["docstring"]`, `trace_path` inbound on `DOCUMENTS` for a symbol named in `docs/`, and edit a file without docstrings in a live session to see the nudge.

## Versioning and upstream

- Every change inside `spine/` gets a row in `spine/LOGAN-CHANGES.md`; `.githooks/pre-commit` enforces it once `git config core.hooksPath .githooks` is set per clone.
- Tag `v0.10.8-logan.2` when this spec's work lands; `plugin/.claude-plugin/plugin.json` carries the same version string.
- Upstream pull procedure is unchanged (run `spine/scripts/logan-rename.sh` on upstream's tree first). New files (`docstrings.c`, `pass_documents.c`) never conflict; the edits to `extract_defs.c`, `pass_semantic_edges.c`, `constants.h`, `hook_augment.c`, `main.c`, and `pipeline.c` are the conflict surface, all small.
