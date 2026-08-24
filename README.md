# logan-mem

A memory system for AI coding agents. Clean rebuild, started 2026-08-21.

- **Stage: version 02 = the spine repackaged as a real Claude Code plugin, merged to `main` via PR #8.** Version 01 built the engine (merged to `main` via PRs #1-7, engine tagged `v0.10.8-logan.4`) and installed it globally; version 02 ships the same engine as `plugins/logan-spine`, enabled per repository. No C code changed.
- `docs/superpowers/02/specs/2026-08-23-plugin-packaging-design.md` — the version 02 design, with the Verified table at the end.
- `docs/superpowers/01/specs/2026-08-21-spine-v1-design.md` — the version 01 design (the engine itself).
- `docs/wiki/` — research facts about things outside our control (other memory systems, Claude Code's features).
- `docs/superpowers/01/ideation/` — stale scratchpad from before version 01 was designed. Never fact.
- Read `CLAUDE.md` first for the rules.

This repo replaces the abandoned `pogan-mem` / `pogan-toolkit` build, which stays beside it as reference only.

## What is here

`spine/` is the code map engine (`logan-spine-mcp`, a renamed vendored copy of DeusData/codebase-memory-mcp; see `spine/LOGAN-CHANGES.md` for every divergence from upstream).

`.claude-plugin/marketplace.json` makes this repository a Claude Code plugin marketplace named `logan-mem`. `plugins/` holds one directory per plugin it publishes; today that is `plugins/logan-spine/`, which carries the `spine` MCP server, three graph subagents, the `graph` skill, and five hook handlers. Its own `plugins/logan-spine/README.md` is the reference for the plugin itself.

## Install

> **Installing the plugin on its own does nothing.** `claude plugin install logan-spine@logan-mem` succeeds, reports the plugin enabled, and then stays silent: the hooks print nothing and the MCP server reports `engine binary not found`. The engine is a 282 MiB native binary that cannot ship inside a plugin — Claude Code caps every plugin source at 256 MiB — so it is a separate install. **Clone this repository and run the installer below first.** Verified 2026-08-24 against a clean `HOME`: the marketplace add and the plugin install both succeed and the plugin is still inert without the engine.

Requirements: Linux or macOS, `git`, a C/C++ toolchain (`gcc`/`g++`), `jq`, and the `claude` CLI. Add `node` and `npm` if you want the graph visualizer. There is no Windows path — the installer is bash and the engine is built from C. Budget about 5 GB of disk: the clone is roughly 1.4 GB (the vendored tree-sitter grammars are large), the build adds a couple of gigabytes, and the published binary is another 282 MiB.

Two halves, at two different scopes.

**Per machine** — clone the repository, then build the engine, publish the binary, warm a daemon, and register this clone as a marketplace:

```bash
git clone https://github.com/pogan314/logan-mem
cd logan-mem
plugins/logan-spine/scripts/install.sh              # engine only
plugins/logan-spine/scripts/install.sh --with-ui    # engine + graph visualizer
```

Run it from the clone: it locates the engine source relative to itself, so it needs the whole repository, not just the plugin directory. Keep the clone — the marketplace is registered against that path, and deleting it stops the plugin resolving.

Cold build is roughly ten minutes without ccache. The script runs six steps: build the engine; publish `logan-spine-mcp` to `~/.local/bin` through the engine's own `install --force --skip-config` (so a resident daemon is drained rather than collided with, and no global Claude Code config gets written); put that directory on `PATH`; turn on `auto_index`; **start a permanent daemon**; and register the marketplace. It enables the plugin nowhere: that is the next step, and it is deliberate.

The visualizer is off by default because its assets compile into the binary, so `--with-ui` is a build-time choice. Once installed, toggle the listener with `plugins/logan-spine/scripts/visualizer.sh on|off|status`; it serves every indexed project on `http://127.0.0.1:9749`, machine-wide rather than per repository.

The permanent daemon is not optional if you want the hooks to speak: a hook connects to a daemon but never spawns one, and gives up after 250 ms, while a session's own MCP server takes about 6.3 s to start — so without a warm daemon the first `SessionStart` of a fresh session loses that race and the hooks look broken.

The binary stays outside the plugin because it is 280 MiB (281 MiB with the visualizer embedded), over the 256 MiB ceiling for every plugin source type that could carry it. Plugin and binary are therefore installed and versioned separately, and an enabled plugin with no binary is inert — the hooks go silent and the MCP server reports "engine binary not found".

**Per repository** — from the root of the repository you want it in:

```bash
claude plugin install logan-spine@logan-mem --scope project
```

That writes `"enabledPlugins": { "logan-spine@logan-mem": true }` into that repository's `.claude/settings.json`. Restart Claude Code, or run `/reload-plugins`. This repository already commits that entry, so on a machine where the marketplace is registered it needs nothing further.

To turn it off again:

```bash
claude plugin disable logan-spine@logan-mem --scope project
```

The marketplace registration stores an absolute path, so it is per machine and is never committed. On a machine that already has the binary, register it by hand instead of running the installer:

```bash
claude plugin marketplace add /path/to/logan-mem
```

## Removing the version 01 global footprint

Version 01 installed hooks, agents, a skill and an MCP registration into `~/.claude/`, which every repository on the machine then loaded. `plugins/logan-spine/scripts/unregister-global.sh` removes exactly that, and nothing else. It is a dry run by default; `--yes` acts; it copies each configuration file it is about to rewrite to a timestamped backup and names it; and it prints the one command that restores the old footprint. It was run on this machine on 2026-08-24, and `docs/superpowers/02/specs/2026-08-23-plugin-packaging-design.md`'s Verified table records what was checked afterwards.
