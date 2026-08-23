---
title: Version 02 — package the spine as a real Claude Code plugin
type: spec
status: draft
created: "2026-08-23 14:42 CDT"
updated: "2026-08-23 15:25 CDT"
sources:
  - "Live read of spine/src/cli/cli.c, spine/src/cli/agent_profiles.c and spine/src/cli/hook_augment.c in the worktree at .worktrees/plugin-packaging, 2026-08-23"
  - "Claude Code docs MCP server, pages /en/plugins.mdx, /en/plugins-reference.mdx, /en/plugin-marketplaces.mdx, /en/hooks.mdx, /en/mcp.mdx, /en/settings-reference.mdx, /en/sub-agents.mdx, /en/skills.mdx, read 2026-08-23"
  - "Live shell on this machine: claude plugin list, claude plugin --help, claude plugin validate --help, claude mcp get, jq over ~/.claude/settings.json and ~/.claude/plugins/*.json, Claude Code 2.1.241"
  - "Live read of the installed artifacts under ~/.claude/ on this machine, 2026-08-23"
  - "Two independent opus spec reviews, 2026-08-23; 22 findings, all resolved"
  - "Two independent opus plan reviews, 2026-08-23; 27 findings, of which two disproved spec claims and are corrected here"
---

# Version 02 — package the spine as a real Claude Code plugin

## Problem

The spine is not a Claude Code plugin. It is a bespoke multi-client CLI installer that writes into the user's global configuration, plus one small skills-directory plugin bolted on beside it.

What the spine's installation puts on a machine today, verified on this one:

| Artifact | Path | Written by |
|---|---|---|
| Binary | `~/.local/bin/logan-spine-mcp` | our `install.sh` |
| MCP registration | `~/.claude.json` → `mcpServers.logan-spine-mcp` | `logan-spine-mcp install --clients=claude` |
| 7 hook handlers | `~/.claude/settings.json` → `hooks.{PreToolUse,PostToolUse,SessionStart,SubagentStart}` | same |
| 3 hook scripts | `~/.claude/hooks/lsm-{code-discovery-gate,session-reminder,subagent-reminder}` | same |
| 3 agents | `~/.claude/agents/logan-spine{,-scout,-auditor}.md` | same |
| 1 skill | `~/.claude/skills/logan-spine/SKILL.md` | same |
| PATH line | `~/.bashrc:134-135`, under the comment `# Added by logan-spine-mcp install` | same |
| 1 skills-dir plugin | `~/.claude/skills/logan-spine-tools/` | our `install.sh` (`cp -r`) |

Three consequences follow, and they are the reason for this version:

1. **There is no per-repo control.** Every artifact above is global. Opening any repository loads the graph hooks, the graph agents and the graph skill. The only per-repo switch that exists is toggling the MCP server off in `/mcp`, which is recorded per project in `~/.claude.json` under `disabledMcpServers` and leaves the hooks, agents and skill running.
2. **Six of the eight artifacts are unmanaged by this repository.** They are rendered from C string literals inside the vendored engine, so their content is not reviewable in a diff, not testable in CI, and not versioned with the rest of the repo. One of them has already drifted: the three installed agent files at `~/.claude/agents/logan-spine*.md` contain a paragraph beginning "If the graph tools are unavailable, say so and fall back" that `grep -rl` finds in neither `spine/` nor any commit in this repository's history. It exists only on this machine's disk. Who wrote it and when is unknown.
3. **Uninstalling cleanly is not possible with the tools that exist.** `logan-spine-mcp uninstall` takes no `--clients` flag (`spine/src/cli/cli.c:11598-11642`), so it processes every agent client it detects. On a machine that also has Codex, Cursor or Gemini configured, removing the Claude Code footprint that way also removes theirs.

## Goal

One Claude Code plugin named `logan-spine`, held in this repository, installed through a marketplace, enabled or disabled per repository, carrying every Claude-facing component the spine needs. Nothing the spine installs for Claude Code lives outside the plugin directory except the binary and the `PATH` line that finds it.

Brainstorming for 02 happened in chat on 2026-08-23, the same way it did for 01. There is no `docs/superpowers/02/ideation/` folder and none is planned; the decisions live in this spec.

## Non-goals

- Changing what the engine does. No C code in `spine/src/` is modified by this version. The `install`/`uninstall` subcommands keep working for the other 42 clients; we simply stop calling them for Claude Code.
- Supporting clients other than Claude Code through the plugin. The plugin is a Claude Code artifact.
- Shipping the binary through the plugin system. See "The binary stays outside the plugin".
- Changing indexing behaviour, `auto_index`, the cache location, or anything under `~/.cache/logan-spine-mcp/`.

## Constraints discovered, and what each forces

Each row is a fact verified against the live docs, the live machine, or the vendored source, followed by the design consequence it forces. These are not preferences. Rows marked **UNVERIFIED** are the ones the plan must settle empirically before the work that depends on them.

