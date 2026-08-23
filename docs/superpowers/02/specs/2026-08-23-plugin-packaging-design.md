---
title: Version 02 — package the spine as a real Claude Code plugin
type: spec
status: draft
created: "2026-08-23 14:42 CDT"
updated: "2026-08-23 14:42 CDT"
sources:
  - "Live read of spine/src/cli/cli.c and spine/src/cli/agent_profiles.c in the worktree at .worktrees/plugin-packaging, 2026-08-23"
  - "Claude Code docs MCP server, pages /en/plugins.mdx, /en/plugins-reference.mdx, /en/plugin-marketplaces.mdx, /en/hooks.mdx, /en/mcp.mdx, /en/settings-reference.mdx, /en/sub-agents.mdx, /en/skills.mdx, read 2026-08-23"
  - "Live shell on this machine: claude plugin list, claude plugin --help, claude plugin validate --help, Claude Code 2.1.241"
  - "Live read of the installed artifacts under ~/.claude/ on this machine, 2026-08-23"
---

# Version 02 — package the spine as a real Claude Code plugin

## Problem

The spine is not a Claude Code plugin. It is a bespoke multi-client CLI installer that writes into the user's global configuration, plus one small skills-directory plugin bolted on beside it.

What `plugins/logan-spine-tools/scripts/install.sh` does today, verified on this machine:

| Artifact | Path | Written by |
|---|---|---|
| Binary | `~/.local/bin/logan-spine-mcp` | our `install.sh` |
| MCP registration | `~/.claude.json` → `mcpServers.logan-spine-mcp` | `logan-spine-mcp install --clients=claude` |
| 7 hook entries | `~/.claude/settings.json` → `hooks.{PreToolUse,PostToolUse,SessionStart,SubagentStart}` | same |
| 3 hook scripts | `~/.claude/hooks/lsm-{code-discovery-gate,session-reminder,subagent-reminder}` | same |
| 3 agents | `~/.claude/agents/logan-spine{,-scout,-auditor}.md` | same |
| 1 skill | `~/.claude/skills/logan-spine/SKILL.md` | same |
| 1 skills-dir plugin | `~/.claude/skills/logan-spine-tools/` | our `install.sh` (`cp -r`) |
| PATH line | `~/.bashrc` | our `install.sh` |

Three consequences follow, and they are the reason for this version:

1. **There is no per-repo control.** Every artifact above is global. Opening any repository loads the graph hooks, the graph agents and the graph skill. The only per-repo switch that exists is toggling the MCP server off in `/mcp`, which is recorded per project in `~/.claude.json` under `disabledMcpServers` and leaves the hooks, agents and skill running.
2. **Six of the seven artifacts are unmanaged by this repository.** They are rendered from C string literals inside the vendored engine, so their content is not reviewable in a diff, not testable in CI, and not versioned with the rest of the repo. One of them has already drifted: the three installed agent files at `~/.claude/agents/logan-spine*.md` contain a paragraph beginning "If the graph tools are unavailable, say so and fall back" that `grep -rl` finds in neither `spine/` nor any commit in this repository's history. It exists only on this machine's disk. Who wrote it and when is unknown.
3. **Uninstalling cleanly is not possible with the tools that exist.** `logan-spine-mcp uninstall` takes no `--clients` flag (`spine/src/cli/cli.c:11598-11642`), so it processes every agent client it detects. On a machine that also has Codex, Cursor or Gemini configured, removing the Claude Code footprint that way also removes theirs.

## Goal

One Claude Code plugin named `logan-spine`, held in this repository, installed through a marketplace, enabled or disabled per repository, carrying every Claude-facing component the spine needs. Nothing the spine installs for Claude Code lives outside the plugin directory except the binary itself.

Brainstorming for 02 happened in chat on 2026-08-23, the same way it did for 01. There is no `docs/superpowers/02/ideation/` folder and none is planned; the decisions live in this spec.

## Non-goals

- Changing what the engine does. No C code in `spine/src/` is modified by this version. The `install`/`uninstall` subcommands keep working for the other 42 clients; we simply stop calling them for Claude Code.
- Supporting clients other than Claude Code through the plugin. The plugin is a Claude Code artifact.
- Shipping the binary through the plugin system. See "The binary stays outside the plugin" below.
- Changing indexing behaviour, `auto_index`, the cache location, or anything under `~/.cache/logan-spine-mcp/`.

