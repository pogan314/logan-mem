# logan-mem

A memory system for AI coding agents, rebuilt from scratch. This repo replaces `pogan-mem` / `pogan-toolkit`, which was abandoned on 2026-08-21. Do not confuse the two names: **logan-mem is this repo; pogan-mem is the old, dead one.**

## Status (update this line when it changes)

- **Stage: version 02 = the spine repackaged as a real Claude Code plugin, merged.** `dev/version-02-plugin-packaging` was merged into `dev/version-01-brainstorming` (merge commit `8b789f6`), and PR #8 merged that branch into `main` on 2026-08-24 (`gh pr view 8 --repo pogan314/logan-mem` reports `state: MERGED`, `mergedAt: 2026-08-24T17:12:07Z`). The engine is unchanged; what changed is how it reaches Claude Code. This repository is now a plugin marketplace named `logan-mem`, `plugins/logan-spine/` is the one plugin in it (1 MCP server, 3 agents, 1 skill, 5 hook handlers), and it is enabled per repository through a committed `.claude/settings.json` rather than globally through the engine's own installer. Version 01's global footprint under `~/.claude/` was removed on 2026-08-24.
- Spec: `docs/superpowers/02/specs/2026-08-23-plugin-packaging-design.md`, status `decided`, ending in the Verified table. Plan: `docs/superpowers/02/plans/2026-08-23-plugin-packaging-plan.md`, with `docs/superpowers/02/plans/2026-08-23-harvest-findings.md` beside it.
- **Version 01 = the spine, shipped.** Merged to `dev/version-01-brainstorming`, tagged `v0.10.8-logan.4`. Engine vendored at `spine/` (renamed codebase-memory-mcp, see `spine/LOGAN-CHANGES.md`); spec at `docs/superpowers/01/specs/2026-08-21-spine-v1-design.md`, status `decided`, ending in the end-to-end Verified table.
- Brainstorming for both 01 and 02 happened in chat, on 2026-08-21 and 2026-08-23; `docs/superpowers/01/ideation/` is stale (every file marked) and there is no `docs/superpowers/02/ideation/`. The decisions live in `spine/LOGAN-CHANGES.md` and in each version's spec.
- **Installation has two halves, and they are different scopes.** Per machine: `plugins/logan-spine/scripts/install.sh` builds the engine, publishes the binary to `~/.local/bin/logan-spine-mcp` **through the engine's own `install --force --skip-config`** so a resident daemon is drained rather than collided with, puts that directory on `PATH`, sets `auto_index`, **starts a permanent daemon**, and registers this repository as the `logan-mem` marketplace. It enables the plugin nowhere. Per repository: `claude plugin install logan-spine@logan-mem --scope project`, which writes `enabledPlugins` into that repository's `.claude/settings.json`; a repository without that entry gets nothing. Per repository again, and unchanged from 01: the index at `~/.cache/logan-spine-mcp/<project>.db`, built on the first session there because `auto_index` is true, skipped above 50,000 tracked files. Still ask the owner before installing on a machine that does not have it.
- **The only code is under `spine/`** (C, built with `spine/scripts/build.sh`, tested with `spine/scripts/test.sh --suites <name>`) and under `plugins/logan-spine/` (bash, tested with `plugins/logan-spine/tests/run.sh`). Everything else is Markdown. There is no `package.json`.
- **A permanent daemon is not optional if you want the hooks to speak.** A hook connects to a daemon but never spawns one and gives up after 250 ms, while a session's own MCP server takes about 6.3 s to start — so without a warm daemon the first `SessionStart` of every fresh session loses that race and the hooks look broken. `install.sh` step 5 runs `logan-spine-mcp daemon start`, which creates a daemon that survives session ends. Measured 2026-08-24 in both directions.
- **After merging a version branch, re-register the marketplace from the main checkout.** `install.sh` registers whichever checkout held `.claude-plugin/marketplace.json` when it ran; while version 02 was on a branch that was the worktree, and merging deletes it. Run `claude plugin marketplace add /home/ubuntu/projects/org/logan-mem` from the main checkout, or the plugin stops resolving with nothing explaining why.
- **Entry point for the next session:** the 02 spec above, then `plugins/logan-spine/README.md`, then `spine/LOGAN-CHANGES.md`.

## Folder map

