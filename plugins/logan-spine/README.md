# logan-spine

A Claude Code plugin that puts a codebase knowledge graph in front of the model: structural queries instead of grep sweeps, three graph subagents at three evidence tiers, and a nudge when an edit leaves a symbol without a docstring.

It is enabled **per repository**. A machine that has it installed gets nothing until a repository opts in.

## What is in it

| Component | Count | What |
|---|---|---|
| MCP server | 1 | `spine`, launched by `bin/spine-launch.sh`. Its tools address as `mcp__plugin_logan-spine_spine__<tool>` — 15 of them. |
| Agents | 3 | `logan-spine:scout` (Tier 1, provisional), `logan-spine:verify` (Tier 2, the default), `logan-spine:auditor` (Tier 3, exhaustive). |
| Skill | 1 | `graph`, addressed as `/logan-spine:graph`. The tool-choice matrix and the query syntax. |
| Hooks | 5 | Five handler entries across four events — see below. |

The five hook handlers, as declared in `hooks/hooks.json`:

| Event | Matcher | Script |
|---|---|---|
| `PreToolUse` | `Grep\|Glob` | `hooks/code-discovery-gate.sh` |
| `PostToolUse` | `Read` | `hooks/code-discovery-gate.sh` |
| `PostToolUse` | `Edit\|Write` | `hooks/docstring-check.sh` |
| `SessionStart` | `startup\|resume\|clear\|compact\|fork` | `hooks/session-reminder.sh` |
| `SubagentStart` | `*` | `hooks/subagent-reminder.sh` |

`claude plugin details logan-spine` reports "Hooks (4)" because it counts **event types**, not handler entries. Both numbers are right about different things.

## The engine binary installs separately, and the plugin is inert without it

The plugin ships no engine. The engine is `logan-spine-mcp`, a 281 MB native binary that lives at `~/.local/bin/logan-spine-mcp`, outside the plugin directory — it is above the 256 MiB ceiling for every plugin source type that could carry it, and a copied plugin may not reference anything outside its own tree or symlink out of the marketplace.

So plugin and binary are two installs, and an enabled plugin with no binary does nothing:

- All four graph hooks exit 0 and print nothing. They fail open by design; a session simply gets no graph context.
- The docstring nudge exits 0 and prints nothing.
- The MCP server is the one component that says so out loud: `bin/spine-launch.sh` exits 127 with `logan-spine: engine binary not found; see the plugin README` on stderr. Both of those — silent exit 0 from all five hook scripts, exit 127 plus one stderr line from the launcher — are asserted by `tests/run.sh` against a deliberately invalid `LOGAN_SPINE_BIN`.

## Install both

From the repository root:

```bash
plugins/logan-spine/scripts/install.sh
```

That script does the per-machine half, and only the per-machine half:

1. Builds the engine with `spine/scripts/build.sh` (cold build ≈10 minutes without ccache).
2. Places the binary at `~/.local/bin/logan-spine-mcp` (`LSM_BIN_DIR` overrides the directory).
3. Makes sure that directory is on `PATH`, appending to `~/.bashrc` under its own comment marker only if nothing already provides it.
4. Turns on `auto_index`.
5. Registers this repository as a plugin marketplace named `logan-mem`.

It deliberately does **not** enable the plugin anywhere. Enabling is a per-repository decision and it is the whole point of this packaging.

If you are registering the marketplace by hand instead — for a clone that already has the binary, say:

```bash
claude plugin marketplace add /path/to/logan-mem
```

Use the path to your own checkout. `claude plugin marketplace add` always stores an **absolute** path, which is why the registration is per machine and is not committed to any tracked file.

## Enable it for one repository

From that repository's root:

```bash
claude plugin install logan-spine@logan-mem --scope project
```

That writes `.claude/settings.json`:

```json
{
  "enabledPlugins": { "logan-spine@logan-mem": true }
}
```

Then restart Claude Code, or run `/reload-plugins`.

A repository that already commits that entry needs nothing further, once the marketplace is registered under `$HOME` on this machine. Before the marketplace is registered, the committed entry is inert.

## Turn it off again

```bash
claude plugin disable logan-spine@logan-mem --scope project
```

Or set the value to `false`, or delete the key. There is no `disabledPlugins`, and no way to disable one hook while keeping it in the configuration — the plugin is the unit of control.

## `LOGAN_SPINE_BIN`

Every script in the plugin resolves the engine through one shared function, `lsm_bin` in `hooks/lib.sh`, in this order:

1. `$LOGAN_SPINE_BIN`, if set.
2. `$HOME/.local/bin/logan-spine-mcp`, if executable.
3. Whatever `command -v logan-spine-mcp` finds.

**A set-but-invalid `LOGAN_SPINE_BIN` is an error, not a reason to look elsewhere.** If the variable is set and the path is not executable, resolution fails and the caller takes its not-found route — the hooks go silent, `spine-launch.sh` exits 127. It never quietly falls through to a different binary than the one you named, because an override whose failures are invisible cannot be tested or trusted.

Set it to point at a build under `spine/build/c/logan-spine-mcp` when you want a session to exercise a binary you just built without installing it.