## Constraints discovered, and what each forces

Each row is a fact verified against the live docs or the live machine, followed by the design consequence it forces. These are not preferences.

| Fact | Source | Consequence |
|---|---|---|
| A plugin's MCP tools are named `mcp__plugin_<plugin-name>_<server-key>__<tool-name>`, and "a hook matcher written against the bare server key never fires for a plugin-bundled server" | `/en/mcp.mdx`, "Plugin MCP tool names" | All 29 `mcp__logan-spine-mcp__*` strings across the three agent files must be rewritten (11 in the verify tier, 11 in the auditor tier, 7 in the scout tier, counted 2026-08-23). This is the single largest mechanical change in the version. |
| `hooks`, `mcpServers` and `permissionMode` in a plugin agent's frontmatter are ignored, for security | `/en/plugins-reference.mdx`, "Agents"; `/en/sub-agents.mdx` frontmatter table | The agents lose `mcpServers: [logan-spine-mcp]` and `permissionMode: plan`. Their `tools:` allowlists already contain only read-only tools, so the enforced loss is the `plan` permission mode. |
| A plugin agent's `name` cannot contain `:`; agents address as `<plugin>:<agent>` | `/en/sub-agents.mdx`; `/en/plugins-reference.mdx:462` | An agent named `logan-spine` inside a plugin named `logan-spine` addresses as `logan-spine:logan-spine`. The agents are renamed `scout`, `verify`, `auditor`. |
| Plugin skills address as `/<plugin>:<skill>`, where the skill name is its folder name | `/en/plugins.mdx`, "Add a skill" | The skill folder is `skills/graph/`, addressing as `/logan-spine:graph`, for the same reason. |
| Copied plugins cannot reference files outside their own directory; `../` paths fail after install because the external files were never copied | `/en/plugins-reference.mdx`, "Path traversal limitations"; `/en/plugin-marketplaces.mdx` walkthrough note | The plugin must be self-contained. Nothing in it may reach into `spine/`. |
| A symlink from inside a plugin to a target outside the marketplace repository is skipped for security | `/en/plugins-reference.mdx`, "Share files within a marketplace with symlinks" | Symlinking `~/.local/bin/logan-spine-mcp` into the plugin does not work. |
| Archive sources are refused above 256 MiB; `command`-source copy mode is refused above 256 MiB or 20,000 entries | `/en/plugin-marketplaces.mdx`, "Zip archives" and "Copy mode and link mode" | The 280 MB binary cannot be shipped inside the plugin under any source type except `command` in link mode, which we are not using. |
| `.mcp.json` expands `${VAR}` and `${VAR:-default}` in `command`, `args`, `env`, `url` and `headers`; an unset variable with no default loads anyway and warns in `claude mcp list` | `/en/mcp.mdx`, "Environment variable expansion in `.mcp.json`" | The plugin locates the binary by expansion, with no machine path written into any tracked file. |
| `enabledPlugins` honours project and local settings; `pluginConfigs` deliberately does not | `/en/plugins-reference.mdx:598`, "User configuration" | Per-repo enable works through `.claude/settings.json`. A `userConfig` value could not have been set per repo even if we had chosen it. |
| There is no `disabledPlugins` key, and "there is no way to disable an individual hook while keeping it in the configuration" | grep of `/en/` returns zero matches for `disabledPlugins`; `/en/hooks.mdx:695` | Disabling is `"logan-spine@logan-mem": false` in `enabledPlugins`, or omission. Disabling one of the plugin's hooks without disabling the plugin is not possible, which is acceptable: the plugin is the unit. |
| `bin/` at a plugin root is added to "the Bash tool's PATH"; the docs never state it reaches an MCP server subprocess | `/en/plugins-reference.mdx`, file-locations table | We do not rely on `bin/` for the MCP server's `command`. |
| `logan-spine-mcp uninstall` has no `--clients` flag | `spine/src/cli/cli.c:11598-11642` | Removing the current global footprint requires our own surgical script. |
| Upstream refuses to overwrite or remove an agent file whose bytes match neither the current render nor a known legacy shape ("preserved modified profile") | `spine/src/cli/cli.c:7922-7924`, `10369-10377`; `spine/src/cli/config_text_edit.c:1276-1347` | Upstream's uninstall would leave the three hand-edited agent files behind even if we ran it. Our script removes them by path. |

