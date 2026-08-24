---
title: Version 02 — package the spine as a real Claude Code plugin
type: spec
status: decided
created: "2026-08-23 14:42 CDT"
updated: "2026-08-24 09:59 CDT"
sources:
  - "The Verified table's evidence: .superpowers/sdd/2026-08-23-plugin-packaging-plan/task-9-report.md (including its appended 'Steps 5 and 6, under an isolated runtime' section) and task-10-report.md"
  - "Live shell in the worktree at .worktrees/plugin-packaging, 2026-08-24 09:12-09:33 CDT, for criteria 1, 2, 3, 7, 8, 9 and 10, and for the post-cutover measurements added to criteria 4 and 5"
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

This is the manifest as task 1 creates it. The `version` field advances through the build — the shipped value is in `plugins/logan-spine/.claude-plugin/plugin.json`, not here.

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

This repository's `.claude/settings.json` is committed and carries one key:

```json
{
  "enabledPlugins": { "logan-spine@logan-mem": true }
}
```

`extraKnownMarketplaces` is deliberately **not** committed alongside it. Measured on 2026-08-23: `claude plugin marketplace add` always stores an absolute path in that entry — a bare `.` is rejected outright and `./` is accepted but resolved — so the portable shape this spec originally proposed is one Claude Code never writes. An absolute path in a tracked file is a machine path, which this repository forbids and which is wrong on any other machine by construction.

Registering the marketplace is therefore a per-machine step, and `install.sh` performs it. That is not an extra burden: measured in the same session, a project-scope `enabledPlugins` entry does **not** load the plugin until the marketplace is registered under `$HOME`, and a fresh clone has to install the 280 MB binary anyway. Once the marketplace is registered, the committed `enabledPlugins` entry alone is sufficient. A repository that does not carry that entry gets nothing.

Both halves of this were open when the spec was first written and are now measured, on 2026-08-23:

1. A project-scope `enabledPlugins` entry alone does **not** load the plugin. The marketplace must first be registered under `$HOME` with `claude plugin marketplace add`; after that, the committed entry is sufficient on its own. One thing remains unknown and is recorded as such: `marketplace add` writes both `~/.claude/plugins/known_marketplaces.json` and `~/.claude/settings.json`'s `extraKnownMarketplaces` in the same command, so which of the two a project-scope enable actually depends on was not separated. It does not matter operationally, because the one command `install.sh` runs writes both.
2. `marketplace add` always stores an absolute path. This is what rules `extraKnownMarketplaces` out of the committed file, above.

The committed file is the per-repo switch; `install.sh` handles the per-machine half and prints the exact commands.

### Development loop

The running plugin is a copy under `~/.claude/plugins/cache/logan-mem/logan-spine/<version>/`, not the repository tree. After editing any plugin file:

1. Bump `version` in `plugins/logan-spine/.claude-plugin/plugin.json`.
2. `claude plugin marketplace update logan-mem`
3. `claude plugin update logan-spine@logan-mem --scope project`
4. `/reload-plugins` in the session, or start a new one.

Step 3's `--scope project` was added on 2026-08-24, after the bare form failed in task 11: `claude plugin update` defaults to `--scope user`, and against this project-scope install it exits 1 with `Plugin "logan-spine" is not installed at scope user`. Measured in the same run: the update does **not** rename the cache directory; it creates a new one for the new version and leaves the previous one beside it, so `~/.claude/plugins/installed_plugins.json` rather than a directory listing is what says which copy is in use.

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

## Verified

One row per success criterion above. Every command named here was run and its output recorded; the evidence is `.superpowers/sdd/2026-08-23-plugin-packaging-plan/task-9-report.md` (including its appended "Steps 5 and 6, under an isolated runtime" section), `task-10-report.md`, and a live shell in this worktree on 2026-08-24 between 09:12 and 09:58 CDT.

**Read the Scope column before the Verdict.** Two rows — 4 and 5 — were measured against an **isolated** LSM daemon and an **isolated, initially empty** cache, reached by relocating the engine's rendezvous with `LSM_RUNTIME_DIR=/run/user/1000/lsm-iso-runtime` and its cache with `LSM_CACHE_DIR=~/.cache/lsm-iso-cache`. That is **not** the machine's shared per-account daemon and **not** the real `~/.cache/logan-spine-mcp/` index. Neither row may be read as a measurement of the machine's normal runtime.

