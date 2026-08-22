# logan-spine-tools

The Claude Code plugin half of logan-spine. The engine is `logan-spine-mcp` in `../spine/` (our renamed vendored copy of DeusData/codebase-memory-mcp; every divergence is logged in `../spine/LOGAN-CHANGES.md`).

## What is in here

- `hooks/hooks.json` — a `PostToolUse` hook on `Edit|Write` that runs `scripts/docstring-check.sh`. After any edit it runs `logan-spine-mcp docstrings <file>` and, if anything is missing, prints the first ten findings to Claude (exit 2, so the edit itself is not marked failed). It nudges; it does not block.
- `scripts/docstring-coverage.sh [--all] [dir]` — the same check over every tracked file in a git checkout. Exported symbols only unless `--all`.
- `scripts/install.sh` — builds the engine, copies the binary to `~/.local/bin` (override with `LSM_BIN_DIR`), runs `logan-spine-mcp install --clients=claude -y`, turns `auto_index` on, and copies this folder to `~/.claude/skills/logan-spine-tools/`, where Claude Code loads it as a skills-directory plugin on the next session.
- `tests/run.sh [path-to-binary]` — bash tests for the two scripts.

## Why the MCP server is not bundled in this plugin

Upstream's installer registers the server in `~/.claude.json` under the name `logan-spine-mcp`, and its generated skill and three subagents hard-code tool names like `mcp__logan-spine-mcp__search_graph`. A plugin-bundled server would be renamed `mcp__plugin_logan-spine-tools_<server>__<tool>` and break all of them. Do not move the server into a `.mcp.json` here.

## Uninstall

`logan-spine-mcp uninstall` (removes upstream's skill, agents, hooks, and the `~/.claude.json` entry), then `rm -rf ~/.claude/skills/logan-spine-tools`.