## Design

### Repository layout after this version

```
.claude-plugin/
  marketplace.json              # new: the repo is a marketplace named logan-mem
plugins/
  logan-spine/                  # new: the one plugin
    .claude-plugin/plugin.json
    .mcp.json
    agents/
      scout.md
      verify.md
      auditor.md
    skills/
      graph/SKILL.md
    hooks/
      hooks.json
      lib.sh                    # shared binary resolution, sourced by the hook scripts
      code-discovery-gate.sh
      session-reminder.sh
      subagent-reminder.sh
      docstring-check.sh
    scripts/
      install.sh                # build + place binary + register the marketplace
      unregister-global.sh      # surgical removal of the pre-plugin footprint
      docstring-coverage.sh
    tests/run.sh
    README.md
plugins/logan-spine-tools/      # deleted; absorbed into plugins/logan-spine/
```

`spine/` is untouched. Its location is pinned by git subtree metadata and stays where it is.

### The marketplace

`.claude-plugin/marketplace.json` at the repository root:

```json
{
  "name": "logan-mem",
  "owner": { "name": "Logan G" },
  "plugins": [
    {
      "name": "logan-spine",
      "source": "./plugins/logan-spine",
      "description": "Codebase knowledge graph for Claude Code: MCP server, tiered graph agents, discovery hooks, and a docstring nudge."
    }
  ]
}
```

A relative `source` must begin with `./` and cannot use `../`. One marketplace file serves both installation routes:

- Local: `claude plugin marketplace add /home/ubuntu/projects/org/logan-mem`
- Remote: `claude plugin marketplace add pogan314/logan-mem`

The plugin key in `enabledPlugins` is `logan-spine@logan-mem` in both cases, so a repository's committed `.claude/settings.json` works for anyone who has added either.

### The manifest

`plugins/logan-spine/.claude-plugin/plugin.json`:

```json
{
  "name": "logan-spine",
  "version": "0.1.0",
  "description": "Codebase knowledge graph for Claude Code: MCP server, tiered graph agents, discovery hooks, and a docstring nudge.",
  "author": { "name": "Logan G" }
}
```

No component-path fields. Every component sits in its default location and is auto-discovered, which is the documented preference and removes a class of path-resolution bug.

`version` is set explicitly and bumped by hand. With an explicit version, "users get updates only when you bump this field" — pushing commits alone does nothing. That is the behaviour we want: the plugin is not silently re-resolved on every repository commit.

### The MCP server

`plugins/logan-spine/.mcp.json`:

```json
{
  "mcpServers": {
    "spine": {
      "command": "${LOGAN_SPINE_BIN:-${HOME}/.local/bin/logan-spine-mcp}",
      "args": []
    }
  }
}
```

Two decisions are encoded here.

**The server key is `spine`, not `logan-spine-mcp`.** The key becomes part of every tool name. `spine` yields `mcp__plugin_logan-spine_spine__search_graph`; `logan-spine-mcp` would yield `mcp__plugin_logan-spine_logan-spine-mcp__search_graph`, 14 characters longer on every one of 15 tools in every agent allowlist and every permission rule.

**The binary is located by variable expansion.** `${LOGAN_SPINE_BIN:-${HOME}/.local/bin/logan-spine-mcp}` puts no machine path in a tracked file, needs no prompt, and gives anyone who installs the binary elsewhere a single environment variable to set. If the binary is missing the server fails to start and the failure is visible in `/mcp` and in `claude mcp list`; the hooks degrade silently by design.

Whether a nested default (`${VAR:-${OTHER}/path}`) expands correctly is not stated in the docs. The implementation plan must verify this empirically before relying on it, and fall back to a flat `${HOME}/.local/bin/logan-spine-mcp` with the override documented in the README if nesting does not expand.

### The binary stays outside the plugin

The binary is 280 MB, above the 256 MiB ceiling for every plugin source type that could carry it, and a copied plugin cannot reference anything outside its own directory. It therefore stays where `install.sh` already puts it, at `~/.local/bin/logan-spine-mcp`, and the plugin reaches it only through the expansion above and through the hook scripts' resolution helper.

