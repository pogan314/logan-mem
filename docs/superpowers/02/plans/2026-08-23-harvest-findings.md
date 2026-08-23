---
title: Harvested live facts for version 02 plugin packaging
type: plan
status: research-fact
created: "2026-08-23 16:05 CDT"
updated: "2026-08-23 18:46 CDT"
sources:
  - "Live Claude Code runs on the EC2 box, 2026-08-23 21:01-21:06 UTC, against a probe plugin at /tmp/lsm-probe/logan-spine built from plugins/logan-spine (task 1)"
  - "Debug logs /tmp/lsm-skill-probe.log and /tmp/lsm-skill-control.log (deleted at cleanup; verbatim lines quoted below)"
  - "Full working notes: .superpowers/sdd/2026-08-23-plugin-packaging-plan/task-2-report.md"
  - "Finding 5 added 2026-08-23 18:46 CDT from the live machine state, not a fixture: jq over ~/.claude/settings.json, ~/.claude/plugins/known_marketplaces.json and ~/.claude/plugins/installed_plugins.json"
---

Every value below was measured, not predicted. Where a measurement contradicts the spec's prediction the measurement wins, and the contradiction is called out in the section that found it. Claude Code version at measurement time is whatever `claude` resolved to on the EC2 box on 2026-08-23; a future version could change any of these, so re-measure before trusting them across an upgrade.

## Finding 1: the MCP tool-name prefix is `mcp__plugin_logan-spine_spine__`

Command:

```
cd /tmp && claude --plugin-dir /tmp/lsm-probe/logan-spine -p "List every tool name available to you that starts with mcp__. Output only the names, one per line, nothing else."
```

The plugin-owned entries in the output, verbatim, were:

```
mcp__plugin_logan-spine_spine__check_index_coverage
mcp__plugin_logan-spine_spine__delete_project
mcp__plugin_logan-spine_spine__detect_changes
mcp__plugin_logan-spine_spine__get_architecture
mcp__plugin_logan-spine_spine__get_code_snippet
mcp__plugin_logan-spine_spine__get_graph_schema
mcp__plugin_logan-spine_spine__index_repository
mcp__plugin_logan-spine_spine__index_status
mcp__plugin_logan-spine_spine__ingest_traces
mcp__plugin_logan-spine_spine__list_projects
mcp__plugin_logan-spine_spine__manage_adr
mcp__plugin_logan-spine_spine__query_graph
mcp__plugin_logan-spine_spine__search_code
mcp__plugin_logan-spine_spine__search_graph
mcp__plugin_logan-spine_spine__trace_path
```

Fifteen tools. The prefix is `mcp__plugin_` + the plugin name `logan-spine` (hyphen preserved) + `_` + the server key `spine` from `.mcp.json` + `__`. This matches the spec's prediction character for character.

The server-registration cross-check is a different fact and confirmed only the plugin-and-server key pair:

```
cd /tmp && claude --plugin-dir /tmp/lsm-probe/logan-spine mcp list 2>&1 | grep -i spine
plugin:logan-spine:spine: /tmp/lsm-probe/logan-spine/bin/spine-launch.sh  - ✔ Connected
logan-spine-mcp: /home/ubuntu/.local/bin/logan-spine-mcp  - ✔ Connected
```

The second line is the pre-existing version-01 user-scope server, unrelated to the plugin. It matters for later tasks only as a reminder that a machine can have both, under two different tool prefixes, at the same time.

Consequence: tasks 4 and 5 write `mcp__plugin_logan-spine_spine__<tool>` wherever a tool must be named in prose, allow-lists, or agent frontmatter. Renaming the plugin or the `.mcp.json` server key changes every one of those strings.

## Finding 2: the bare form `skills: [graph]` loads; the method that proved it can also detect failure

Two agents were dispatched from the probe plugin, one naming the real skill and one naming `totally-nonexistent-skill-xyz`, each under its own `--debug-file`. Both warnings and confirmations appear only at dispatch time, and only in the debug file — never on stdout.

Control run, which had to warn for the probe's silence to mean anything:

```
cd /tmp && claude --debug-file /tmp/lsm-skill-control.log --plugin-dir /tmp/lsm-probe/logan-spine -p "Dispatch the logan-spine:control subagent with the task: say control. Then stop."
grep -n "\[Agent:" /tmp/lsm-skill-control.log
394:2026-08-23T21:02:24.863Z [WARN] [Agent: logan-spine:control] Warning: Skill 'totally-nonexistent-skill-xyz' specified in frontmatter was not found
```

Probe run, same command shape against `skills: [graph]`:

```
grep -n "\[Agent:" /tmp/lsm-skill-probe.log
394:2026-08-23T21:01:59.298Z [DEBUG] [Agent: logan-spine:scout] Preloaded skill 'graph'
grep -n "was not found" /tmp/lsm-skill-probe.log   # no match, exit 1
```

