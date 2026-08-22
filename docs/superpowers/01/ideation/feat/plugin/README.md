---
title: feat/plugin — one plugin, one install, one uninstall
type: ideation
status: stale
created: "2026-08-21 13:14 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [owner-requirements.md #14, what-went-wrong.md, docs/wiki/claude-code-harness-facts.md, docs/wiki/obra-episodic-memory.md]
---

# feat/plugin

- The owner: "the other major issue was not making it a plugin, which also forces me to think this is shit."
- Owner-stated, so the one real requirement here: **everything the system needs on a machine arrives through one `/plugin install` and leaves through one `/plugin uninstall`.** Nothing else is written anywhere.

## What the old build scattered (so we never do it again)

| Surface | Where it went | Problem |
|---|---|---|
| SessionStart + SessionEnd hooks | `/home/ubuntu/.claude/settings.json` (written directly by `pogan init`) | Survives plugin removal; had to be hand-deleted twice |
| 7 deny rules | same file | Same |
| `mem` MCP server | `/home/ubuntu/.claude.json`, user scope | `uninstall` could not remove it (a hardcoded path bug); hand-deleted |
| Skills, agents, hooks.json | via a symlink at `/home/ubuntu/.claude/skills/pogan` → the repo | Mutagen refused the absolute symlink; the symlink was how everything loaded |
| Git hooks | `/home/ubuntu/.pogan/hooks/` + `core.hooksPath` on 3 store repos | Re-enable needs a second, undocumented command |
| A running MCP server process | memory | Deleting the config entry does not kill it; survived a "verified clean" teardown |

## What a single plugin gives us (verified in the wiki, 2026-08-21)

- `.claude-plugin/plugin.json` names the plugin. `hooks/hooks.json` declares hooks. `.mcp.json` declares the MCP server. `skills/`, `agents/`, `commands/` folders load automatically.
- `obra/episodic-memory` is an existence proof: one plugin, one MCP server, one agent, one skill, one SessionStart hook — and it is installed with one command from a marketplace.
- A marketplace can be a git repo (`marketplace.json`), so `pogan314/logan-mem` (the GitHub location the owner named) can be its own marketplace. No symlinks.

## Proposed rules for version 01 packaging (ideas, except the first line, which the owner stated)

- The plugin folder **is** the deliverable. If a feature cannot be delivered inside it, the feature waits.
- No `init` command that writes global config. If the plugin needs a per-user data folder, it creates it lazily on first use and says so.
- `uninstall` is Claude Code's own `/plugin uninstall`. We do not write one. If something would survive it, we do not ship that something.
- A `doctor`-style check, if any, is one command that prints what exists and exits. Not 18 checks, not 2,856 lines.
- Deny rules cannot ship inside a plugin (verified 2026-08-21, see Open below), so if version 01 wants any they are a documented optional manual step, never written by us.
- When the owner authorises the first install, the first test is the uninstall: install, confirm the tools exist, uninstall, confirm `ps`, `settings.json`, and `.claude.json` show nothing. Until that test passes nothing counts as done. (No install happens before the owner says so — see `CLAUDE.md`.)

## Open

- ~~Does Claude Code let a plugin ship permission deny rules?~~ **Answered 2026-08-21 from the live docs** (`/en/plugins-reference`, the plugin `settings.json` row): a plugin's own `settings.json` supports only the `agent` and `subagentStatusLine` keys. So no — deny rules cannot ship inside a plugin. If version 01 wants any, they are a documented optional manual step, never written by us.
- One plugin for memory + episodic, or two? If episodic is adopted as-is (`../episodic/`), it is two plugins from two marketplaces, which is fine; "one plugin" is about *ours*.