This is a real seam: the plugin and the binary are versioned and installed separately, and a plugin enabled without the binary present is inert. The README states this, `install.sh` installs both in one command, and the plugin's own tests assert the failure is silent rather than noisy.

### Agents

Three files under `agents/`, one per evidence tier, renamed to avoid the `logan-spine:logan-spine` collision:

| File | `name` | Addressed as | Was |
|---|---|---|---|
| `agents/scout.md` | `scout` | `logan-spine:scout` | `logan-spine-scout` |
| `agents/verify.md` | `verify` | `logan-spine:verify` | `logan-spine` |
| `agents/auditor.md` | `auditor` | `logan-spine:auditor` | `logan-spine-auditor` |

Content is taken from the **installed files on this machine**, not from the C source, because the installed files carry the graph-unavailable fallback paragraph and the C source does not. That paragraph is the only known difference; the implementation must diff the installed file against a freshly rendered one and record every difference it finds, rather than assuming there is exactly one.

Frontmatter changes, all forced:

- `tools:` entries rewritten from `mcp__logan-spine-mcp__X` to `mcp__plugin_logan-spine_spine__X`.
- `mcpServers: [logan-spine-mcp]` removed — ignored for plugin agents.
- `permissionMode: plan` removed — ignored for plugin agents.
- `skills: [logan-spine]` becomes `skills: [logan-spine:graph]`, matching the plugin-namespaced skill name.

Prompt bodies are carried over verbatim except for tool-name strings, which are updated wherever they appear in prose.

### Skill

`skills/graph/SKILL.md`, addressed as `/logan-spine:graph`. Content is the installed `~/.claude/skills/logan-spine/SKILL.md`, which is a verbatim copy of the `skill_content` literal at `spine/src/cli/cli.c:1277-1389`. The bare tool names in its tables (`trace_path`, `search_graph`) are prose and stay as written; the description frontmatter is preserved so the same triggers fire.

### Hooks

`hooks/hooks.json` carries all five hooks — the four the engine installs today plus the docstring nudge absorbed from `logan-spine-tools`:

| Event | Matcher | Script | Origin |
|---|---|---|---|
| `PreToolUse` | `Grep\|Glob` | `code-discovery-gate.sh` | engine |
| `PostToolUse` | `Read` | `code-discovery-gate.sh` | engine |
| `SessionStart` | `startup`, `resume`, `clear`, `compact` | `session-reminder.sh` | engine |
| `SubagentStart` | `*` | `subagent-reminder.sh` | engine |
| `PostToolUse` | `Edit\|Write` | `docstring-check.sh` | `logan-spine-tools` |

Every `command` uses `"${CLAUDE_PLUGIN_ROOT}"/hooks/<name>.sh`, quoted, matching the pattern the existing `logan-spine-tools` hook already uses. Timeouts stay at the engine's 5 seconds for the four graph hooks and 10 for the docstring hook.

The engine writes the four `SessionStart` matchers as four separate array entries. Whether a single entry with the regex `startup|resume|clear|compact` is equivalent is not established. The plan verifies it; if it is not, the four entries are written out separately, which is behaviour-identical to today.

All four graph hook scripts run the identical command — `"$BIN" hook-augment` — with no event-specific argument (`spine/src/cli/cli.c:5059-5083`). They differ only in their comment headers. They are kept as separate files anyway so each hook's purpose is legible at its call site and so one can be changed without the others.

`hooks/lib.sh` holds the one piece of shared logic, sourced by all five scripts:

```bash
# Resolve the engine binary. Print its path and return 0, or return 1 if absent.
lsm_bin() {
  if [ -n "${LOGAN_SPINE_BIN:-}" ] && [ -x "$LOGAN_SPINE_BIN" ]; then printf '%s\n' "$LOGAN_SPINE_BIN"; return 0; fi
  if [ -x "$HOME/.local/bin/logan-spine-mcp" ]; then printf '%s\n' "$HOME/.local/bin/logan-spine-mcp"; return 0; fi
  command -v logan-spine-mcp 2>/dev/null && return 0
  return 1
}
```

The resolution order matches `.mcp.json`'s (`LOGAN_SPINE_BIN`, then `~/.local/bin`) and then falls back to `PATH`, which the MCP server cannot use but a hook running under the Bash environment can. Every graph hook exits 0 silently when the binary is absent, preserving today's fail-open behaviour. The docstring hook keeps its exit-2 contract, which is how it shows its message to Claude without failing the tool.