## When the graph updates, and what an autonomous run should expect

Measured 2026-08-24 in a repository created fresh for the test.

**A repository is not indexed until something indexes it.** `auto_index` builds an index when a session connects to a project that has none *and* the engine's file-count scan passes, but on a brand-new repository the first session's `SessionStart` context reports `no indexed graph project matched this working directory. Run index_repository before structural exploration` — which is correct and actionable. One `index_repository` call builds it.

**After that, changes are picked up automatically while a session is connected.** The daemon runs one file watcher that polls each *watched* project every 5 s, rising with repository size to a 60 s ceiling, comparing git HEAD and a dirty signature — so it notices uncommitted saves as well as commits, and re-indexes incrementally. Measured: a commit made while a session was connected produced `watcher.changed` then `index.supervisor.reap` in the daemon log, and the new symbol was queryable immediately after.

**A project is only watched while at least one session holds a live connection to it.** The watch is registered after a session's first request and released when the last subscriber disconnects. So a commit made when nothing is connected is not picked up on its own; the next session's activity is what brings the index forward.

**What that means for an autonomous run.** The controlling session stays connected for the whole run, so its project is watched throughout: a change one subagent commits is visible to the next subagent within seconds, with no explicit re-index. Measured directly — a symbol added during a run was found by a query about 6 s later. A subagent working in a **different** repository, or in a git worktree, gets a different index: the project key is the canonicalised absolute path with non-alphanumerics mapped to dashes and never consults git, so a worktree is always its own project with its own database, and parallel worktrees never contend.

## Development loop

When this repository is registered as a marketplace with a `directory` source — which is what `scripts/install.sh` does — **Claude Code loads the plugin from this tree, not from a cache**, so an edit is live in the next session with no version bump. Measured 2026-08-24 from a session's `--debug-file`:

```
Read hooks.json for plugin logan-spine (enabled=true):
  …/plugins/logan-spine/hooks/hooks.json
Attempting to load skills from plugin logan-spine default skillsPath:
  …/plugins/logan-spine/skills
```

The only mention of `~/.claude/plugins/cache/logan-mem/logan-spine/` in an entire session is a sweeper declining to delete the directory. So the loop is: edit, then `/reload-plugins` or start a new session.

An earlier version of this section said the opposite — that the running plugin was the cached copy and every edit needed a version bump plus a three-command update. That was wrong, and following it cost a bump per iteration.

Bumping `version` and running the update is still worth doing when you want the recorded install to match the manifest, since `~/.claude/plugins/installed_plugins.json` is what other tooling reads:

1. Bump `version` in `.claude-plugin/plugin.json`.
2. `claude plugin marketplace update logan-mem`
3. `claude plugin update logan-spine@logan-mem --scope project`

**Step 3 needs the scope you installed at.** `claude plugin update` defaults to `--scope user`; run it bare against a project-scope install and it exits 1 with `Plugin "logan-spine" is not installed at scope user`. Measured 2026-08-24.

The update creates a new cache directory rather than renaming the old one, so `~/.claude/plugins/cache/logan-mem/logan-spine/` accumulates copies that nothing reads. Do not diff against them to decide whether a change is live; read the session's `--debug-file`.

The faster loop, for iterating on hooks and agents, is to load the tree in place for a single session:

```bash
claude --plugin-dir plugins/logan-spine
```

No bump, no marketplace, no cache. Use the four-step loop when you want to measure what a normal install produces.

## Removing the pre-plugin global footprint

Version 01 installed the spine globally through the engine's own multi-client installer: hook handlers and hook scripts under `~/.claude/`, three agent files, two skill directories, and an MCP registration in `~/.claude.json`. `scripts/unregister-global.sh` removes exactly that and nothing else.

It exists because the engine's `uninstall` takes no `--clients` flag, so running it would also strip logan-spine from every other agent client configured on the machine.

```bash
plugins/logan-spine/scripts/unregister-global.sh          # dry run: prints every change, changes nothing
plugins/logan-spine/scripts/unregister-global.sh --yes    # act
plugins/logan-spine/scripts/unregister-global.sh --home DIR --yes   # target a fixture instead of $HOME
```

**The dry run is the default.** Acting requires `--yes`. It copies each configuration file to `<path>.logan-spine-backup-<timestamp>` before rewriting it, names every backup it wrote in its output, and prunes at handler level — a matcher group holding one of ours and one of yours keeps yours. It stops before touching any file on disk if a configuration edit could not succeed.

It leaves alone the engine binary, `~/.bashrc`, the index cache under `~/.cache/logan-spine-mcp/`, and every other client's configuration.

To restore the old global footprint at any time:

```bash
~/.local/bin/logan-spine-mcp install --clients=claude -y
```

## Tests

```bash
plugins/logan-spine/tests/run.sh
```

231 checks as of 2026-08-24, all passing. It never touches the real `$HOME`: every `unregister-global.sh` case runs against a fixture directory passed with `--home`, and the binary-resolution cases run against stubs under a fixture home. The engine-backed docstring cases use the real binary when `lsm_bin` resolves one and print a skip notice when it does not.