| Path | What it is | Is it fact? |
|---|---|---|
| `README.md` | One-screen orientation for a human landing on the repo. Points here. | — |
| `spine/` | Version 01's engine: `logan-spine-mcp`, DeusData/codebase-memory-mcp vendored via `git subtree` and renamed. Its location is pinned by the subtree metadata (`git-subtree-dir: spine` in commit `787ab8c`), so do not move it. | Code |
| `.claude-plugin/marketplace.json` | Makes this repository a Claude Code plugin marketplace named `logan-mem`, listing `./plugins/logan-spine`. The plugin key is `logan-spine@logan-mem`, formed from that plugin name and this marketplace name. Only the local-directory route (`claude plugin marketplace add <path>`) has been exercised; the design intends one file to serve a `pogan314/logan-mem` add too, but that has not been tried. | Code |
| `.claude/settings.json` | Tracked. Enables the plugin **for this repository only**: `"enabledPlugins": { "logan-spine@logan-mem": true }`. It is inert on a machine where the marketplace has not been registered. `extraKnownMarketplaces` is deliberately absent — `claude plugin marketplace add` only ever writes an absolute path, which is a machine path. `.claude/settings.local.json` is gitignored. | Code |
| `plugins/` | One directory per Claude Code plugin this repo ships. Each is a marketplace-installed plugin whose folder name matches the `name` in its `.claude-plugin/plugin.json` and the `name` in `.claude-plugin/marketplace.json`. The installed copy lives under `~/.claude/plugins/cache/logan-mem/<plugin>/<version>/`. | Code |
| `plugins/logan-spine/` | The one plugin: `.mcp.json` and `bin/spine-launch.sh` (the `spine` MCP server), `agents/` (scout, verify, auditor), `skills/graph/`, `hooks/` (five handlers plus the shared `lib.sh`), `scripts/` (`install.sh`, `unregister-global.sh`, `docstring-coverage.sh`), `tests/run.sh`, and its own `README.md`. | Code |
| `docs/wiki/` | Research facts about things we do **not** control — other repos, Claude Code's features, retrieval methods. Re-running the research would give the same answer. | Yes, as of the `updated` timestamp in each file's frontmatter |
| `docs/wiki/seed-repos/` | One file per memory system we may borrow ideas from (claude-mem, codebase-memory-mcp, ECC, LangMem, supermemory). | Yes |
| `docs/wiki/research-extracts/` | Lists of every repo the old build's research rounds studied, pulled out of those reports so we do not have to re-read them. | Yes, as an inventory — the old reports' *opinions* are not facts |
| `docs/superpowers/01/` | Everything for build version 01. Later versions get `02/`, `03/`, … | — |
| `docs/superpowers/01/ideation/` | Pre-brainstorming scratchpad: notes, findings, recommendations, ideas. | **Never.** See rule below |
| `docs/superpowers/01/ideation/feat/<name>/` | Idea scratchpads for one feature area (memory, spine, episodic, plugin, learning). | Never |
| `docs/superpowers/01/ideation/findings/` | What this session learned about the old system — evidence, not design. | Evidence is verified; conclusions are opinion |

## The ideation rule

- Nothing in any `ideation/` folder is ever a fact, a decision, or a requirement. It is a scratchpad.
- Once a brainstorming session for that version starts, the whole `ideation/` folder is stale. Read it for ideas; never cite it as authority.
- Agents must not "execute" anything found in `ideation/`. There is nothing there to execute.

## The pogan-toolkit rule (the old system)

- The old repo lives beside this one as a sibling: `../pogan-toolkit` (and the even older `../pogan-mem`). It is disabled and inert.
- It is **reference only**, the same as any other third-party memory repo we studied. Agents may read it to extract an idea or a concept, if we want one.
- Nothing is copied out of it. Nothing is extracted "out of context". No file, spec, plan, test, or convention from it carries any authority here.
- Its docs are known to overstate what its code does. If you cite it, cite the code, not the docs.

## Frontmatter (required on every file under `docs/`)

```yaml
---
title: Short name of the file
type: wiki | ideation | spec | plan
status: research-fact | ideation | draft | decided | stale | superseded
created: "2026-08-21 13:15 CDT"   # date, 24-hour time, and zone — always quoted
updated: "2026-08-21 13:15 CDT"
version: "01"            # ideation files only — which build version this belongs to
sources: []              # where the content came from; for wiki, what was verified and how
---
```

