---
title: Spine v1 — design for the first logan-spine-mcp tweaks and the Claude Code plugin
type: spec
status: draft
created: "2026-08-21 16:36 CDT"
updated: "2026-08-21 17:40 CDT"
version: "01"
sources: [spine/LOGAN-CHANGES.md decisions table; code reads of spine/src and spine/internal/lsm cited inline as file:line (two mapping passes and three adversarial review passes, 2026-08-21); Claude Code docs via claude-code-docs MCP (/en/hooks.mdx, /en/plugins-reference.mdx, /en/plugin-marketplaces.mdx, /en/mcp.mdx) read 2026-08-21]
---

# Spine v1 design

## What this is

- Version 01 of logan-mem is **the spine**: a code map an agent queries instead of re-reading files. The engine is `logan-spine-mcp`, our renamed vendored copy of DeusData/codebase-memory-mcp v0.10.8 at `spine/` (see `spine/LOGAN-CHANGES.md`).
- This spec covers the first set of tweaks to that engine plus the thin Claude Code plugin that installs and enforces it. The memory system is a later, separate build.
- Guiding constraint from the owner: do not over-engineer. Every item below is the smallest change that makes the decided behaviour real. Where upstream already has a mechanism, we use it.

## Decisions this spec implements (already made, not re-opened)

| # | Decision | Source |
|---|---|---|
| A1 | Installer scope: keep upstream code; our wrapper always passes `--clients=claude`. | `spine/LOGAN-CHANGES.md` |
| A2 | Auto-index must work out of the box. | `spine/LOGAN-CHANGES.md` |
| A3 | Hook deadline 2000 ms → 3000 ms. | `spine/LOGAN-CHANGES.md` |
| A4 | A `PostToolUse` hook on Edit/Write reports missing docstrings in the file just changed; the same hook enforces B5. | `spine/LOGAN-CHANGES.md` |
| B5 | Capture a file's leading comment as the file-level docstring; expose it through the query tools. | `spine/LOGAN-CHANGES.md` |
| B6 | Markdown sections go into the semantic index; a `DOCUMENTS` edge links a section to the code symbols it names in backticks. | chat, 2026-08-21 |
| — | Coverage report: exported symbols only by default, flag for all. | chat, 2026-08-21 |

## Components

| Component | Where | Inside `spine/`? | Purpose |
|---|---|---|---|
| `docstrings` subcommand | `spine/src/cli/docstrings.c` (new), dispatch in `spine/src/main.c` | yes | Parse files with the existing single-file extractor and list the file header and definitions that have no docstring. Used by the hook and the coverage report. |
| File-level docstring (B5) | `spine/internal/lsm/extract_defs.c`, `spine/internal/lsm/lsm.h` | yes | Leading comment → `docstring` on the per-file Module node. |
| Markdown sections + `DOCUMENTS` edge (B6) | `spine/internal/lsm/extract_defs.c`, `spine/src/pipeline/pass_documents.c` (new), `spine/src/pipeline/pipeline.c`, `spine/src/pipeline/pass_semantic_edges.c`, `spine/src/pipeline/pass_definitions.c`, `spine/src/pipeline/pass_parallel.c`, `spine/src/store/store.c` | yes | Section body stored and embedded; edge from section to named symbols. |
| Hook deadline (A3) | `spine/src/cli/hook_augment.c:62` | yes | One constant. |
| Plugin `logan-spine-tools` | `plugin/` at repo root, copied to `~/.claude/skills/logan-spine-tools/` by the installer | no | `.claude-plugin/plugin.json`, `hooks/hooks.json` (the A4 hook), `scripts/docstring-check.sh`, `scripts/docstring-coverage.sh`, `scripts/install.sh`. |

## Packaging decision: a skills-directory plugin, not a marketplace