| Fact | Source | Consequence |
|---|---|---|
| A plugin's MCP tools are named `mcp__plugin_<plugin-name>_<server-key>__<tool-name>`, with any character outside `A-Za-z0-9_-` replaced by `_`; "a hook matcher written against the bare server key never fires for a plugin-bundled server" | `/en/mcp.mdx:454-460` | All 29 `mcp__logan-spine-mcp__*` strings across the three agent files must be rewritten (11 verify, 11 auditor, 7 scout, counted 2026-08-23). Hyphens survive, so the derived name is `mcp__plugin_logan-spine_spine__search_graph`. |
| MCP scope precedence is local, project, **user**, then **plugin-provided**; "plugins and connectors match by endpoint, so one that points at the same URL or command as a server above is treated as a duplicate". Measured on 2026-08-23 with a probe plugin loaded by `--plugin-dir` while the live user-scope entry was still registered: **both servers connected**, listed as `plugin:logan-spine:spine` and `logan-spine-mcp` | `/en/mcp.mdx:546-552`; live `claude --plugin-dir <probe> mcp list` | The old user-scope entry does **not** shadow the plugin's server. Endpoint matching compares the `command` string, and the plugin's command is the launcher path while the user-scope entry's is the binary path, so the two are not duplicates. They coexist, which means the plugin can be proved end to end — graph calls under the new tool names included — before anything is removed. Removal is still required, to stop the hooks firing twice and the same tools appearing under two names, but it is a cleanup step rather than a prerequisite. |
| For a plugin skill, "the frontmatter `name` replaces the directory name in the last segment of the command" | `/en/skills.mdx:367,369` | Copying the installed `SKILL.md` verbatim into `skills/graph/` would still address as `/logan-spine:logan-spine`. Its `name` field must be changed to `graph`. |
| A listed subagent skill that is missing "is skipped, and a warning is logged to the debug log" | `/en/sub-agents.mdx:531` | A wrong `skills:` value in an agent fails silently. It must be checked under `claude --debug`, not by grepping the file. |
| `hooks`, `mcpServers` and `permissionMode` in a plugin agent's frontmatter are ignored, for security. `skills` is supported | `/en/plugins-reference.mdx:62`; `/en/sub-agents.mdx:226,289,292-293` | The agents lose `mcpServers: [logan-spine-mcp]` and `permissionMode: plan`. Their `tools:` allowlists already contain only read-only tools, so the enforced loss is the `plan` permission mode. The removal is complete: the installed files use only `name`, `description`, `tools`, `mcpServers`, `permissionMode`, `skills`. |
| A plugin agent's `name` cannot contain `:`; agents address as `<plugin>:<agent>` | `/en/sub-agents.mdx:284`; `/en/plugins-reference.mdx:462` | An agent named `logan-spine` inside a plugin named `logan-spine` addresses as `logan-spine:logan-spine`. The agents are renamed `scout`, `verify`, `auditor`. |
| The engine's `ha_active_tier` selects the evidence tier by exact string comparison of the `SubagentStart` payload's `agent_type` against `"scout"`, `"logan-spine-scout"`, `"auditor"`, `"logan-spine-auditor"`, defaulting to Tier 2. Measured on 2026-08-23 with a probe plugin: a plugin agent's `SubagentStart` payload carries `agent_type` as `<plugin>:<agent>` | `spine/src/cli/hook_augment.c:1107-1122`; live probe payload | The scoped form matches neither accepted string, so under the rename both scout and auditor would silently become Tier 2. The prefix strip in `subagent-reminder.sh` is **required**, not conditional. See "The tier router" below. |
| A hook matcher containing only letters, digits, `_`, `-`, spaces, `,` and the alternation bar is an exact string or a bar-separated list of exact strings, not a regex | `/en/hooks.mdx:288-294` | One `SessionStart` entry listing all five sources is equivalent to five separate entries. No empirical check needed. |
| `SessionStart` has five sources: `startup`, `resume`, `clear`, `compact`, `fork` | `/en/hooks.mdx:303,1076` | The engine registers only four. A forked session gets no graph reminder today. We add `fork`. |
| Copied plugins cannot reference files outside their own directory; parent-relative paths fail after install because the external files were never copied. A symlink resolving outside the marketplace is skipped for security | `/en/plugins-reference.mdx:807-809,817,819` | The plugin must be self-contained. Nothing in it may reach into `spine/`, and it cannot symlink to `~/.local/bin`. |
| Archive sources are refused above 256 MiB; `command`-source copy mode is refused above 256 MiB or 20,000 entries | `/en/plugin-marketplaces.mdx`, "Zip archives", "Copy mode and link mode" | The 280 MB binary cannot ship inside the plugin under any source type we are using. |
| `${CLAUDE_PLUGIN_ROOT}` is substituted in a stdio server's `command`, `args` and `env` | `/en/mcp.mdx:444-448` | This is the only substitution documented **for a plugin's** `.mcp.json`. |
| The `${VAR}` and `${VAR:-default}` expansion section sits under project-scope `.mcp.json`, and the plugin section names only the three `CLAUDE_*` placeholders. An unexpanded reference is used "as-is" | `/en/mcp.mdx:558-592` against `:444-448` | Plain `${HOME}` in a **plugin** `.mcp.json` is not documented to expand, and a non-expanding value becomes a literal path so the server silently fails to start. We do not depend on it. See "The MCP server". |
| A marketplace install copies the plugin into `~/.claude/plugins/cache/<marketplace>/<plugin>/<version>/`, keyed by the resolved version | `/en/plugin-marketplaces.mdx`, "Plugin sources"; `/en/plugins-reference.mdx:1288-1293`; live `~/.claude/plugins/installed_plugins.json` | The running plugin is a **copy**, not the repo tree. Editing a plugin file changes nothing until the marketplace and plugin are updated. A development loop is mandatory, not optional. |
| `enabledPlugins` honours project and local settings; `pluginConfigs` deliberately does not | `/en/plugins-reference.mdx:598` | Per-repo enable works through `.claude/settings.json`. A `userConfig` value could not have been set per repo even if we had chosen it. |
| `extraKnownMarketplaces` committed to a project's `.claude/settings.json` adds the marketplace for a teammate "once they trust the project folder, with no separate prompt" | `/en/plugin-marketplaces.mdx:704-737` | The repository can carry both halves — the marketplace and the enable — so a clone needs only the binary. |
| There is no `disabledPlugins` key, and "there is no way to disable an individual hook while keeping it in the configuration" | grep of `/en/` returns zero matches; `/en/hooks.mdx:695` | Disabling is `"logan-spine@logan-mem": false` in `enabledPlugins`, or omission. The plugin is the unit of control. |
| `bin/` at a plugin root is added to "the Bash tool's PATH"; the docs never state it reaches an MCP server subprocess | `/en/plugins-reference.mdx:895` | We do not rely on `bin/` being on `PATH` for the MCP server. The server names its launcher by explicit `${CLAUDE_PLUGIN_ROOT}` path. |
| `logan-spine-mcp uninstall` has no `--clients` flag | `spine/src/cli/cli.c:11598-11642` | Removing the current Claude Code footprint requires our own surgical script. |
| Upstream refuses to overwrite or remove an agent file whose bytes match neither the current render nor a known legacy shape | `spine/src/cli/cli.c:7922-7924,10369-10377`; `config_text_edit.c:1276-1347` | Upstream's uninstall would leave the three hand-edited agent files behind. Our script removes them by path. |
| `~/.claude/settings.json`'s `SubagentStart` group contains two handlers: ours and an unrelated tmux subagent counter | live `jq '.hooks.SubagentStart'` on this machine, 2026-08-23 | Removal must operate on individual **handler objects**, never on matcher groups. Deleting the group would destroy the owner's tmux hook. |
| `spine/scripts/setup.sh:202`, `smoke-local.sh`, `smoke-invariants.sh`, `soak-legs.sh` and `benchmark-search-graph.sh` all consume `spine/build/c/logan-spine-mcp` | live grep, 2026-08-23 | `install.sh` must not `mv` the build output away. It copies to a temporary name beside the destination and renames over it. |