### Scripts

**`scripts/install.sh`** — one command, same entry point as today, with the Claude Code portion changed:

1. Build via `spine/scripts/build.sh` (unchanged).
2. Place the binary at `${LSM_BIN_DIR:-$HOME/.local/bin}/logan-spine-mcp` (unchanged), using `mv` rather than `cp` so replacing a running binary does not fail with "Text file busy".
3. **Removed:** the `logan-spine-mcp install --clients=claude -y` call.
4. **Removed:** the `cp -r` into `~/.claude/skills/logan-spine-tools`.
5. `logan-spine-mcp config set auto_index true` (unchanged — this is the engine's own global config under `~/.cache/`, not Claude Code configuration).
6. Register the marketplace: `claude plugin marketplace add "$ROOT"`, idempotent.
7. Print the exact next steps: the per-repo enable command and `/reload-plugins`.

The script must not enable the plugin anywhere on its own. Enabling is a per-repo decision and the whole point of the version.

**`scripts/unregister-global.sh`** — removes the pre-plugin footprint and nothing else. Required because upstream's `uninstall` has no `--clients` flag and would also strip other agent clients' configuration from this machine.

- Default mode is a dry run that prints exactly what it would change and exits 0. Acting requires `--yes`.
- Backs up `~/.claude/settings.json` and `~/.claude.json` to `<path>.logan-spine-backup-<timestamp>` before writing.
- Removes from `~/.claude/settings.json` only hook entries whose `command` contains `lsm-code-discovery-gate`, `lsm-session-reminder` or `lsm-subagent-reminder`, then removes any hook event array left empty. Everything else in the file is preserved byte-for-byte as far as `jq` allows; the script states in its output that `jq` reformats the file, which is why the backup is taken.
- Removes `mcpServers["logan-spine-mcp"]` from `~/.claude.json` and nothing else. This file also holds per-project state, so the script must not rewrite unrelated keys.
- Removes `~/.claude/hooks/lsm-code-discovery-gate`, `lsm-session-reminder`, `lsm-subagent-reminder`.
- Removes `~/.claude/agents/logan-spine.md`, `logan-spine-scout.md`, `logan-spine-auditor.md`.
- Removes `~/.claude/skills/logan-spine/` and `~/.claude/skills/logan-spine-tools/`.
- Leaves the binary, `~/.bashrc`, `~/.cache/logan-spine-mcp/` and every other client's configuration alone. Removing the index cache would force a full re-index of six projects for no reason.
- Refuses to run when `HOME` is unset.

**`scripts/docstring-coverage.sh`** moves across unchanged. It is already portable across GNU and BSD `xargs`.

### Per-repo enablement

For this repository, `.claude/settings.json` gains:

```json
{
  "enabledPlugins": { "logan-spine@logan-mem": true }
}
```

This file is committed, so the setting travels with the repository. A repository that does not list the plugin does not load it. Turning it off explicitly in one repository is `false` rather than omission.

The marketplace registration itself is per machine, not per repository, and lives in `~/.claude/settings.json` under `extraKnownMarketplaces`. `install.sh` handles it.

### Tests

`plugins/logan-spine/tests/run.sh` replaces the `logan-spine-tools` suite and grows to cover the new surface. It must be runnable with no Claude Code session active and must never touch the real `$HOME`.

1. `claude plugin validate plugins/logan-spine --strict` exits 0.
2. `claude plugin validate .` (the marketplace root) exits 0.
3. Every JSON file the plugin ships parses, and `plugin.json`'s `name` equals the directory name and the marketplace entry's `name`.
4. `hooks.json` declares exactly the five hooks in the table above, with the expected events, matchers and script paths, and every referenced script exists and is executable.
5. Every agent file's `tools:` entries that begin `mcp__` use the `mcp__plugin_logan-spine_spine__` prefix; none use the old `mcp__logan-spine-mcp__` prefix. No agent file contains `mcpServers:` or `permissionMode:`.
6. Each of the five hook scripts, run with `LOGAN_SPINE_BIN` pointed at a non-existent path, exits 0 and prints nothing.
7. Each graph hook script, run with `LOGAN_SPINE_BIN` pointed at a stub that echoes a marker, passes the marker through.
8. `docstring-check.sh` exits 2 and writes to stderr when the stub reports a missing docstring, and exits 0 when it does not.
9. `unregister-global.sh --dry-run` against a fixture `HOME` populated with the full pre-plugin footprint lists every artifact named in the Problem table and changes no file. Run with `--yes` against the same fixture, it removes exactly those and leaves an unrelated hook entry, an unrelated MCP server and an unrelated agent file intact.
10. No tracked file under `plugins/` or `.claude-plugin/` contains an absolute machine path (`/home/`, `/Users/`, `C:\`). This enforces the standing rule mechanically.

### Documentation

- `CLAUDE.md`: the status line moves to version 02; the folder map gains `.claude-plugin/` and `plugins/logan-spine/` and loses `plugins/logan-spine-tools/`; the installation bullet is rewritten from "installs once per machine, never per repo" to the marketplace-plus-per-repo-enable model; a gotcha records that plugin MCP tool names are `mcp__plugin_logan-spine_spine__*` so future sessions do not write the old names.
- `README.md`: install and enable instructions in the new shape.
- `plugins/logan-spine/README.md`: what the plugin contains, how to enable it per repository, that the binary is installed separately, and how to point at a binary outside `~/.local/bin`.
- `spine/LOGAN-CHANGES.md`: a new row recording that we no longer call upstream's Claude Code installer, with the reason and the date. The engine version does not change; no C code is modified.
- `docs/superpowers/02/plans/`: the implementation plan written from this spec.

## What could go wrong

| Risk | Handling |
|---|---|
| The nested default `${VAR:-${OTHER}/path}` does not expand in `.mcp.json` | Verified empirically in the first task. Flat `${HOME}/...` is the documented fallback. |
| A single `SessionStart` matcher regex is not equivalent to four separate entries | Verified empirically. Four entries is the fallback and matches today exactly. |
| Plugin MCP tool names differ from the documented form in the installed Claude Code version | Verified against a live `/mcp` listing before any agent file is rewritten. The names propagate to three agent files and the tests, so this is checked once, early, and everything else follows. |
| `unregister-global.sh` damages `~/.claude.json`, which holds per-project state for every project on the machine | Dry run by default, timestamped backup, `jq` edit of exactly one key, and a fixture-`HOME` test asserting unrelated keys survive. Never run against the real `HOME` in tests. |
| The plugin is enabled in a repository where the binary is not installed | Every hook exits 0 silently. The MCP server fails visibly in `/mcp`, which is the correct place for it to be visible. |
| Losing `permissionMode: plan` on the agents lets one write a file | Their `tools:` allowlists contain only `Read`, `Grep`, `Glob` and read-only MCP tools, so no write tool is reachable. Asserted by test 5. |
| The hand-edited agent text is lost in the move | The move is sourced from the installed files, and the implementation diffs installed against freshly rendered and records every difference before writing. |

## Success criteria

The version is done when all of the following hold on this machine, each demonstrated by a command whose output is recorded:

1. `claude plugin list` shows `logan-spine@logan-mem`, scope `project`, status enabled, in this repository.
2. In a repository whose `.claude/settings.json` does not list it, `claude plugin list` shows `logan-spine@logan-mem` as not enabled, and `/mcp` does not list the `spine` server.
3. `/mcp` shows the `spine` server connected, and a graph tool call succeeds under its new `mcp__plugin_logan-spine_spine__` name.
4. `~/.claude/settings.json` contains zero `lsm-` hook entries; `~/.claude.json` contains no `mcpServers.logan-spine-mcp`; `~/.claude/hooks/`, `~/.claude/agents/` and `~/.claude/skills/` contain no `logan-spine` artifacts.
5. Editing a file with a missing docstring in a repository with the plugin enabled produces the docstring nudge; doing so in a repository without it produces nothing.
6. `plugins/logan-spine/tests/run.sh` passes.
7. `git grep -nE '/home/|/Users/' -- plugins .claude-plugin` returns nothing.
8. `HOME="$(mktemp -d)" spine/scripts/test.sh` reports the version 01 baseline of 273 passed, 10 failed, confirming no C code was touched. The isolated `HOME` is mandatory: see the cli-suite gotcha in `CLAUDE.md`.