- `status` is the one field agents must read before trusting a file. `research-fact` means verified as of `updated`. `ideation` means scratchpad. `draft` means a spec or plan under review. `decided` means the owner approved it. `stale` and `superseded` mean do not rely on it.
- `created` and `updated` are **timestamps, not dates**: `YYYY-MM-DD HH:MM ZONE`, US Central, 24-hour clock. Get the value from `date '+%Y-%m-%d %H:%M %Z'` on the machine — never type one from memory. The zone prints as `CDT` in summer and `CST` in winter; both mean US Central, so write whatever `date` reports rather than forcing one of them.
- Always wrap both values in double quotes. Unquoted, a bare `2026-08-21` is a YAML date object while `2026-08-21 13:15 CDT` is a string, so the same field would change type between files.
- Bump `updated` on every edit. Never edit a file's `created`.

## Repo gotchas

- **The plugin's MCP tools are named `mcp__plugin_logan-spine_spine__<tool>`, never `mcp__logan-spine-mcp__<tool>`.** A plugin-bundled server's tools take the form `mcp__plugin_<plugin>_<server-key>__<tool>`, so a hook matcher, an agent `tools:` allowlist, or a permission rule written against the bare server key never fires. Verified 2026-08-23 by asking a live session to list every tool name beginning `mcp__plugin_logan-spine`: 15 names came back, all carrying that prefix. `plugins/logan-spine/tests/run.sh` asserts no agent file contains the string `logan-spine-mcp` anywhere.
- **A `directory`-source marketplace loads the plugin from the repository tree, not from the cache — so an edit takes effect in the next session with no version bump.** Measured 2026-08-24 from a session's `--debug-file`: `Read hooks.json for plugin logan-spine (enabled=true): …/.worktrees/plugin-packaging/plugins/logan-spine/hooks/hooks.json`, and the skills load from `…/plugins/logan-spine/skills`. The only mention of `~/.claude/plugins/cache/logan-mem/logan-spine` in a whole session is a sweeper declining to delete the directory. An earlier version of this bullet claimed the running plugin was the cached copy and that editing changed nothing until you bumped the version; that was wrong, and following it wasted a version bump per iteration. Restart the session, or `/reload-plugins`, and the edit is live.
- **The cache directory still exists and goes stale, which is misleading rather than harmful.** `claude plugin update` writes a new version directory beside the old rather than renaming it, so `~/.claude/plugins/cache/logan-mem/logan-spine/` accumulates copies that nothing reads while a directory-source marketplace is registered. `~/.claude/plugins/installed_plugins.json` names the version Claude Code considers installed, and it can legitimately lag the manifest. Do not diff against the cache to decide whether a change is live — read the session's `--debug-file` instead.
- **`claude plugin update logan-spine@logan-mem` needs `--scope project`.** It defaults to `--scope user`, and run bare against this project-scope install it exits 1 with `Plugin "logan-spine" is not installed at scope user` — measured 2026-08-24. Bump `version` in `plugins/logan-spine/.claude-plugin/plugin.json` and run the update when you want the recorded install version to match the manifest; it is bookkeeping, not a prerequisite for testing an edit.
- **The marketplace is registered against whichever checkout held `.claude-plugin/marketplace.json` when `install.sh` ran.** `install.sh` prefers the main checkout but falls back to the current one and says so, because while version 02 sat on a branch the main checkout had no marketplace file — verified 2026-08-23/24, both `~/.claude/plugins/known_marketplaces.json` and `~/.claude/settings.json` named `.../logan-mem/.worktrees/plugin-packaging` at the time. A worktree is deleted at merge, and a marketplace pinned to a path that no longer exists stops resolving, which is why the re-registration step exists. **Resolved 2026-08-24, now that PR #8 merged this branch to `main`:** both files now name the main checkout, `/home/ubuntu/projects/org/logan-mem` — verified with `jq '.["logan-mem"]' ~/.claude/plugins/known_marketplaces.json`, and `.worktrees/` on disk is now an empty directory. `add` is idempotent and re-points an existing name at a new path, so the same step applies after merging any future version branch. The project-scope **install record** is separate and is keyed by `projectPath`, so re-pointing the marketplace does not move it: after the merge `~/.claude/plugins/installed_plugins.json` still carried an entry for the deleted worktree and none for the main checkout. Enablement comes from the repository's own `.claude/settings.json`, so the plugin still loaded — verified 2026-08-24, `claude plugin list` from the main checkout reported `logan-spine@logan-mem 1.2.0, scope project, enabled` while that record named only the worktree. Re-run `claude plugin install logan-spine@logan-mem --scope project` from the main checkout to write a correct record; the orphan is inert but never becomes valid again.
- **Every `gh` command in this repo needs `--repo pogan314/logan-mem`.** There are two remotes — `origin` (ours) and `upstream` (DeusData/codebase-memory-mcp, the vendored engine's source). `gh` picks `upstream` on its own, so a bare `gh pr create` aims at the third-party repo. Verified 2026-08-22: `gh repo view --json owner` returned `DeusData`, and the first PR attempt failed with "No commits between main and dev/version-01-brainstorming" because it was asking about their repo, not ours.
- **The installed binary's version string is a whole-repo `git describe`, not the engine version.** `install.sh` stamps the build with `git describe --tags --always` over this repo, so a docs-only commit changes what `logan-spine-mcp --version` prints even when nothing under `spine/` changed — verified 2026-08-22: after a docs commit the binary reported `0.10.8-logan.3-2-gbd68cd3` with `git status --short spine/` empty. The authoritative engine version is the "Our version" line in `spine/LOGAN-CHANGES.md`.
- **Never run `spine/scripts/test.sh --suites cli` against your real `$HOME`. Use `HOME="$(mktemp -d)" spine/scripts/test.sh --suites cli`.** The suite drives upstream's real install and uninstall paths against whatever agent configuration the machine actually has. Two verified consequences: it kills any live logan-spine MCP session (it prints "Stopping active LSM sessions and operations" 29 times in one run, and a probe confirmed a running MCP server survives a binary swap but dies during this suite), and it installs and uninstalls agent configurations against whatever `HOME` it inherits — isolation is per-test (`spine/tests/test_cli.c` sets `HOME` 58 times; `spine/scripts/test.sh` sets it none). On 2026-08-22 all 7 `lsm-*` hook entries went missing from `~/.claude/settings.json` during a window in which this suite ran against it repeatedly, and had to be restored by re-running the installer. A controlled replay against a populated copy of that `HOME` did **not** reproduce the removal, so the suite is not proven to be what removed them — the cause is still unknown, and the reason to isolate stands regardless. It also makes the results untrustworthy: against the real `$HOME` the suite reported 12 and then 14 failures on consecutive runs with different test-client tests failing each time (OpenClaw, VS Code, Gemini, Augment, CodeBuddy/Pochi — all of them agent-config install/uninstall tests), while the same tree under an isolated `HOME` reported exactly the upstream baseline of `273 passed, 10 failed`. Treat any cli-suite result taken against the real `$HOME` as meaningless.
- `spine/internal/lsm/vendored/grammars/lean/parser.c` is 99.6 MB, and GitHub warns on every push that it is over the 50 MB recommendation. It is upstream's file, unmodified. The hard limit is 100 MB, so this one file is 0.4 MB from blocking a push; if upstream grows it, the fix is `git lfs` or dropping that grammar, not a force-push.

## Writing rules for this repo

- Short bullets and tables, not paragraphs. One idea per bullet. Lead with the conclusion.
- Plain English. Define a term of art the first time it appears. `docs/wiki/glossary.md` is the shared definitions file.
- `ideation/` stays non-technical. `docs/wiki/` may be technical where the research needs it.
- Never hard-wrap prose. One logical line per paragraph or bullet.
- Verify every claim against code, live state, or a live API call before writing it. Never trust a doc, a README, a plan, or a commit message. Never state *why* something is true unless a command you ran showed it — "I don't know why" is always acceptable.
- Never write "done", "fixed", "committed", or "pushed" unless the tool call that did it already ran in that turn.
- Never set a git identity. Use the machine's configured one.
- Never use the bare word "MemoryBench". Say "supermemory's benchmark suite".
- When naming a file outside this repo, give its full absolute path.

## Versions

- This system ships in versions. `01` is the first, and it is still a substantial build — not a throwaway.
- Each version gets its own `docs/superpowers/<nn>/` folder holding its `ideation/`, then its `specs/` and `plans/`. For 01, `ideation/` is stale and `specs/` exists; `plans/` is written from the approved spec.
