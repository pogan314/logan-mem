# logan-mem

A memory system for AI coding agents. Clean rebuild, started 2026-08-21.

- **Stage: ideation.** Nothing is built or installed.
- `docs/wiki/` — research facts about things outside our control (other memory systems, Claude Code's features).
- `docs/superpowers/01/ideation/` — the scratchpad for version 01. Never fact.
- Read `CLAUDE.md` first for the rules.

This repo replaces the abandoned `pogan-mem` / `pogan-toolkit` build, which stays beside it as reference only.

`spine/` is the code map engine (`logan-spine-mcp`, a renamed vendored copy of DeusData/codebase-memory-mcp; see `spine/LOGAN-CHANGES.md` for every divergence from upstream). `plugin/` is the Claude Code plugin that installs and enforces it: `plugin/scripts/install.sh` builds, installs for Claude Code only, and copies the plugin into `~/.claude/skills/`. Design: `docs/superpowers/01/specs/2026-08-21-spine-v1-design.md`.
