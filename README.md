# logan-mem

A memory system for AI coding agents. Clean rebuild, started 2026-08-21.

- **Stage: version 01 = the spine, built and installed on this machine, merged to `main` (PRs #1-4). `dev/version-01-brainstorming` is tagged `v0.10.8-logan.4`.**
- `docs/wiki/` — research facts about things outside our control (other memory systems, Claude Code's features).
- `docs/superpowers/01/ideation/` — stale scratchpad from before version 01 was designed. Never fact.
- Read `CLAUDE.md` first for the rules.

This repo replaces the abandoned `pogan-mem` / `pogan-toolkit` build, which stays beside it as reference only.

`spine/` is the code map engine (`logan-spine-mcp`, a renamed vendored copy of DeusData/codebase-memory-mcp; see `spine/LOGAN-CHANGES.md` for every divergence from upstream). `plugins/` holds one directory per Claude Code plugin this repo ships; today that is `plugins/logan-spine-tools/`, the spine's plugin, which installs and enforces it: `plugins/logan-spine-tools/scripts/install.sh` builds, installs for Claude Code only, and copies the plugin into `~/.claude/skills/`. Design: `docs/superpowers/01/specs/2026-08-21-spine-v1-design.md`.