## Design

### Repository layout after this version

```
.claude/
  settings.json                 # new: commits the marketplace + the enable for THIS repo
.claude-plugin/
  marketplace.json              # new: the repo is a marketplace named logan-mem
plugins/
  logan-spine/                  # new: the one plugin
    .claude-plugin/plugin.json
    .mcp.json
    bin/
      spine-launch.sh           # resolves the binary and execs it; the MCP server's command
    agents/
      scout.md
      verify.md
      auditor.md
    skills/
      graph/SKILL.md
    hooks/
      hooks.json
      lib.sh                    # shared binary resolution, sourced by every script
      code-discovery-gate.sh
      session-reminder.sh
      subagent-reminder.sh
      docstring-check.sh
    scripts/
      install.sh                # build + place binary + PATH + register marketplace
      unregister-global.sh      # surgical removal of the pre-plugin footprint
      docstring-coverage.sh
    tests/run.sh
    README.md
plugins/logan-spine-tools/      # deleted; absorbed into plugins/logan-spine/
```

`spine/` is untouched. Its location is pinned by git subtree metadata.

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

A relative `source` must begin with `./` and cannot use a parent reference. One marketplace file serves both routes — a local directory add and `pogan314/logan-mem` — and the plugin key is `logan-spine@logan-mem` either way, so a repository's committed settings work for both.

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

No component-path fields; every component sits in its default location and is auto-discovered.