So the result is positive evidence, not merely an absence: the probe log states outright that the skill was preloaded, and the control proves the same log channel reports a failure when there is one. The prefixed form `logan-spine:graph` was never tested, because the decision tree in the brief only reaches it when the bare form warns.

Both runs also show the skill being discovered at session start — `[DEBUG] Attempting to load skills from plugin logan-spine default skillsPath: /tmp/lsm-probe/logan-spine/skills` followed by `[DEBUG] Loaded 1 skills from plugin logan-spine default directory` — so discovery and per-agent preload are two separate events, and only the second one is evidence about the `skills:` frontmatter form.

Consequence: task 5 writes plugin agent frontmatter as `skills: [<skill-directory-name>]`, unprefixed. Neither run produced any output on stdout about skills, so any later verification of this must use `--debug-file` and grep for `[Agent:`.

## Finding 3: an `enabledPlugins` entry alone does NOT load the plugin — the marketplace must also be registered under `$HOME`

This is the one finding that contradicts a hoped-for outcome, and the sequence matters, so it is given in order.

A scratch marketplace was built at `/tmp/lsm-mp` (the repo's `.claude-plugin/marketplace.json` plus a copy of `plugins/logan-spine`), and a scratch project at `/tmp/lsm-proj` whose `.claude/settings.json` was exactly:

```json
{
  "extraKnownMarketplaces": { "logan-mem": { "source": { "source": "directory", "path": "/tmp/lsm-mp" } } },
  "enabledPlugins": { "logan-spine@logan-mem": true }
}
```

With only that file, run from `/tmp/lsm-proj`:

- `claude plugin list` listed no `logan-spine@logan-mem` entry (only the unrelated user-scope skills-directory plugin `logan-spine-tools@skills-dir`).
- `claude plugin marketplace list` did not list `logan-mem` at all.
- `claude mcp list | grep -i spine` showed only the version-01 user-scope server, with no `plugin:logan-spine:spine` line.
- A session asking for every `mcp__` tool containing "spine" returned only `mcp__logan-spine-mcp__*` — no `mcp__plugin_` tool.
- `claude plugin install logan-spine@logan-mem --scope project` failed: `Plugin "logan-spine" not found in marketplace "logan-mem". Your local copy may be out of date` and wrote nothing (the project settings file was byte-identical afterwards, and no other file appeared under `/tmp/lsm-proj`).

**A trap worth recording, because the brief's own command falls into it.** The prescribed check was `claude -p "List the subagent types available to you" | grep -i logan-spine`, and it matched three names — `logan-spine`, `logan-spine-auditor`, `logan-spine-scout`. Those are pre-existing user-scope agent files in `/home/ubuntu/.claude/agents/`, confirmed by `ls`; the plugin from task 1 ships no `agents/` directory at all, and a plugin agent would appear namespaced as `logan-spine:<name>`. Read naively that grep would have certified a load that did not happen. Later tasks should detect this plugin by its `mcp__plugin_logan-spine_spine__` tools or the `plugin:logan-spine:spine` server line, never by a substring of an agent name.

The variable was then isolated. `claude plugin marketplace add /tmp/lsm-mp` was run once against the real `$HOME`, which registered the marketplace but did **not** add `logan-spine` to user-scope `enabledPlugins` (verified by `jq .enabledPlugins ~/.claude/settings.json`, which continued to list only the eight pre-existing plugins). No install was performed. Re-running the same checks from `/tmp/lsm-proj`, whose project settings were the sole thing enabling the plugin:

```
plugin:logan-spine:spine: /tmp/lsm-mp/plugins/logan-spine/bin/spine-launch.sh  - ✔ Connected
```

and a session listing every `mcp__plugin_` tool returned all fifteen `mcp__plugin_logan-spine_spine__*` names.

So: a project-scope `enabledPlugins` entry **is** sufficient to enable an already-known marketplace's plugin, and is **not** sufficient on its own — the marketplace has to be registered under `$HOME` first. What the registration writes is Finding 4. The one thing the measurements do not separate is whether the decisive missing piece was the `known_marketplaces.json` record or the user-scope `extraKnownMarketplaces` key, because `marketplace add` writes both in one action; distinguishing them would take a run that hand-writes one file without the other.

The real `$HOME` was restored afterwards with `claude plugin marketplace remove logan-mem`, and both `~/.claude/settings.json` and `~/.claude/plugins/known_marketplaces.json` were verified byte-identical to backups taken before the experiment (`md5sum` match, empty `diff`).

Consequence: task 9's install instructions cannot be "commit a settings file and you are done". They must include a `claude plugin marketplace add <absolute-path-to-repo>` step per machine. Committing `enabledPlugins` into project settings is still worth doing — after that one-time add, it is what turns the plugin on for the repo without a per-user install.

## Finding 4: `marketplace add` writes two files, and the stored path is always absolute

Run under a fixture `HOME` (no session involved, so a fixture is safe here):

```
HOME=/tmp/lsm-fixture-home2 claude plugin marketplace add /tmp/lsm-mp
Adding marketplace…✔ Successfully added marketplace: logan-mem (declared in user settings)
```

Four files appeared under that fixture home: `.claude.json`, `.claude/settings.json`, `.claude/plugins/known_marketplaces.json`, and a timestamped backup of `.claude.json`. The two that carry the entry:

`~/.claude/plugins/known_marketplaces.json`

```json
{
  "logan-mem": {
    "source": { "source": "directory", "path": "/tmp/lsm-mp" },
    "installLocation": "/tmp/lsm-mp",
    "lastUpdated": "2026-08-23T21:04:33.718Z"
  }
}
```

`~/.claude/settings.json`

```json
{
  "extraKnownMarketplaces": {
    "logan-mem": { "source": { "source": "directory", "path": "/tmp/lsm-mp" } }
  }
}
```

The settings shape is exactly the spec's guess apart from the path value. `known_marketplaces.json` adds two fields the spec did not predict, `installLocation` and `lastUpdated`, and for a `directory` source `installLocation` equals the source path — nothing is copied into a cache directory.

**Relative paths: `.` is rejected, `./` is accepted, and neither is stored.** From inside `/tmp/lsm-mp` against a second fixture home:

```
claude plugin marketplace add .
✘ Invalid marketplace source format. Try: owner/repo, https://..., or ./path   (exit 2, nothing written)

claude plugin marketplace add ./
✔ Successfully added marketplace: logan-mem (declared in user settings)
```

and the `./` run stored `"path": "/tmp/lsm-mp"` in both files. The argument may be relative if it is written in `./` form, but the recorded value is resolved to an absolute path at add time. This contradicts the spec's proposed `{ "source": { "source": "directory", "path": "." } }`: a literal `"."` is not a shape Claude Code ever writes, and task 9 must not instruct anyone to hand-write it.

A completing control also ran under the first fixture home: with the marketplace properly registered there, `claude plugin install logan-spine@logan-mem` succeeded (`scope: user`) and the resulting `settings.json` held precisely the two keys used in Finding 3's failing project file — `extraKnownMarketplaces` plus `"enabledPlugins": { "logan-spine@logan-mem": true }`. The same JSON content therefore works at user scope with the marketplace registered and fails at project scope without it, which is what makes the registration step, not the settings content, the thing task 9 must document.

Consequence for task 9: document `claude plugin marketplace add <absolute path to the repo checkout>` as the per-machine step, state that the recorded path is absolute and therefore machine-specific, and do not commit `extraKnownMarketplaces` with a relative path expecting it to resolve.

## Finding 5: the `known_marketplaces.json` record alone is enough to load a plugin — `extraKnownMarketplaces` is not required

Finding 3 could not separate the two files `marketplace add` writes, because it writes both in one action. The live machine already contains the separation, so no experiment was needed.

`~/.claude/plugins/known_marketplaces.json` lists five marketplaces: `claude-plugins-official`, `global-plugins`, `fitlitics-old-admin-wiki-mcp`, `marketingskills`, `web-app-marketing-marketplace`. `~/.claude/settings.json`'s `extraKnownMarketplaces` lists two: `global-plugins` and `marketingskills`. Three marketplaces therefore have a `known_marketplaces.json` record and no settings key.

`claude-plugins-official` is one of the three, and `superpowers@claude-plugins-official` is listed in `enabledPlugins` and is demonstrably loaded — the session writing this line is running its skills. So a plugin resolves and loads from a marketplace that `extraKnownMarketplaces` does not mention.

What this does and does not establish:

- Established: the settings key is not a prerequisite for loading, at least for `claude-plugins-official`.
- Not established: whether a `directory`-source marketplace such as `logan-mem` behaves the same. `claude-plugins-official` may be special-cased as the built-in. `fitlitics-old-admin-wiki-mcp` is a genuine user-local `directory` marketplace also absent from the settings key, but its plugin's enablement scope was not determined, so it is not proof.

Consequence for task 9, and for resilience. `~/.claude/settings.json` on this machine was observed reverting wholesale to an earlier snapshot on 2026-08-23, losing every edit made in a 21-minute window. If registration depended solely on the settings key, such a revert would silently unregister the marketplace and the plugin would stop loading with nothing in the repository changed. Because `known_marketplaces.json` is a separate file that carries its own record, the registration has a second home. Task 9 should record both files' contents after `install.sh` runs, so a later disappearance can be attributed to the right file.