| # | Criterion | Command run | Recorded output | Scope | Verdict |
|---|---|---|---|---|---|
| 1 | `claude plugin list` shows `logan-spine@logan-mem` enabled in this repository | `claude plugin list` in the worktree root, 2026-08-24 09:16 CDT | `logan-spine@logan-mem` / `Version: 1.0.0` / `Scope: project` / `Status: ✔ enabled` | Shared runtime, after cutover, at manifest 1.0.0. No daemon involved | **Verified.** |
| 2 | A repository that does not list it does not show it enabled, and `/mcp` does not list the `spine` server there | `cd /tmp && claude plugin list`; `cd /tmp && claude mcp list \| grep -i spine`, 2026-08-24 09:16 CDT | `Status: ✘ disabled`; the grep matched nothing, so the fallback line `no spine server listed outside the enabled repo` printed instead | Shared runtime, after cutover | **Verified.** Task 9 measured the same two things before cutover, when the old user-scope `logan-spine-mcp` was still listed from `/tmp`; task 10 removed it, so now no spine server of any kind is listed outside the enabled repository. |
| 3 | Zero `lsm-` handlers in `~/.claude/settings.json`, the tmux `SubagentStart` handler survives, no `mcpServers.logan-spine-mcp`, no `logan-spine` artifacts under `~/.claude/{hooks,agents,skills}` | `jq '[.hooks[]?[]?.hooks[]? \| select((.command // "") \| test("lsm-"))] \| length' ~/.claude/settings.json`; `jq -c '.hooks.SubagentStart' ~/.claude/settings.json`; `jq '.mcpServers \| has("logan-spine-mcp")' ~/.claude.json`; `ls ~/.claude/hooks ~/.claude/agents ~/.claude/skills \| grep -i logan-spine`, 2026-08-24 09:16 CDT | `0`; the `SubagentStart` group holds exactly one handler, the tmux subagent counter; `false`; no match, so `no logan-spine artifacts remain` printed | Shared runtime, after cutover | **Verified.** Task 10 additionally proved the removal was surgical by diffing complete before/after captures rather than spot checks: 16 `event\|matcher\|command` handler triples before and 9 after, with seven lines removed and **zero added or altered**; 9 MCP servers before and 8 after, the diff showing only `logan-spine-mcp`; `diff <(jq -S 'del(.hooks)' pre) <(jq -S 'del(.hooks)' post)` identical. |
| 4 | `/mcp` shows the `spine` server connected, and a graph tool call succeeds under `mcp__plugin_logan-spine_spine__*` | Under isolation, 2026-08-23 21:50-21:56 CDT: `claude mcp list \| grep -iE 'spine'`; `claude -p "List every tool name available to you that starts with mcp__plugin_logan-spine…"` then `grep -c '^mcp__plugin_logan-spine_spine__'`; `claude -p "Using the mcp__plugin_logan-spine_spine__list_projects tool, list the indexed graph projects."` — and against the shared daemon, `claude mcp list \| grep -i spine` in the worktree, 2026-08-24 09:16 CDT | Isolated: `plugin:logan-spine:spine: …/bin/spine-launch.sh - ✔ Connected`; the tool-name count returned `15`; `list_projects` resolved and returned `{"projects":[],"total":0}` with the hint `No projects indexed. Call index_repository(repo_path=...) first.`, neither erroring nor timing out. Shared: `plugin:logan-spine:spine: … - ✘ Failed to connect — connection timed out after 30000ms`, and probing the launcher directly returned `{"jsonrpc":"2.0","id":null,"error":{"code":-32001,"message":"LSM daemon is active or starting but could not accept this client within 30000 ms"}}` | **Connected, 15 tool names and the graph call: ISOLATED daemon and ISOLATED, initially empty cache only** (`LSM_RUNTIME_DIR=/run/user/1000/lsm-iso-runtime`). The `mcp list` line quoted second is the shared runtime, after cutover | **Partly verified.** *Proved:* the plugin's server registers under the predicted key, connects, its 15 tools resolve under the `mcp__plugin_logan-spine_spine__` prefix, and a graph call succeeds — **all under isolation**, and the successful call read a **fresh, empty** cache, so a call against a populated graph is **not** proved. *Proved separately, on the shared runtime:* the overlap is gone — `claude mcp list` lists only `plugin:logan-spine:spine`, with no `logan-spine-mcp`. *Not proved:* the server connecting against the machine's shared daemon. That failure is **not** a version conflict, and two controls in task 10 separate the causes: the shared daemon still serves CLI clients (`~/.local/bin/logan-spine-mcp config get auto_index` → `true`), and the same launcher under an isolated rendezvous completes an MCP `initialize` returning `serverInfo.version 0.10.8-logan.4-46-gdc37c42`. So the plugin, its launcher and its binary resolution work, and the shared daemon refuses new MCP clients. **The cause is not established and none is offered here.** *New at 09:30 CDT, after the cli suite in row 10 ended the machine's daemon:* `claude mcp list` in this worktree now reports `plugin:logan-spine:spine: … - ✔ Connected` against the **normal, non-isolated** runtime, with no LSM process left resident afterwards. **But the tools still do not reach a session:** `claude --debug-file /tmp/lsm-t11.log -p "Using the mcp__plugin_logan-spine_spine__list_projects tool…"` answered `TOOL ABSENT`, and that log names eight MCP servers, none of them the plugin's, with zero occurrences of `spine-launch` — while the same log shows the plugin itself loading fully (`Read hooks.json for plugin logan-spine (enabled=true)`, `Loaded 3 agents`, `Loaded 1 skills`). This is the exact check task 9 proposed to settle its second open question, run now that both the version conflict and the global footprint are gone. It did not settle it. **RESOLVED — this row is verified on the shared runtime, including the graph call.** An earlier revision of this row claimed that on the strength of an `initialize`, a `tools/list` and a `claude -p` returning 15 tool *names*. Those are listings, not a call, and the criterion asks for a call; the claim was corrected rather than left standing. The measurement that actually meets it, run 2026-08-24 09:44 CDT with `env -u LSM_RUNTIME_DIR -u LSM_CACHE_DIR` and the current build installed: `initialize`, `notifications/initialized`, then `tools/call` for `list_projects`, piped into `plugins/logan-spine/bin/spine-launch.sh`. It returned `{"projects":[{"name":"home-ubuntu-.claude",...},{"name":"home-ubuntu-projects-apps-beezys-bagels",...},...]}`, **10 projects**, from the real `~/.cache/logan-spine-mcp/` index — so the graph call is proved on the shared runtime against a **populated** cache, not merely an empty one. **One gap in that probe, closed separately.** Piping into the launcher calls the server directly with the bare tool name `list_projects`, so it never exercises the `mcp__plugin_logan-spine_spine__` prefix this criterion names. Run 2026-08-24 09:58 CDT to close it: `claude -p "Call the tool mcp__plugin_logan-spine_spine__list_projects with no arguments. Print the raw result verbatim and nothing else."` returned the same populated project list. So the call is proved through Claude Code's own tool routing under the prefixed name, not only at the server socket. Once no daemon held the version cohort, `plugins/logan-spine/scripts/install.sh`'s binary step was completed by hand — the current build placed at `~/.local/bin/logan-spine-mcp`, reporting `0.10.8-logan.4-46-gdc37c42` — and `config set auto_index true` then completed, returning `auto_index = true`, the first time that has been observed on this machine. With that in place and **no** `LSM_RUNTIME_DIR` or `LSM_CACHE_DIR` set: piping `initialize` and `tools/list` into `plugins/logan-spine/bin/spine-launch.sh` returned `serverInfo` `logan-spine-mcp 0.10.8-logan.4-46-gdc37c42` and **exactly 15 tools**; and `claude -p "List every tool name available to you that starts with mcp__plugin_logan-spine"` in this worktree returned all **15** `mcp__plugin_logan-spine_spine__*` names. So the server connects, its tools resolve, and a real child session receives them, all on the machine's normal runtime after cutover. The earlier `-p` failures are therefore not a standing defect. **What made the difference was not isolated:** two things changed between the failing and passing runs — the resident daemon ended, and the installed binary was replaced with the current build. No command was run that attributes the fix to one of them. |
| 5 | The three agents are dispatchable, each reaches the graph, and the tier text in the `SubagentStart` reminder matches the agent dispatched | Under isolation, 2026-08-23: `claude --debug-file iso-scout-clean.log -p "Dispatch the logan-spine:scout subagent…"`, likewise `iso-auditor-clean.log` for `logan-spine:auditor` and `iso-tier.log` for `logan-spine:verify`; then `grep -nE 'Hook SubagentStart' <log>` on each | Three different tiers, one per agent: `Active tier: Tier 1 quick scout.`, `Active tier: Tier 3 full graph verification.`, `Active tier: Tier 2 verification.` Each attributed by its own debug log to `Hook SubagentStart ("${CLAUDE_PLUGIN_ROOT}"/hooks/subagent-reminder.sh) provided additionalContext`. The stop-and-report condition — all three reporting Tier 2 — was **not** observed | **ISOLATED daemon and ISOLATED, initially empty cache only.** No post-cutover tier measurement succeeded | **Partly verified, and only with a precondition.** *Proved:* the plugin's `logan-spine:` prefix strip and the engine's tier mapping are correct — `Tier 1 quick scout` and `Tier 3 full graph verification` cannot come from the version-01 global hook at all, since it passes `agent_type` through unstripped and `ha_active_tier` matches by exact string. *The precondition:* two of the three dispatches needed the version-cohort race for the isolated rendezvous **biased deliberately**, by holding the cohort with the new build (`setsid bash -c 'sleep 600 \| "$LOGAN_SPINE_BIN"'`). Without that bias, two other `logan-spine:scout` dispatches took their context from the old global hook and reported `Active tier: Tier 2 verification` three times over — the brief's own stop-and-report signature, produced by footprint contamination rather than a broken strip. So this does **NOT** prove tiering was reliable on this machine while the version-01 footprint was still installed. Task 10 removed that contention; **no tier measurement has been taken since, and task 10's scout Tier 1 re-check was not re-proved against the shared daemon.** *"Each reaches the graph" is only partly met:* the first scout and auditor runs reported `search_graph` and `check_index_coverage` results (generation `2026-08-24T02:52:48Z`, `generation_matches: true`), while later runs in the same window had no graph tools at all. *A post-cutover tier measurement was attempted on 2026-08-24 at 09:31 CDT and did not produce one:* `claude --debug-file /tmp/lsm-t11-scout.log -p "Dispatch the logan-spine:scout subagent…"` dispatched and returned, but the string `SubagentStart` does not appear anywhere in that debug log and no `Active tier:` string was emitted. Probing the hook directly, `subagent-reminder.sh` exits 0 printing nothing, and `logan-spine-mcp hook-augment` for `agent_type: scout` in this worktree exits 0 emitting no JSON at all. Two facts observed alongside it, with **no causal link established between them and the empty output**: `~/.cache/logan-spine-mcp/` holds no database for this worktree (only `home-ubuntu-projects-org-logan-mem.db`, the main checkout, dated Aug 22), and `~/.local/bin/logan-spine-mcp` at that moment reported build `bb71bffe…`, the version 01 build. **That is now explained:** the controller replaced it deliberately at 21:43 on 2026-08-23, to stop the machine being degraded while an unrelated long-running session held the cohort with the old build; it is recorded in the run ledger. It was not an unknown agent. **PARTLY RESOLVED — the tier half is now verified on the shared runtime with no precondition; the "each reaches the graph" half is not.** An earlier revision of this row claimed the whole criterion; that was wrong and is corrected here. With the current build installed, no daemon resident, no `LSM_RUNTIME_DIR` set, and the version-01 footprint already removed by task 10, a single `claude -p` session dispatched all three agents in turn and reported: `logan-spine:scout -> tier: Tier 1 quick scout`, `logan-spine:auditor -> tier: Tier 3 full graph verification`, `logan-spine:verify -> tier: Tier 2 verification`. Three distinct tiers, one per agent, **with no cohort race to bias** — the old contending hook no longer exists — so the earlier precondition no longer applies to the tier claim. The stop-and-report condition of all three reporting Tier 2 was not observed. **Still not proved:** "each reaches the graph". That 09:38 run measured tiers only. `logan-spine:verify` has never been observed reaching the graph, under isolation or on the shared runtime; `scout` and `auditor` were observed doing so only under isolation, against an empty cache. The plugin's server does serve a real graph call on the shared runtime — see row 4 — but no *agent* has been observed making one there. |
| 6 | The docstring nudge fires in a repository with the plugin enabled and not in one without it | Task 9 step 8: a child session removed one docstring line with the `Edit` tool from a bash-created, verified-clean `nudge-probe.py` in this worktree; the identical edit through the identical prompt in a fresh `mktemp -d` + `git init` repository at `/tmp/lsm-task9-control-5WBZcX` with no `.claude` directory | Here, two nudges, one of them from the plugin: `PostToolUse:Edit hook blocking error from command: ""${CLAUDE_PLUGIN_ROOT}"/hooks/docstring-check.sh" … logan-spine: add docstrings before moving on:` naming `…/nudge-probe.py:4 function alpha`. In the control, exactly one nudge, from the old global `"${CLAUDE_PLUGIN_ROOT}"/scripts/docstring-check.sh`, and `grep -nE 'plugin logan-spine[^-]'` on that session's debug log returned no match | Shared runtime, **before** cutover | **Verified, by script path rather than by message text.** The version-01 global nudge was still installed and prints an identical message, so a naive "did a nudge appear?" test could not have told them apart; the two were separated by the `hooks/` versus `scripts/` path Claude Code prints in the hook line. Task 10 has since removed the old hook, so the second nudge no longer exists — **that post-cutover state was not re-measured.** |
| 7 | `plugins/logan-spine/tests/run.sh` passes | `plugins/logan-spine/tests/run.sh` in the worktree, at manifest version 1.0.0, 2026-08-24 09:15 CDT | 199 `ok` lines, zero failures, `exit=0` | Shared runtime, after cutover. Uses stubs and a fixture `HOME`; never touches the real `$HOME` | **Verified.** |
| 8 | A repository-wide grep for absolute machine paths under `plugins/` and `.claude-plugin/` returns nothing | `git grep --untracked -lE '/[h]ome/\|/[U]sers/\|[C]:\\' -- plugins .claude-plugin \| wc -l`, 2026-08-24 09:13 CDT; also asserted inside the suite | `0`, and the suite line `ok   no absolute machine path under plugins/ or .claude-plugin/` | Static; no runtime involved | **Verified**, and `--untracked` means the check covered the new `plugins/logan-spine/README.md` before it was committed. |
| 9 | The version segment of the installed plugin's path equals `plugin.json`'s `version` | `jq -r .version plugins/logan-spine/.claude-plugin/plugin.json`; `claude plugin update logan-spine@logan-mem --scope project`; `ls ~/.claude/plugins/cache/logan-mem/logan-spine/`; `jq -S '.plugins["logan-spine@logan-mem"]' ~/.claude/plugins/installed_plugins.json`; `diff -r ~/.claude/plugins/cache/logan-mem/logan-spine/1.0.0 plugins/logan-spine`, 2026-08-24 09:15 CDT | Manifest `1.0.0`; update printed `✔ Plugin "logan-spine" updated from 0.7.5 to 1.0.0 for scope project`; the cache directory holds **two** entries, `0.7.5` and `1.0.0`; `installed_plugins.json` gives `"version": "1.0.0"` and `"installPath": "…/logan-mem/logan-spine/1.0.0"`; `diff rc=0` | Shared runtime, after cutover | **Verified**, with two corrections to what the plan expected. (a) The directory is **not renamed** — `claude plugin update` creates a new `1.0.0` directory and leaves the stale `0.7.5` beside it. The criterion is about the *installed* plugin's path, and `installed_plugins.json` names `1.0.0`, so it holds; manifest-versus-loaded agreement was additionally confirmed by `diff -r` returning 0 against the repository tree and by `claude plugin details logan-spine` printing `logan-spine 1.0.0`. (b) `claude plugin update logan-spine@logan-mem` **as the plan wrote it fails**: it defaults to `--scope user` and this plugin is installed at project scope, so it exits 1 with `Plugin "logan-spine" is not installed at scope user`. `--scope project` is required. |
| 10 | `HOME="$(mktemp -d)" spine/scripts/test.sh --suites cli` reports the version 01 baseline of 273 passed, 10 failed | `HOME="$(mktemp -d)" spine/scripts/test.sh --suites cli`, 2026-08-24 09:19-09:30 CDT (about 11 minutes, most of it rebuilding the sanitized `test-runner`) | `273 passed, 10 failed`, `[exited with code 0]` | Isolated `HOME`, as `CLAUDE.md` requires; a result taken against the real `$HOME` is meaningless | **Verified — exactly the version 01 baseline**, so the non-goal "no C code in `spine/src/` is modified by this version" holds. Confirmed independently: `git diff --stat dev/version-01-brainstorming...HEAD -- spine/` is empty, and the only working-tree change under `spine/` is `LOGAN-CHANGES.md`. **Machine-state consequence:** the machine's LSM daemon (pid 1094721) and the client holding it (pid 1094677) were alive before this run and gone after it; `pgrep -af logan-spine-mcp` returned nothing. `CLAUDE.md` records that this suite kills live LSM sessions, but the moment of death was not captured, so the suite is **not established** as what ended them. The owner's `claude --model fable` session (pid 1094422) itself survived. |

