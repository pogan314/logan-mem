# LOGAN-CHANGES — what this tree changes versus upstream

- Upstream: https://github.com/DeusData/codebase-memory-mcp (MIT)
- Upstream base: tag `v0.10.8` (upstream commit `46ae198`, imported as squash commit `787ab8c` in this repo)
- Our version: `v0.10.8-logan.1`
- Every commit that touches `spine/` must add or update an entry here. A pre-commit hook at `.githooks/pre-commit` enforces it.
- The diff against pristine upstream at any time: `git diff 787ab8c HEAD:spine` (run from the repo root).

## How to pull upstream changes

1. `git fetch upstream tag vX.Y.Z --no-tags`
2. `git checkout -b upstream-renamed FETCH_HEAD` and run `spine/scripts/logan-rename.sh .` on that branch, commit.
3. From `dev/…`: `git subtree pull --prefix=spine --squash upstream-renamed`.
4. Resolve any conflicts (they are only where our tweaks below and upstream touched the same lines), update the base tag above, bump our version.

## Changes

| # | Date | Area | What | Why |
|---|---|---|---|---|
| 1 | 2026-08-21 15:50 CDT | whole tree | Renamed `codebase-memory-mcp` → `logan-spine-mcp` in every file and path, including `CBM_`/`cbm_` → `LSM_`/`lsm_`, `CBMType`-style prefixed identifiers → `LSMType`, `internal/cbm/` → `internal/lsm/`, `.cbmignore` → `.lsmignore`. Done by `scripts/logan-rename.sh`, which is idempotent and is re-run on upstream's tree before every merge. The arXiv paper title "Codebase-Memory: …" is a citation and is left as is. | This is our fork; the binary, MCP server key, cache dir, and config entries should carry our name. |
| 2 | 2026-08-21 16:43 CDT | `scripts/logan-rename.sh`, `Makefile.lsm`, `scripts/build.sh`, tests | Rename fix: the `CBM_`/`cbm_` rules no longer require a word boundary, so `-DCBM_VERSION` and 20 other `-DCBM_*` compile defines in `Makefile.lsm`, plus `_cbm_*` test symbols, are now `LSM_`/`lsm_`. Before this, `build.sh --version` stamped a macro the code never read and test-seam defines were silently off. | The first rename pass missed mid-word matches; verified by rebuilding with `--version`. |
| 3 | 2026-08-21 17:25 CDT | `src/cli/cli.c:798` | `TAR_BINARY_NAME_LEN` was a literal `19` (length of `codebase-memory-mcp`); now `sizeof(TAR_BINARY_NAME) - 1`. | Rename fix: `cli_extract_binary_from_targz` failed because `strncmp` compared 19 bytes of a 15-byte name. With this, the `cli` suite matches pristine upstream on this host (272 passed, 10 failed, same 10). |
| 4 | 2026-08-21 18:14 CDT | `src/cli/hook_augment.c` | `HA_DEADLINE_DEFAULT_MS` 2000 → 3000. | Decision A3: our hardware has headroom inside the 5 s outer hook timeout. |
| 5 | 2026-08-21 18:45 CDT | `internal/lsm/lsm.h`, `internal/lsm/extract_defs.c`, tests | B5: `LSMFileResult.file_docstring` from the leading comment (JS `@file`/`@fileoverview` or blank-line-separated run; Python module docstring; Go package comment), capped at `MAX_COMMENT_LEN` 500; also set as the Module def's `docstring`. | Decision B5. Module def chosen over File node because it already flows through `build_def_props`; Go/Java have folder Modules so the graph does not carry it for them (spec, known limitation). |
| 6 | 2026-08-21 18:45 CDT | `internal/lsm/extract_defs.c`, tests | B5b: `extract_docstring` also looks before an enclosing `export_statement`, so `/** doc */ export function f()` keeps its docstring. | Upstream dropped docstrings on every exported JS/TS function; A4 would nag on documented code. |
| 7 | 2026-08-21 19:18 CDT | `internal/lsm/extract_defs.c`, `src/pipeline/pass_definitions.c`, `src/pipeline/pass_parallel.c`, tests | B6: Markdown Section defs span the enclosing `section` node and carry the trimmed body (≤ 1,500 bytes) as `docstring`; definition props buffer 2K → 4K at both `build_def_props` call sites so the escaped body is never dropped. An H1's section spans the whole document (nested sections). | Decision B6. The 2K buffer skipped the field atomically (`append_json_string`). |
| 8 | 2026-08-21 19:42 CDT | `src/pipeline/pass_semantic_edges.c`, `src/store/store.c`, `src/pipeline/pipeline_internal.h` (comment), tests | B6: Section and Module nodes with a docstring are embedded into `node_vectors`; vector search admits those labels. Same-file Module↔Function pairs can now also gain `SEMANTICALLY_RELATED` edges; cross-extension pairs still cannot. | Decision B6 (semantic search over docs). |

## Decided, not yet built (2026-08-21)

| # | Tweak | Decision | Where |
|---|---|---|---|
| A1 | Installer scope | Keep upstream code; our install wrapper always passes `--clients=claude`. | outside `spine/` |
| A2 | Auto-index | Must work out of the box; wrapper runs `config set auto_index true` after install (or code default flips if that proves unreliable). | wrapper, maybe `src/cli/cli.h` |
| A3 | Hook deadline | Raise `HA_DEADLINE_DEFAULT_MS` 2000 → 3000. | `src/cli/hook_augment.c` |
| A4 | Docstring enforcement | `PostToolUse` hook on Edit/Write reports missing docstrings in the file just changed; same hook enforces B5. | outside `spine/` |
| B5 | File-level descriptions | Capture the file's leading comment (JSDoc `@file`/`@fileoverview`, Python module docstring, Go package comment, else first comment block) into `LSMFileResult.file_docstring` and the per-file Module node's `docstring`; visible via `search_graph label:"Module" fields:["docstring"]` and semantic search. Go/Java have folder Modules, so for them it is visible only via the `docstrings` subcommand (spec, B5 known limitation). | `internal/lsm/extract_defs.c`, `internal/lsm/lsm.h`, tests |
| B5b | Exported JS/TS docstrings | `extract_docstring` hoists through `export_statement` so `/** doc */ export function f()` keeps its docstring. Found in plan review; required for A4 to be useful on TypeScript. | `internal/lsm/extract_defs.c`, tests |
| — | Coverage report | Decided by agent: a script outside `spine/` querying the graph for symbols with empty docstring, exported symbols only by default. | outside `spine/` |