- Any folder under `~/.claude/skills/` that contains `.claude-plugin/plugin.json` loads as a plugin named `<folder>@skills-dir` on the next session, with no marketplace, no install command, and no cache copy (`/en/plugins-reference.mdx:354-372`). `install.sh` copies `plugin/` to `~/.claude/skills/logan-spine-tools/`. That is the whole distribution mechanism; teammates clone the repo and run the script.
- Why not a marketplace: a marketplace install copies the plugin into `~/.claude/plugins/cache` (`/en/plugins-reference.mdx:753`), so script edits in the repo go stale until reinstall, and `claude plugin marketplace add <path>` records an absolute local path under `~/.claude`, which the owner syncs between two machines with different clone paths.
- The folder is named `logan-spine-tools`, not `logan-spine`, because upstream's installer writes a plain skill to `~/.claude/skills/logan-spine/SKILL.md` (`spine/src/cli/cli.c:1415,1463`); dropping a `plugin.json` into that folder would turn upstream's skill into our plugin's root skill and put it in the path upstream rewrites on every `install`.
- **The MCP server is not bundled in the plugin.** Plugin-bundled servers get their tools renamed `mcp__plugin_<plugin>_<server>__<tool>` (`/en/mcp.mdx:454-462`), while upstream's generated skill and three subagents hard-code the unscoped names `mcp__logan-spine-mcp__search_graph` etc. (22 occurrences, `spine/src/cli/cli.c:3011-3020` and `spine/src/cli/agent_profiles.c`). Upstream's installer keeps writing the server into `~/.claude.json`. A later reader must not "simplify" this.
- `plugin.json` carries `name` and `description` only. No `version`: skills-dir plugins are not versioned by that field, and the repo tag is the version of record.

## A1 + A2 — install wrapper

- `plugin/scripts/install.sh`, run from a clone of this repo on each machine (macOS/Linux; Windows is out of scope for v1). `ROOT="$(cd "$(dirname "$0")/../.." && pwd)"`, the same idiom as `spine/scripts/build.sh:17`. Steps:
  1. `"$ROOT/spine/scripts/build.sh" --version "$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)"`. The stamp reaches the binary through `-DLSM_VERSION` (`spine/scripts/build.sh:138` → `spine/src/main.c:97-98`); verified 2026-08-21: `logan-spine-mcp --version` prints `logan-spine-mcp 0.10.8-logan.2` after a stamped build. `build.sh` is ccache-backed (`build.sh:24-29`), so there is no skip-build flag.
  2. Copy `spine/build/c/logan-spine-mcp` to `~/.local/bin/logan-spine-mcp` (mkdir -p; warn if `~/.local/bin` is not on `PATH`).
  3. `logan-spine-mcp install --clients=claude -y`. Verified writes (`spine/src/cli/cli.c:7568-7738`): skill `~/.claude/skills/logan-spine/SKILL.md`, three subagents `~/.claude/agents/logan-spine{-scout,,-auditor}.md`, the MCP entry in `~/.claude.json`, three hook scripts under `~/.claude/hooks/`, and `PreToolUse` (`Grep|Glob`), `PostToolUse` (`Read`), `SessionStart`, `SubagentStart` entries in `~/.claude/settings.json`, each with `timeout: 5`.
  4. `logan-spine-mcp config set auto_index true` (A2). Default is `false` (`spine/src/cli/cli.c:6753`); `config set` persists to `~/.cache/logan-spine-mcp/_config.db` (`cli.c:6603-6612`); the MCP server reads it in `maybe_auto_index` on `initialize` (`spine/src/mcp/mcp.c:11619,11726`) and the daemon reads it at `spine/src/daemon/application.c:1936`, both indexing a project that has no DB yet. `auto_watch` already defaults to `true` (`mcp.c:11501`). The code default is not changed.
  5. `rm -rf ~/.claude/skills/logan-spine-tools && cp -r "$ROOT/plugin" ~/.claude/skills/logan-spine-tools`. No `claude` CLI call is needed; the plugin loads on the next session (or after `/reload-plugins`, `/en/plugins-reference.mdx:385`).
- Uninstall: `logan-spine-mcp uninstall` plus `rm -rf ~/.claude/skills/logan-spine-tools`. No custom script. Upstream's `install --dry-run` (`cli.c:10002`) is available directly for previews; the wrapper has no dry-run flag.
- Open question for the owner, recorded here and not assumed: is `~/.local/bin` in the Mutagen sync set? If it is, a 280 MB binary syncs on every rebuild and is unrunnable on the other platform; the binary should then go somewhere unsynced (e.g. `/usr/local/bin` or `~/bin-local/`). The script takes the destination from `LSM_BIN_DIR` (default `~/.local/bin`) so the answer changes one environment variable, not the script.