`version` is explicit and bumped by hand, so the plugin is not re-resolved on every unrelated repository commit. The cost of that choice is a development loop, specified below, and it is a real cost during this build: **every task that edits a plugin file must bump `version` and refresh the install, or it is testing a stale copy.**

### The MCP server

`plugins/logan-spine/.mcp.json`:

```json
{
  "mcpServers": {
    "spine": {
      "command": "${CLAUDE_PLUGIN_ROOT}/bin/spine-launch.sh",
      "args": []
    }
  }
}
```

`${CLAUDE_PLUGIN_ROOT}` in a stdio `command` is the one substitution documented for a plugin's own `.mcp.json`. Nothing else is relied on. `spine-launch.sh` sources `hooks/lib.sh`, resolves the binary through the single shared code path, and replaces itself with it:

```bash
#!/usr/bin/env bash
# The MCP server's entry point. Resolves the engine binary and replaces this
# process with it, so stdio passes straight through.
set -u
. "$(dirname "$0")/../hooks/lib.sh"
bin="$(lsm_bin)" || { echo "logan-spine: engine binary not found; see the plugin README" >&2; exit 127; }
exec "$bin"
```

This removes an entire class of risk. Whether `${HOME}` or a nested default expands inside a plugin `.mcp.json` stops mattering, the resolution order becomes identical for the server and the hooks by construction rather than by coincidence, and a missing binary produces one legible line in `/mcp` instead of a literal unexpanded path.

**The server key is `spine`, not `logan-spine-mcp`.** The key becomes part of every tool name; `spine` yields `mcp__plugin_logan-spine_spine__search_graph`, 14 characters shorter on each of 15 tools in every agent allowlist.

### The binary stays outside the plugin

The binary is 280 MB, above the 256 MiB ceiling for every plugin source type that could carry it, and a copied plugin cannot reference anything outside its own directory or symlink out of the marketplace. It stays at `~/.local/bin/logan-spine-mcp`, placed by `install.sh`.

This is a real seam: plugin and binary are versioned and installed separately, and a plugin enabled without the binary is inert. The README says so, `install.sh` does both in one command, and the tests assert the failure is silent for hooks and legible for the server.

### Binary resolution

`hooks/lib.sh` is the single resolution path, sourced by all five hook scripts, by `spine-launch.sh`, and by `docstring-coverage.sh`:

```bash
# Resolve the engine binary. Print its path and return 0, or return 1.
# An explicitly set LOGAN_SPINE_BIN is authoritative: if it is set and not
# executable, that is an error, not a reason to look elsewhere. Silently
# falling through to a different binary than the one the operator named
# would make the override untestable and its failures invisible.
lsm_bin() {
  if [ -n "${LOGAN_SPINE_BIN:-}" ]; then
    [ -x "$LOGAN_SPINE_BIN" ] || return 1
    printf '%s\n' "$LOGAN_SPINE_BIN"
    return 0
  fi
  if [ -x "$HOME/.local/bin/logan-spine-mcp" ]; then
    printf '%s\n' "$HOME/.local/bin/logan-spine-mcp"
    return 0
  fi
  command -v logan-spine-mcp 2>/dev/null && return 0
  return 1
}
```

A set-but-invalid `LOGAN_SPINE_BIN` returning 1 is what makes test 10 meaningful: without it, the absent-binary path can never be exercised on a machine that has the binary installed.

`docstring-coverage.sh` currently invokes `logan-spine-mcp` by bare name and so depends on `PATH`. It moves to `lsm_bin` for the same reason, which also removes its dependence on the `~/.bashrc` line.

### Agents

Three files under `agents/`, renamed to avoid the `logan-spine:logan-spine` collision:

| File | `name` | Addressed as | Was |
|---|---|---|---|
| `agents/scout.md` | `scout` | `logan-spine:scout` | `logan-spine-scout` |
| `agents/verify.md` | `verify` | `logan-spine:verify` | `logan-spine` |
| `agents/auditor.md` | `auditor` | `logan-spine:auditor` | `logan-spine-auditor` |

Content is taken from the **installed files on this machine**, not from the C source, because the installed files carry the graph-unavailable fallback paragraph and the C source does not. The implementation diffs each installed file against a freshly rendered one and records every difference it finds, rather than assuming there is exactly one.

Frontmatter changes:

- `tools:` rewritten from `mcp__logan-spine-mcp__X` to `mcp__plugin_logan-spine_spine__X`.
- `mcpServers: [logan-spine-mcp]` removed — ignored for plugin agents.
- `permissionMode: plan` removed — ignored for plugin agents.
- `skills:` becomes `[graph]`. The namespaced form `logan-spine:graph` is **not documented** for this field — `/en/sub-agents.mdx:515-522` shows bare names, no page shows a namespaced one, and a wrong value fails silently. The plan verifies which form loads under `claude --debug` and uses whichever works, with `graph` as the default.

