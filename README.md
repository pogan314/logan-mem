# logan-mem

A memory system for AI coding agents. Clean rebuild, started 2026-08-21.

- **Stage: version 01 = the spine, built on branch `dev/spine-v1`; not yet installed on any machine.**
- `docs/wiki/` — research facts about things outside our control (other memory systems, Claude Code's features).
- `docs/superpowers/01/ideation/` — stale scratchpad from before version 01 was designed. Never fact.
- Read `CLAUDE.md` first for the rules.

This repo replaces the abandoned `pogan-mem` / `pogan-toolkit` build, which stays beside it as reference only.

`spine/` is the code map engine (`logan-spine-mcp`, a renamed vendored copy of DeusData/codebase-memory-mcp; see `spine/LOGAN-CHANGES.md` for every divergence from upstream). `plugin/` is the Claude Code plugin that installs and enforces it: `plugin/scripts/install.sh` builds, installs for Claude Code only, and copies the plugin into `~/.claude/skills/`. Design: `docs/superpowers/01/specs/2026-08-21-spine-v1-design.md`.