## A3 — hook deadline

- `spine/src/cli/hook_augment.c:62`: `HA_DEADLINE_DEFAULT_MS` 2000 → 3000. This sits inside the 5 s outer timeout upstream writes into `settings.json` (`spine/src/cli/cli.c:4085`). No test asserts the literal 2000 (`spine/tests/test_cli.c:11601` sets its own value via env). Env override `LSM_HOOK_DEADLINE_MS` and the breadcrumb log at `~/.cache/logan-spine-mcp/logs/hook-augment-timeouts.log` (`hook_augment.c:100-124`) are untouched.

## `docstrings` subcommand (serves A4, B5 enforcement, and the coverage report)

- Invocation: `logan-spine-mcp docstrings [--all] <file>...`. Files only; the coverage script supplies the list. Dispatched beside `cli`/`install` in `spine/src/main.c:1056-1075`. No JSON flag — nothing consumes JSON.
- Per file: `lsm_language_for_filename` (`spine/src/discover/language.c:880`; returns `LSM_LANG_COUNT` for unknown → skip silently), then `lsm_extract_file` (`spine/internal/lsm/lsm.h:637`; callable standalone — it allocates its own arena and resolves the grammar lazily, as `spine/tests/test_extraction.c:78-82` shows). Report:
  1. `path:1 file <path>` when `LSMFileResult.file_docstring` (new field, B5) is NULL and the language has a B5 rule.
  2. `path:<start_line> <kind> <name>` for each definition whose `docstring` is NULL and whose label is Function, Method, Class, Struct, Interface, Enum, Type, or Trait. `kind` is the label lowercased.
- "Exported" (the default filter; `--all` disables it), per what the extractor actually records:
  1. Go, Python, Java, C#, Kotlin: `is_exported` (`spine/internal/lsm/helpers.c:238-253`; Python is `name[0] != '_'` at `:246`).
  2. JS, TS, JSX, TSX: functions use `is_entry_point`, which `is_js_exported` sets on the `export` keyword (`spine/internal/lsm/extract_defs.c:3768-3774`); classes carry no export signal and are always reported.
  3. Every other language: `lsm_is_exported` returns `true` (`helpers.c` `default:`), so the default and `--all` are identical. `--help` says so in one line.
- Exit 0 when nothing is missing, 1 when something is, 2 on a usage error or an unreadable file. A file that parses but yields nothing is complete. No indexing, no daemon, no database: it reads the file from disk, so the hook sees just-written content.

## A4 — the enforcement hook (plugin)

- `plugin/hooks/hooks.json`, literal (`/en/hooks.mdx:620-640`, `/en/plugins-reference.mdx:72-93`):

```json
{
  "description": "logan-spine: nudge for missing docstrings after Edit/Write",
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Edit|Write",
        "hooks": [
          { "type": "command", "command": "\"${CLAUDE_PLUGIN_ROOT}\"/scripts/docstring-check.sh", "timeout": 10 }
        ]
      }
    ]
  }
}
```

- `Edit|Write` is the exact-string list form (letters and `|` only, `/en/hooks.mdx:285-287`), so it fires on exactly those two tools and not on `NotebookEdit`. `MultiEdit` is not a current tool. `timeout` is seconds (`/en/hooks.mdx:418`).
- `docstring-check.sh`: read `tool_input.file_path` from stdin JSON (absolute, per the PostToolUse input schema). If `logan-spine-mcp` is not on `PATH`, or the path is not a readable regular file, exit 0 silently. Otherwise run `logan-spine-mcp docstrings "$file"`. On exit 1: print to stderr one header line `logan-spine: add docstrings before moving on:` followed by the first 10 finding lines and, if there were more, `… and N more`, then exit 2 — stderr is shown to Claude next to the tool result and the tool is not marked failed (`/en/hooks.mdx:774`, `:842`). Any other exit code from the subcommand: exit 0 silently. No project-membership check: the subcommand needs no index and skips unknown languages itself.
- It nudges; it does not block. Presence only — not quality, not staleness.