Prose bodies carry over with two substitutions, not one. Beyond the `tools:` entries, each file names the server in prose — "Use logan-spine-mcp in the exact graph project" and "the `logan-spine-mcp` server being unreachable" — three such occurrences per file. Under the plugin the server registers as `plugin:logan-spine:spine`, so those sentences would name nothing. Every occurrence of the string `logan-spine-mcp` is rewritten, and test 8 asserts the string appears nowhere in any agent file.

### The tier router

`ha_active_tier` (`spine/src/cli/hook_augment.c:1107-1122`) maps the `SubagentStart` payload's `agent_type` to an evidence tier by exact string match, accepting both bare (`scout`, `auditor`) and prefixed (`logan-spine-scout`, `logan-spine-auditor`) forms, and defaulting to Tier 2.

Claude Code reports a plugin agent's `agent_type` as `<plugin>:<agent>` — measured on 2026-08-23 by capturing a real `SubagentStart` payload from a probe plugin's agent. `logan-spine:scout` matches neither accepted form, so without intervention both the scout and auditor tiers silently become Tier 2.

No C change is permitted by the non-goals, so the fix lives in our own hook script: `subagent-reminder.sh` strips a leading `logan-spine:` from `agent_type` before piping the payload to `hook-augment`. The engine already accepts the bare forms `scout` and `auditor`, so the strip is sufficient on its own.

### Skill

`skills/graph/SKILL.md`, addressing as `/logan-spine:graph`. Content is the installed `~/.claude/skills/logan-spine/SKILL.md`, a verbatim copy of the `skill_content` literal at `spine/src/cli/cli.c:1277-1389`, with **one change**: the frontmatter `name` becomes `graph`. Leaving it as `logan-spine` would address the skill as `/logan-spine:logan-spine`, because a plugin skill's frontmatter `name` overrides its directory name.

The `description` frontmatter is preserved exactly, so the same triggers fire. The bare tool names in its tables are prose and stay as written.

### Hooks

`hooks/hooks.json` carries all five hooks — the four the engine installs plus the docstring nudge absorbed from `logan-spine-tools`:

| Event | Matcher | Script | Timeout | Origin |
|---|---|---|---|---|
| `PreToolUse` | Grep or Glob | `code-discovery-gate.sh` | 5 | engine |
| `PostToolUse` | Read | `code-discovery-gate.sh` | 5 | engine |
| `SessionStart` | startup, resume, clear, compact, fork | `session-reminder.sh` | 5 | engine, plus `fork` |
| `SubagentStart` | `*` | `subagent-reminder.sh` | 5 | engine |
| `PostToolUse` | Edit or Write | `docstring-check.sh` | 10 | `logan-spine-tools` |

Alternatives are written in the file as a single matcher string with the sources separated by the alternation bar, exactly as the existing `logan-spine-tools` hook writes `Edit|Write` today.

Every `command` is `"${CLAUDE_PLUGIN_ROOT}"/hooks/<name>.sh`, quoted, matching `/en/plugins-reference.mdx:1248`.

The `SessionStart` matcher is a single entry: matchers of that character class are an exact-string list, not a regex. `fork` is added deliberately — it is a documented fifth source that the engine never registered, so a forked session gets no graph reminder today.

All four graph hook scripts run the same command, `"$BIN" hook-augment`, with no event-specific argument (`spine/src/cli/cli.c:5059-5083`); they differ only in comment headers. They stay separate files so each hook's purpose is legible at its call site. Every graph hook exits 0 silently when the binary is absent, preserving today's fail-open behaviour. The docstring hook keeps its exit-2 contract, which is how it shows Claude a message without failing the tool.

### Per-repo enablement

This repository's `.claude/settings.json` is committed and carries both halves:

```json
{
  "extraKnownMarketplaces": {
    "logan-mem": { "source": { "source": "directory", "path": "." } }
  },
  "enabledPlugins": { "logan-spine@logan-mem": true }
}
```

The marketplace half means a clone does not need a separate `marketplace add`; the enable half means the plugin loads. A repository that lists neither gets nothing.

Two things about this are **UNVERIFIED** and the plan settles both before anything depends on them:

1. Whether an `enabledPlugins` entry alone causes the plugin to load, or whether `claude plugin install logan-spine@logan-mem --scope project` must also run to create a record in `~/.claude/plugins/installed_plugins.json`. On this machine every `enabledPlugins` entry also has an install record; no counter-example was found. **This cannot be settled under a fixture `HOME`**: measured on 2026-08-23, `claude plugin list` there enumerates install records rather than what a session would load, and a session under a fixture `HOME` cannot start at all because the credentials live in the real one. The plan settles it in a scratch **project** directory under the real `HOME`, carrying only a `.claude/settings.json`, since project settings are per-directory and give the isolation the question needs. The answer determines what `install.sh` prints as next steps.
2. The exact JSON shape of a local-directory `extraKnownMarketplaces` entry, and whether a relative `path` resolves against the repository. Live state shows `claude plugin marketplace add` records marketplaces in `~/.claude/plugins/known_marketplaces.json` — three of the five on this machine appear only there and not in `extraKnownMarketplaces` — so the two mechanisms are not interchangeable and the committed form must be confirmed by running it against a fixture `HOME`.