### What is left unproved, in one place

Everything here is a gap in the *evidence*, not a known defect. Where a cause is unknown it is written as unknown, with the command that would find out.

- ~~**A `claude -p` session in this worktree does not receive the plugin's MCP tools.**~~ **Resolved 2026-08-24 09:36 CDT** — all 15 `mcp__plugin_logan-spine_spine__*` names were returned by a `claude -p` session on the normal runtime, after the current build was installed and the resident daemon had ended. The original observation stands as recorded: Measured 2026-08-24 09:30 CDT with the version conflict gone, the machine's daemon gone, and the version-01 footprint gone: the debug log names eight MCP servers, none of them the plugin's, and never mentions `spine-launch`, while the plugin's hooks, agents and skill all load in that same session. Task 9 saw the same shape from about 22:00 on 2026-08-23 and could not explain it either; it *had* previously seen the plugin server named in a `-p` debug log under isolation, so "plugin MCP servers never start in `-p` mode" is ruled out. **I do not know why.** What would find out: capture the launcher's own stderr from inside a `-p` session, or compare a `--plugin-dir` run against the marketplace-installed run in the same directory.
- ~~**No tier measurement has succeeded outside the isolated runtime.**~~ **Resolved 2026-08-24 09:38 CDT** — Tier 1 / Tier 3 / Tier 2, one per agent, measured on the shared runtime after cutover with no race to bias. The original limitation, now superseded: The post-cutover attempt on 2026-08-24 produced no `SubagentStart` line and no `Active tier:` string at all, and `hook-augment` returns exit 0 with empty output for this worktree. Criterion 5 therefore rests entirely on the isolated measurement, with the cohort-bias precondition stated in its row.
- ~~**A graph call against a populated graph is unproved.**~~ **Resolved 2026-08-24 09:44 CDT** — `tools/call` for `list_projects` through the launcher on the shared runtime, no isolation, returned 10 projects from the real index. See row 4 for the command. The original note, now superseded: the one earlier call read a fresh, empty isolated cache and correctly reported zero projects.
- **No AGENT has been observed reaching the graph on the shared runtime.** This is the half of criterion 5 that an earlier revision of this document wrongly folded into a "resolved" claim. The server serves graph calls there (row 4) and the three agents receive their correct tiers there, but the two facts have not been joined by a measurement of an agent making a graph call outside the isolated runtime. `logan-spine:verify` has not been observed reaching the graph anywhere. What would find out: dispatch each agent with a task that forces a graph tool call and capture the tool-use records.
- ~~**`~/.local/bin/logan-spine-mcp` is the version 01 build, and what replaced it is unknown.**~~ **Resolved** — the controller replaced it deliberately at 21:43 on 2026-08-23 and recorded it in the ledger; the current build has since been installed and `install.sh` step 4 completed, returning `auto_index = true`. The original note: (`0.10.8-logan.4`, build `bb71bffe…`), not the `0.10.8-logan.4-46-gdc37c42` / `fc9a1cc5…` that `plugins/logan-spine/scripts/install.sh` placed during task 9. Its mtime is 2026-08-23 21:43. **What replaced it is unknown**; task 9 recorded the same unknown. Re-running `install.sh` would put the current build back, and `install.sh` step 4 (`config set auto_index true`) has never been observed to complete on this machine.
- **This worktree has no index in the real cache.** `~/.cache/logan-spine-mcp/` holds `home-ubuntu-projects-org-logan-mem.db` from 2026-08-22 (the main checkout) and nothing for `.worktrees/plugin-packaging`. Whether that is connected to the empty `hook-augment` output above is **not established**.
- **The control-repository half of the docstring criterion has not been re-measured since the version-01 nudge was removed.** It passed while both nudges existed, attributed by script path.