## Coverage report (plugin)

- `plugin/scripts/docstring-coverage.sh [--all] [dir]`: `git -C "${dir:-.}" ls-files -z | xargs -0 logan-spine-mcp docstrings ${all:+--all}`. Exit code passes through. Anyone wanting a per-directory breakdown pipes to `cut -d/ -f1 | sort | uniq -c`.

## B5 — file-level docstring

### Rule, per language

| Language | What counts as the file docstring |
|---|---|
| JS / TS / JSX / TSX | Consider only the run of comment nodes at the very start of `ctx->root` (before any non-comment node). If any of them contains `@file` or `@fileoverview`, that one. Otherwise the first comment of the run, provided `next_non_comment.start_point.row - last_comment_of_run.end_point.row >= 2` (at least one wholly blank line separates the run from the first real node, so a definition's own doc comment is not taken). |
| Python | Root `module` → first named child is `expression_statement` → its first named child is `string` or `concatenated_string` (the shape `extract_python_docstring` checks at `extract_defs.c:1247-1267`, applied to `ctx->root`). |
| Go | The comment node that is the previous sibling of `package_clause` (node name verified in the vendored grammar). |
| Every other language whose grammar emits `is_comment_node` kinds (`extract_defs.c:1210-1213`) | The JS rule without the `@file` check. |
| Markdown | Not applicable; B6 covers it. |

- Text goes through `extract_comment_text` (`extract_defs.c:1217`), so it is capped at `MAX_COMMENT_LEN` 500 like every other docstring.

### Data

- `LSMFileResult` (`lsm.h:472-489`) gains `const char *file_docstring` (arena-owned, NULL when absent), set by a new `extract_file_docstring(LSMExtractCtx *ctx)` (`ctx->root`, `ctx->source`, `ctx->language` all exist, `lsm.h:575-594`) called from `lsm_extract_definitions` (`extract_defs.c:7439`) before the Module def is built.
- The same value is set as `docstring` on the **Module** definition every file emits (`extract_defs.c:7447-7458`, label `Module`, docstring currently never set; it is pushed first, before `walk_defs`). `build_def_props` then writes it with no change to either of its two copies (`spine/src/pipeline/pass_definitions.c:285` and `pass_parallel.c:474`; `pipeline_incremental.c:1388` re-enters `pass_definitions`).
- **Known limitation, accepted for v1:** Go and Java have folder-level Modules, not per-file ones (`helpers.c:1461`: `lsm_lang_module_is_dir` is `JAVA || GO`), and the graph buffer keeps the existing Folder node on that QN collision (`spine/src/graph_buffer/graph_buffer.c:666-669`). For those two languages the file docstring is visible through the `docstrings` subcommand and the hook but not in the graph. Putting it on the File node instead would need an in-place property append in both definition paths; not worth it until Go or Java is actually in use.

### Exposure

- `search_graph` with `label:"Module"` and `fields:["docstring"]` returns it with no code change — extra fields are read from the properties JSON generically (`spine/src/mcp/mcp.c:3349-3361`).
- Semantic search returns Module nodes once B6's corpus change lands (below).
- `get_code_snippet` accepts any qualified name (`mcp.c:8734`, `spine/src/store/store.c:1849`).
- Not changed: `get_architecture` (its `file_tree` reads only `file_path`, `store.c:6713`) and BM25 keyword search, which excludes `Module` and `Section` labels (`mcp.c:3028`). Semantic search is the channel for both.

## B6 — Markdown in the semantic index and `DOCUMENTS` edges

### Today (verified)

- `.md`/`.mdx` → `LSM_LANG_MARKDOWN` (`spine/src/discover/language.c:179-180`). Each file emits one Module node and one `Section` node per heading (`extract_defs.c:4009-4014` via `push_simple_class_def`, `:3828-3840`, shared with TOML/INI/XML/HCL), with `start_line`/`end_line` equal to the heading's own span, no body text.
- Only the block grammar of tree-sitter-markdown is vendored (`spine/internal/lsm/vendored/grammars/MANIFEST.md:143`): `code_span` and `link` are not AST nodes. The grammar does emit nested, named `section` nodes (`grammars/markdown/parser.c:130`), and the heading's parent `section` spans exactly to the next heading of equal or higher level.
- The semantic corpus collects `{"Function","Method"}` only (`spine/src/pipeline/pass_semantic_edges.c:957`), tokenizes `name`, `qualified_name`, `file_path` plus the `signature`, `return_type`, `docstring`, `param_names`, `param_types`, `decorators`, `bt` properties (`:465-498`) under `LSM_SEM_MAX_TOKENS` 512 (`spine/src/semantic/semantic.h:93`). Every node in the array gets a `node_vectors` row (`:1182-1207`); none of the tokenizers or scorers fail on a node with no calls, no signature, or an empty docstring (verified `:448-462`, `semantic.c:1572,1613,1632`). `lsm_store_vector_search` filters results by `LSM_SQL_CALLABLE_OR_TYPE_LABELS` (`store.c:8585`).
- `SEMANTICALLY_RELATED` edges are only ever emitted between nodes with identical file extensions (`pass_semantic_edges.c:848-850`), so sections will pair only with other sections. That is fine and is not changed.
- Definition properties are built into a `char props[LSM_SZ_2K]` buffer (`pass_definitions.c:325`, `pass_parallel.c:684`) and `append_json_string` skips a field atomically when it does not fit (`pass_definitions.c:184-186`). A long docstring is dropped silently.

### Change

1. **Section body as docstring.** In the Markdown branch of `extract_config_class_def` (`extract_defs.c:4009-4014`), build and push the `LSMDefinition` inline instead of calling `push_simple_class_def`, so the shared helper's signature stays as is. `end_line` = end row of `ts_node_parent(heading)` (the enclosing `section`). `docstring` = source text from the heading's end to the section's end, leading and trailing ASCII whitespace stripped, interior blank lines kept, truncated to 1,500 bytes on a UTF-8 boundary. Raise the two `props[LSM_SZ_2K]` buffers to `LSM_SZ_4K` (`constants.h:37`) so the escaped body (worst case 2× plus ~82 bytes of base fields) always fits; the pipeline test below asserts a body full of quotes and newlines survives.
2. **Corpus.** Add `"Section"` and `"Module"` to the label array at `pass_semantic_edges.c:957`; for those two labels only, skip nodes whose properties lack a non-empty `docstring`. Function and Method collection is unchanged. Extend the single query site `store.c:8585` inline to `" AND n.label IN (" LSM_SQL_CALLABLE_OR_TYPE_LABELS ",'Section','Module')"`; the macro itself and its other three users are untouched.
3. **`DOCUMENTS` edges.** New file `spine/src/pipeline/pass_documents.c` exposing `predump_documents(lsm_pipeline_ctx_t *ctx)`, registered in `run_predump_passes` (`pipeline.c:941-947`) as `{predump_documents, "documents", false}` before `predump_sem`, with `PREDUMP_PASS_COUNT` bumped 6 → 7. `moderate_only=false`, so it runs in FAST mode too. It runs on both the sequential and the parallel path; `ctx` carries `gbuf`, `project_name`, and `repo_path` (`spine/src/pipeline/pipeline_internal.h:85-90`). For each Section node (`lsm_gbuf_find_by_label`, `graph_buffer.h:77`): re-read its file and take lines `start_line..end_line` (so the scan sees the full body, not the truncated docstring); find every `` `token` `` span; for each distinct token:
   - path-like (contains `/`, or `lsm_language_for_filename(token) != LSM_LANG_COUNT`): candidates are File nodes whose `file_path` ends with the token (`lsm_gbuf_find_by_name` on the basename, `graph_buffer.h:88`, then suffix check);
   - otherwise: candidates are nodes returned by `lsm_gbuf_find_by_name` on the token's last `.`/`::` segment whose label is Function, Method, or one of the type-like labels;
   - link to every candidate when there are ≤ 5, none otherwise (ambiguity is not worth guessing). `lsm_gbuf_insert_edge(gbuf, section_id, target_id, "DOCUMENTS", "{}")` (`graph_buffer.h:124`; edge types are free strings, `store.c:264-274`).
4. **Query.** `trace_path` inbound on `DOCUMENTS` answers "which docs mention X"; `search_graph label:"Section" fields:["docstring"]` returns section text; `semantic_query` returns Section and Module nodes. No new MCP tool.

### Known limitations, accepted for v1

- The `DOCUMENTS` pass runs in the full pipeline only. An incremental re-index of an edited Markdown file refreshes its Section nodes and docstrings but not its `DOCUMENTS` edges until the next full index.
- Two headings with identical text in one file share one qualified name (`extract_defs.c:3831`), so one Section node survives and the other body is lost. Pre-existing; now user-visible.
- BM25 keyword search excludes Section and Module (`mcp.c:3028`); semantic search is the channel.

### Not in scope

- Links to URLs, images, or other Markdown files; prose matching without backticks; relabeling Section nodes; relaxing the same-extension guard on `SEMANTICALLY_RELATED`.

## Error handling

- `docstrings`: an unreadable file is exit 2 with one stderr line. A file that fails to parse is reported as `path:0 error <reason>` and does not change the exit code of the other files.
- Hook: any failure inside the script exits 0 with nothing on stdout or stderr. The hook must never make an edit look failed.
- B5/B6 extraction: a missing or odd comment yields NULL / no body / no edge. The index never fails because of a comment.

## Testing

- Extraction (`spine/tests/test_extraction.c`, suite `extraction`, helper `extract()` at `:78-82`, already in `Makefile.lsm:514-522`): the JS `@file` rule; the JS blank-line rule, positive and negative; the Python module docstring; the Go package comment; a Markdown section whose `end_line` spans to the next same-level heading and whose docstring holds the body.
- Pipeline (`spine/tests/test_pipeline.c`, helper `setup_lang_repo` at `:5693`): a Module node's `properties_json` contains `docstring`; a Section whose body is 1,400 bytes of quotes and newlines still has its `docstring` stored; a `DOCUMENTS` edge from a Section to a function named in backticks; no edge when six functions share the name; a `node_vectors` row exists for a Section and it appears in `semantic_query` results.
- CLI (`spine/tests/test_cli.c`, suite `cli`): `docstrings` on three fixtures (JS with `@file` and one undocumented exported function, Python module docstring with one `_private` and one public undocumented function, Go package comment) asserting lines, `--all`, and exit codes.
- Plugin scripts (`plugin/tests/`, plain bash): pipe a fake `PostToolUse` JSON into `docstring-check.sh` for a fixture missing docstrings → exit 2, header line, ≤ 10 finding lines; for a complete fixture → exit 0, empty; for a `.txt` path → exit 0, empty. `claude plugin validate ./plugin` passes (`/en/plugins.mdx:359`).
- End-to-end before calling v1 done, on this machine: run `plugin/scripts/install.sh`; start a session in this repo; confirm `/hooks` lists the plugin hook; `search_graph label:"Module" fields:["docstring"]`; `trace_path` inbound on `DOCUMENTS` for a symbol named in `docs/`; edit a source file with no docstrings and see the nudge; write a `.txt` file and see nothing; `NotebookEdit` is not matched.

## Versioning and upstream

- Every change inside `spine/` gets a row in `spine/LOGAN-CHANGES.md`; `.githooks/pre-commit` enforces it once `git config core.hooksPath .githooks` is set per clone.
- Tag `v0.10.8-logan.2` when this spec's work lands.
- Upstream pull procedure is unchanged (run `spine/scripts/logan-rename.sh` on upstream's tree first). New files (`docstrings.c`, `pass_documents.c`) never conflict; the edits to `extract_defs.c`, `lsm.h`, `pass_semantic_edges.c`, `pass_definitions.c`, `pass_parallel.c`, `store.c`, `hook_augment.c`, `main.c`, and `pipeline.c` are the conflict surface, each a few lines.