Whichever mechanism wins, the committed file is the per-repo switch and `install.sh` prints the exact commands.

### Development loop

The running plugin is a copy under `~/.claude/plugins/cache/logan-mem/logan-spine/<version>/`, not the repository tree. After editing any plugin file:

1. Bump `version` in `plugins/logan-spine/.claude-plugin/plugin.json`.
2. `claude plugin marketplace update logan-mem`
3. `claude plugin update logan-spine@logan-mem`
4. `/reload-plugins` in the session, or start a new one.

`claude --plugin-dir plugins/logan-spine` loads the tree in place for a single session and is the faster loop for iterating on hooks and agents; it is what the plan uses for verification tasks. The cache path above is what the final success criteria measure, because that is what a normal install produces.

`install.sh` must register the **main checkout**, not the current directory. Run from `.worktrees/plugin-packaging`, a naive `claude plugin marketplace add "$ROOT"` would pin the marketplace to a worktree that is deleted at merge. The script resolves the main checkout from `git rev-parse --path-format=absolute --git-common-dir` and registers its parent.

### Scripts

**`scripts/install.sh`** — one command, same entry point as today:

1. Build via `spine/scripts/build.sh` (unchanged).
2. Place the binary: copy to `"$BIN_DIR/logan-spine-mcp.new"`, then rename over the destination. Renaming over a running binary succeeds where a plain copy fails with "Text file busy", and unlike moving the build output it leaves `spine/build/c/logan-spine-mcp` in place for `smoke-local.sh`, `smoke-invariants.sh`, `soak-legs.sh`, `benchmark-search-graph.sh` and `setup.sh`.
3. Ensure `$BIN_DIR` is on `PATH`: append the export to `~/.bashrc` under our own comment marker if no line already provides it, idempotently. The engine's installer owned this line (`~/.bashrc:134-135`, `# Added by logan-spine-mcp install`); since we stop calling it, we own it.
4. **Removed:** the `logan-spine-mcp install --clients=claude -y` call.
5. **Removed:** the recursive copy into `~/.claude/skills/logan-spine-tools`.
6. `logan-spine-mcp config set auto_index true` (unchanged — the engine's own config under `~/.cache/`, not Claude Code configuration).
7. Register the marketplace against the main checkout, idempotently.
8. Print the per-repo enable command and `/reload-plugins`.

The script must not enable the plugin anywhere on its own. Enabling is a per-repo decision and the point of the version.

**`scripts/unregister-global.sh`** — removes the pre-plugin footprint and nothing else. Required because upstream's `uninstall` has no `--clients` flag and would strip other agent clients from this machine.

- Default is a dry run that prints every change it would make and exits 0. Acting requires `--yes`.
- Refuses to run when `HOME` is unset.
- Backs up `~/.claude/settings.json` and `~/.claude.json` to `<path>.logan-spine-backup-<timestamp>` before writing, and prints both paths in its final output.
- Prints the restore command in its final output: `~/.local/bin/logan-spine-mcp install --clients=claude -y`. The binary survives this script by design, so the old footprint can always be rebuilt.
- **Three-level pruning of `~/.claude/settings.json`**, in this order: remove individual **handler objects** whose `command` contains `lsm-code-discovery-gate`, `lsm-session-reminder` or `lsm-subagent-reminder`; then drop a matcher group only if its `hooks` array became empty; then drop an event only if its group array became empty. Operating on matcher groups would destroy the unrelated tmux subagent counter that shares the `SubagentStart` group on this machine. The script states that `jq` reformats the file, which is why the backup is taken.
- Removes `mcpServers["logan-spine-mcp"]` from `~/.claude.json` and nothing else. That file holds per-project state for every project on the machine.
- Removes the three hook scripts, the three agent files, `~/.claude/skills/logan-spine/` and `~/.claude/skills/logan-spine-tools/`.
- Leaves alone: the binary, `~/.bashrc`, `~/.cache/logan-spine-mcp/`, and every other client's configuration. Removing the index cache would force a full re-index of six projects for no reason.

**`scripts/docstring-coverage.sh`** moves across with one change: it resolves the binary through `lsm_bin` instead of by bare name.

## Order of operations

The migration has a point of no return and one circular-looking dependency. This sequence resolves both and is the skeleton of the plan.

1. **Build the plugin skeleton only** — `marketplace.json`, `plugin.json`, `.mcp.json`, `bin/spine-launch.sh`, `hooks/lib.sh`. No agents, no hooks, no skill.
2. **Load it and harvest the unknowns.** Load the skeleton with `--plugin-dir` and record from live output: the exact MCP tool names; whether `skills: [graph]` or `skills: [logan-spine:graph]` loads; whether an `enabledPlugins` entry alone loads the plugin; and the JSON shape `claude plugin marketplace add` actually writes. Each needs a loaded plugin, which is why the skeleton comes first, and every later task depends on the answers.
3. **Write the components** — agents, skill, hooks, scripts, tests — using the harvested values.
4. **Prove the plugin works end to end while the old footprint is still in place.** Because the old user-scope server does not shadow the plugin's, both connect and the new `mcp__plugin_logan-spine_spine__*` tools are callable at this stage. Everything that can fail is therefore proved here: the plugin loads, its hooks fire, its agents dispatch and reach the graph at their own tiers, its skill resolves, the docstring nudge fires, and `claude plugin list` shows it enabled for this repository and not elsewhere. The visible cost of the overlap is that each graph hook fires twice and the same tools appear under two names.
5. **Run `unregister-global.sh --dry-run`, read the output, then `--yes`.** This is the point of no return, and it is one command from reversible: the script prints the backups and the restore command.
6. **Restart and confirm the cleanup was surgical** — the duplicate tools and duplicate hook firings are gone, the unrelated handlers that shared a group survive, and the plugin still does everything step 4 proved.
7. **Documentation, then merge.**

Step 4 before step 5 is what makes the removal safe to attempt: everything the plugin must do is already demonstrated, so step 5 is only removing a redundant second copy. If step 4 fails, step 5 never runs and nothing was destroyed.

## Tests

`plugins/logan-spine/tests/run.sh` is the old `logan-spine-tools` suite's assertions **plus** the new surface. It must run with no Claude Code session active and must never touch the real `$HOME`.

Carried over from `plugins/logan-spine-tools/tests/run.sh`, unchanged in intent:

1. `docstring-check.sh` caps its output at 10 findings and appends the "… and N more" remainder line.
2. `docstring-check.sh` passes silently on a file whose language the engine does not parse (`.txt`).
3. `docstring-coverage.sh` exits 1 when findings exist, 0 when clean, and 2 when the binary is missing.
4. `docstring-coverage.sh` produces the expected listing shape, is silent on a clean file, and returns the non-git-directory result rather than a false green.

These keep running against the real engine binary when one is resolvable, and skip with a printed notice when it is not, so the engine's `docstrings` contract stays tested somewhere.

New:

5. `claude plugin validate plugins/logan-spine --strict` exits 0, and `claude plugin validate .` (the marketplace root) exits 0.
6. Every JSON file the plugin ships parses; `plugin.json`'s `name` equals the directory name and the marketplace entry's `name`.
7. `hooks.json` declares exactly the five hooks in the table above, with the expected events, matchers and script paths; every referenced script exists and is executable.
8. No agent file contains the string `logan-spine-mcp` anywhere — not in `tools:`, not in prose. Every `mcp__` entry uses the `mcp__plugin_logan-spine_spine__` prefix. No agent file contains `mcpServers:` or `permissionMode:`.
9. `skills/graph/SKILL.md`'s frontmatter `name` equals its directory name and equals the value every agent's `skills:` entry uses.
10. With `LOGAN_SPINE_BIN` set to a non-existent path, each of the five hook scripts exits 0 and prints nothing, and `spine-launch.sh` exits 127 with one line on stderr. This is meaningful because `lsm_bin` treats a set-but-invalid override as fatal rather than falling through.
11. With `LOGAN_SPINE_BIN` pointing at a stub that echoes a marker, each graph hook passes the marker through.
12. `docstring-check.sh` exits 2 and writes to stderr when the stub reports a missing docstring, and exits 0 when it does not.
13. `unregister-global.sh --dry-run` against a fixture `HOME` populated with every artifact in the Problem table lists them all and changes no file. The fixture **must** include a matcher group mixing one `lsm-` handler with one unrelated handler, mirroring the live `SubagentStart` group.
14. The same script with `--yes` against that fixture removes exactly the spine artifacts, leaves the unrelated handler in place inside its group, leaves an unrelated MCP server in `~/.claude.json` and an unrelated agent file untouched, and leaves no empty matcher group or empty event array behind.
15. No tracked file under `plugins/` or `.claude-plugin/` contains an absolute machine path.

Test 15 constrains the documentation: `plugins/logan-spine/README.md` writes the local install route with a placeholder path rather than this machine's. Machine-specific examples belong in the top-level `README.md` and `CLAUDE.md`, which the grep does not cover.

## Documentation

- `CLAUDE.md`: status line moves to version 02; the folder map gains `.claude/`, `.claude-plugin/` and `plugins/logan-spine/` and loses `plugins/logan-spine-tools/`; the "installs once per machine, never per repo" bullet is rewritten to the marketplace-plus-per-repo-enable model; new gotchas record that plugin MCP tool names are `mcp__plugin_logan-spine_spine__*`, and that editing a plugin file does nothing until the version is bumped and the plugin updated.
- `README.md`: install and enable instructions in the new shape.
- `plugins/logan-spine/README.md`: contents, how to enable per repository, that the binary installs separately, how `LOGAN_SPINE_BIN` overrides its location, and the development loop.
- `spine/LOGAN-CHANGES.md`: a new row recording that we no longer call upstream's Claude Code installer, with the reason and date. The engine version does not change; no C code is modified.
- `docs/superpowers/02/plans/`: the implementation plan written from this spec.

## What could go wrong

| Risk | Handling |
|---|---|
| During the overlap in step 4 every graph hook fires twice and the same tools appear under two names | Expected and harmless: `hook-augment` injects context and never blocks, so a doubled injection is noise rather than a failure. Step 5 ends it. |
| `unregister-global.sh` destroys the owner's tmux `SubagentStart` handler | Handler-level removal with three-level pruning, and a fixture test whose `SubagentStart` group deliberately mixes one of ours with one unrelated handler. |
| `unregister-global.sh` damages `~/.claude.json`, which holds per-project state for every project | Dry run by default, timestamped backups, `jq` edit of exactly one key, fixture-`HOME` test asserting unrelated keys survive, restore command printed. Never run against the real `HOME` in tests. |
| A `jq` filter aborts mid-edit and the shell redirect has already truncated the file it was rewriting | Every `jq` rewrite writes to a temporary file and installs it only on success; a failure leaves the original in place, names the backup, and exits non-zero. |
| The plugin does not load after the global footprint is removed | Step 4 proves everything the plugin must do before step 5 removes anything. The script prints both backup paths and the one-command restore. |
| `agent_type` for a plugin agent is scoped, silently demoting scout and auditor to Tier 2 | Measured: it is scoped. `subagent-reminder.sh` strips the prefix, and step 4 confirms each agent reports its own tier. |
| `skills:` takes the wrong form and all three agents silently lose their preloaded skill | Harvested live in step 2 under `claude --debug`, which logs the skip warning. Test 9 pins the value once chosen. |
| Every test measures a stale cached copy of the plugin | The development loop is specified, every editing task bumps `version`, and success criterion 9 asserts the installed copy's version segment matches the manifest. |
| The marketplace is pinned to the worktree, which is deleted at merge | `install.sh` resolves and registers the main checkout via `git rev-parse --git-common-dir`. |
| Losing `permissionMode: plan` lets an agent write a file | Their `tools:` allowlists contain only `Read`, `Grep`, `Glob` and read-only MCP tools; no write tool is reachable. Asserted by test 8. |
| The hand-edited agent text is lost in the move | The move is sourced from the installed files, and the implementation diffs installed against freshly rendered and records every difference before writing. |
| Removing the engine's installer call removes the `PATH` line on a fresh machine | `install.sh` takes ownership of the `~/.bashrc` line, and `docstring-coverage.sh` stops depending on `PATH` by using `lsm_bin`. |

## Success criteria

Done when all of the following hold on this machine, each demonstrated by a command whose output is recorded:

1. `claude plugin list` shows `logan-spine@logan-mem` enabled in this repository.
2. In a repository whose `.claude/settings.json` does not list it, `claude plugin list` does not show it as enabled and `/mcp` does not list the `spine` server.
3. `~/.claude/settings.json` contains zero `lsm-` handlers **and still contains the tmux `SubagentStart` handler**; `~/.claude.json` contains no `mcpServers.logan-spine-mcp`; `~/.claude/hooks/`, `~/.claude/agents/` and `~/.claude/skills/` contain no `logan-spine` artifacts.
4. `/mcp` shows the `spine` server connected, and a graph tool call succeeds under `mcp__plugin_logan-spine_spine__*`.
5. `logan-spine:scout`, `logan-spine:verify` and `logan-spine:auditor` are dispatchable, each reaches the graph, and the tier text in the `SubagentStart` reminder matches the agent dispatched.
6. Editing a file with a missing docstring in a repository with the plugin enabled produces the nudge; doing so in a repository without it produces nothing.
7. `plugins/logan-spine/tests/run.sh` passes.
8. A repository-wide grep for absolute machine paths under `plugins/` and `.claude-plugin/` returns nothing.
9. The version segment of the installed plugin's path under `~/.claude/plugins/cache/logan-mem/logan-spine/` equals `plugin.json`'s `version`.
10. `HOME="$(mktemp -d)" spine/scripts/test.sh --suites cli` reports the version 01 baseline of 273 passed, 10 failed, confirming no C code was touched. The isolated `HOME` is mandatory: see the cli-suite gotcha in `CLAUDE.md`.
