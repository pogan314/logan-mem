---
title: Claude Code harness facts
type: wiki
status: research-fact
created: 2026-08-21
updated: 2026-08-21
sources: [live docs via mcp__claude-code-docs__query_docs_filesystem_claude_code_docs, pages cited inline as /en/*.mdx]
---

# What Claude Code itself provides — facts checked against live docs, 2026-08-21

## 1. Hook events

All fire at a point in the session lifecycle. "Can inject" = the event supports `hookSpecificOutput.additionalContext`, documented in the "Add context for Claude" section (/en/hooks.mdx). "Can block" = the event has some decision-control field that stops or denies the action (/en/hooks.mdx, "Decision control" table).

| Event | Fires when | Can inject context? | Can block? |
|---|---|---|---|
| SessionStart | Session begins or resumes | Yes | No |
| Setup | `--init-only`, or `--init`/`--maintenance` in `-p` mode | Yes | No |
| UserPromptSubmit | You submit a prompt, before Claude processes it | Yes | Yes (`decision: "block"`) |
| UserPromptExpansion | A typed command expands into a prompt, before it reaches Claude | Yes | Yes |
| PreToolUse | Before a tool call executes | Yes (next to tool result) | Yes (`permissionDecision`: allow/deny/ask/defer) |
| PermissionRequest | A tool call needs a permission decision | No | Yes (`decision.behavior`: allow/deny) |
| PermissionDenied | Auto mode denies a tool call | No | No (denial already happened; only sets `retry`) |
| PostToolUse | After a tool call succeeds | Yes | Yes (`decision: "block"`, or `updatedToolOutput`) |
| PostToolUseFailure | After a tool call fails | Yes | Yes |
| PostToolBatch | After a full batch of parallel tool calls resolves | Yes | Yes |
| Notification | Claude Code sends a notification | No | No |
| MessageDisplay | While assistant message text is displayed | No | No (can only change on-screen `displayContent`, not what Claude sees) |
| SubagentStart | A subagent is spawned | Yes | No |
| SubagentStop | A subagent finishes | Yes | Yes (same pattern as Stop) |
| TaskCreated | A task is being created via `TaskCreate` | No | Yes (exit 2 or `decision: "block"` cancels it) |
| TaskCompleted | A task is being marked completed | No | Yes (`continue: false` when it fires from a teammate stopping) |
| Stop | Claude finishes responding | Yes | Yes (`decision: "block"` prevents stopping) |
| StopFailure | The turn ends due to an API error | No | No |
| TeammateIdle | An agent-team teammate is about to go idle | No | Yes (`continue: false` stops the teammate) |
| InstructionsLoaded | A CLAUDE.md or `.claude/rules/*.md` file loads into context | No | No |
| ConfigChange | A configuration file changes during a session | No | Yes |
| CwdChanged | The working directory changes (e.g. Claude runs `cd`) | No | No |
| DirectoryAdded | A directory is added mid-session via `/add-dir` | No | No |
| FileChanged | A watched file changes on disk (via `watchPaths`) | No | No |
| WorktreeCreate | A worktree is being created | No | Yes (hook must return a path or creation fails) |
| WorktreeRemove | A worktree is being removed | No | No |
| PreCompact | Before context compaction | No | Yes |
| PostCompact | After context compaction completes | No | No |
| Elicitation | An MCP server requests user input during a tool call | No | Yes (`action`: accept/decline/cancel) |
| ElicitationResult | After a user responds to an elicitation, before it's sent back | No | Yes (can override `action`/`content`) |
| SessionEnd | A session terminates | No | No (cleanup only; JSON output discarded) |

## 2. Hook types

Five handler types, set via `type` on a hook entry (/en/hooks.mdx, "Hook handler fields"):
- `command` — runs a shell command; input on stdin, decision via exit code/stdout. Default timeout 600s (30s for UserPromptSubmit, 10s for MessageDisplay).
- `http` — POSTs the JSON input to a URL; same JSON output format as command hooks.
- `mcp_tool` — calls a tool on an already-connected MCP server; the server must already be connected (never triggers OAuth/connect).
- `prompt` — sends the input to an LLM (Haiku by default) for a single-turn `{ok, reason}` decision. Default timeout 30s.
- `agent` — spawns a subagent with tool access (Read, Grep, Glob, etc.) to verify conditions before returning `{ok, reason}`. Default timeout 60s.
- `$ARGUMENTS` placeholder: in `prompt`/`agent` hooks, injects the hook's JSON input into the prompt text; if omitted, the input JSON is appended to the prompt instead. Escape with `\$ARGUMENTS` for literal text.
- Agent hooks run for **up to 50 turns** before returning their decision (/en/hooks.mdx, "How agent hooks work").
- Verbatim experimental warning on agent hooks (/en/hooks.mdx): "Agent hooks are experimental. Behavior and configuration may change in future releases. For production workflows, prefer command hooks."
- Not every event supports every type: `SessionStart` and `Setup` support only `command` and `mcp_tool` (no `http`, `prompt`, or `agent`). Most tool/turn-lifecycle events support all five types.

## 3. SessionStart, SessionEnd, Stop

**SessionStart** (/en/hooks.mdx):
- Matchers: `startup` (new session), `resume` (`--resume`/`--continue`/`/resume`), `clear` (`/clear`), `compact` (auto or manual compaction), `fork` (new session forked via `--fork-session`, `/fork`, or `/branch`).
- Delivers `additionalContext` at the start of the conversation, before the first prompt.
- Size guidance: none specific to SessionStart, but the general rule (any event) is that any single `additionalContext` value over 10,000 characters gets written to a file and Claude gets a path + short preview instead of the full text.
- Also supports `initialUserMessage` (creates the first turn, `-p` mode only), `sessionTitle`, `watchPaths`, and `reloadSkills` (re-scans skill/command dirs after hooks finish).

**SessionEnd** (/en/hooks.mdx):
- Fires when a session terminates; `reason` field: `clear`, `resume`, `logout`, `prompt_input_exit`, `other`.
- Has **no decision control** — cannot block session termination, cleanup/logging only. Claude Code discards its JSON output fields (e.g. `systemMessage`).
- Default timeout budget is 1.5 seconds total across all SessionEnd hooks (auto-raised to the highest per-hook `timeout` configured, capped at 60s; override with `CLAUDE_CODE_SESSIONEND_HOOKS_TIMEOUT_MS`).

**Stop** (/en/hooks.mdx):
- Fires when the main agent finishes responding (not on user interrupt; API errors fire `StopFailure` instead).
- Can block and re-prompt: `decision: "block"` with a required `reason` prevents Claude from stopping — Claude receives the reason as its next instruction and continues.
- `stop_hook_active` (input field) is `true` when Claude Code is already continuing because of a stop hook, so a hook can check it to avoid an infinite loop. Claude Code hard-caps this at **8 consecutive blocks**, after which it overrides the hook and ends the turn regardless.
- Alternative to blocking: `hookSpecificOutput.additionalContext` gives non-error feedback that also continues the conversation, but is shown in the transcript as "Stop hook feedback" rather than a hook error.
- Also receives `last_assistant_message`, `background_tasks` (in-flight shell/subagent/monitor/etc. tasks), and `session_crons` (scheduled wakeups) so a Stop hook can tell "session is done" from "session is paused waiting on background work."

## 4. Plugin anatomy

A plugin is a self-contained directory; the manifest is optional (/en/plugins-reference.mdx):
- `.claude-plugin/plugin.json` — the manifest: `name`, `description`, `version`, `author`, etc. If omitted, Claude Code auto-discovers components by default directory names and derives the plugin name from the folder name.
- `hooks/hooks.json` (or inline `hooks` in plugin.json) — same event/matcher/handler schema as settings-file hooks, all five hook types supported.
- `.mcp.json` (or inline `mcpServers` in plugin.json) — bundled MCP servers; they connect automatically when the plugin is enabled, using standard MCP server config with `${CLAUDE_PLUGIN_ROOT}` path substitution.
- `skills/<name>/SKILL.md` (or `commands/` for flat markdown, or a bare root `SKILL.md` for a single-skill plugin) — namespaced as `/plugin-name:skill-name`.
- `agents/` — custom subagent definitions, appear in @-mention typeahead as `plugin-name:agent-name`. Plugin-shipped agents do NOT support `hooks`, `mcpServers`, or `permissionMode` frontmatter fields (blocked for security).
- Also possible: `.lsp.json` (LSP servers), `monitors/monitors.json` (background monitors), `bin/` (executables added to Bash's PATH), `settings.json` (default settings, currently only `agent` and `subagentStatusLine` keys).
- **Marketplaces**: a `.claude-plugin/marketplace.json` at a repo root lists plugins and their sources (git repo, local path, URL). Users register a marketplace with `claude plugin marketplace add <owner/repo | url | path>` then install with `claude plugin install <plugin>[@marketplace]`.
- **Install scopes** (/en/plugins-reference.mdx): `user` (`~/.claude/settings.json`, default, all projects), `project` (`.claude/settings.json`, shared via version control), `local` (`.claude/settings.local.json`, gitignored), `managed` (read-only, org-controlled).
- Once a plugin is **enabled**, its hooks merge automatically with user/project hooks, and its MCP servers connect automatically at session startup — no separate activation step needed beyond install/enable.
- A separate mechanism exists outside marketplaces: any skills-directory folder (`~/.claude/skills/<name>/` or `<project>/.claude/skills/<name>/`) containing its own `.claude-plugin/plugin.json` auto-loads as a plugin named `<name>@skills-dir` on the next session, no install step.

## 5. MCP servers

Three installation scopes (/en/mcp.mdx, "MCP installation scopes"):

| Scope | Loads in | Shared with team | Stored in |
|---|---|---|---|
| Local (default) | Current project only | No | `~/.claude.json` (under that project's path) |
| Project | Current project only | Yes, via version control | `.mcp.json` in project root |
| User | All your projects | No | `~/.claude.json` |

- A project-scoped `.mcp.json` server prompts for approval in interactive sessions before first use (reset with `claude mcp reset-project-choices`); `claude -p`, Agent SDK, and cloud sessions load it without asking.
- MCP servers can ship inside a plugin (`.mcp.json` at plugin root or inline `mcpServers` in `plugin.json`). Plugin-provided servers behave like manually configured ones but are added/removed by installing/uninstalling the plugin, not via `/mcp`. Their tool names take the form `mcp__plugin_<plugin-name>_<server-name>__<tool-name>`, and the server itself registers as `plugin:<plugin-name>:<server-name>`.

## 6. Native memory features (no plugin needed)

From /en/memory.mdx:
- **CLAUDE.md hierarchy**, broadest to narrowest, all loaded every session: managed policy CLAUDE.md (org-wide, e.g. `/etc/claude-code/CLAUDE.md`) → `~/.claude/CLAUDE.md` (user) → `./CLAUDE.md` or `./.claude/CLAUDE.md` (project, shared via git) → `./CLAUDE.local.md` (personal, gitignored).
- CLAUDE.md files load in full up to 4 MiB (skipped if larger); recommended target is under 200 lines per file for adherence.
- Supports `@path/to/file` imports (max depth 4 hops), `.claude/rules/*.md` with optional `paths:` frontmatter glob-scoping so a rule only loads when Claude touches matching files.
- **Auto memory**: on by default, toggled via `/memory` or `autoMemoryEnabled` setting. Stored at `~/.claude/projects/<project>/memory/` (one directory per git repo, shared across worktrees; machine-local, not synced). Contains `MEMORY.md` (index) plus topic files Claude creates.
- `MEMORY.md` loads at session start up to **the first 200 lines or 25KB, whichever comes first** — content past that point is not loaded. Topic files load on demand only, not at startup.
- Auto memory is written by Claude, not you; CLAUDE.md is written by you. Both are "context, not enforced configuration" per the docs — to hard-enforce something, docs say to use a PreToolUse hook instead.
- **`#` shortcut**: confirmed REMOVED (/en/changelog.mdx:4585 — "Removed # shortcut for quick memory entry (tell Claude to edit your CLAUDE.md instead)"). It does not exist in the current docs.
- **`/memory`**: lists CLAUDE.md/CLAUDE.local.md/auto-memory file locations across user and project scope, lets you toggle auto memory, and opens any file in your editor (creating it first if it doesn't exist yet).
- Subagents can have their own separate persistent memory via a `memory` frontmatter field — distinct from, and not shared with, the main session's auto memory (/en/memory.mdx; /en/sub-agents.mdx).

## 7. Transcripts

- Full session transcripts live at `~/.claude/projects/<project>/<session>.jsonl` — documented as "Full conversation transcript: every message, tool call, and tool result" (/en/claude-directory.mdx, "Application data" table).
- Subagent transcripts live separately at `~/.claude/projects/<project>/<session>/subagents/agent-{agentId}.jsonl` (/en/sub-agents.mdx, "Resume subagents").
- Both are deleted after `cleanupPeriodDays` (default 30 days) unless excluded (auto-memory files are explicitly excluded from this sweep; transcripts are not).
- The docs do not specify the internal JSONL record schema (line-by-line field structure) anywhere I found — only that it holds messages, tool calls, and tool results, plus one documented internal record shape: a `{"type": "system", "subtype": "compact_boundary", "compactMetadata": {...}}` entry logged at compaction (/en/sub-agents.mdx, "Auto-compaction").
- Not encrypted at rest — OS file permissions are the only protection (/en/claude-directory.mdx).

## 8. Subagents

From /en/sub-agents.mdx and /en/hooks.mdx:
- **Hooks from settings files, managed policy, and plugins run inside subagents.** When a subagent calls a tool, the same configured `PreToolUse`/`PostToolUse`/etc. hooks fire, with the input carrying `agent_id` and `agent_type` fields identifying the subagent (/en/hooks.mdx, "Hook locations").
- `SubagentStart` and `SubagentStop` **do exist** as project-level settings.json events (matcher = agent type name), firing when any subagent begins/completes.
- A subagent can also define its own hooks in its markdown frontmatter; these run only while that subagent is active and are removed when it finishes. A `Stop` hook defined in subagent frontmatter is automatically converted to a `SubagentStop` event at runtime.
- **Fork subagents**: `fork` is a special subagent type that inherits the *entire* parent conversation (system prompt, tools, model, full message history) instead of starting fresh — the only subagent type that does. Its own tool calls stay out of the main context; only its final result returns. A fork cannot spawn further forks. Started via `/subtask` or Claude requesting the `fork` type through the Agent tool (fork mode is on by default in interactive sessions).
- A normal (non-fork) subagent starts with a fresh, isolated context: its own system prompt + task message + the full CLAUDE.md hierarchy + a git-status snapshot + any preloaded skills — but explicitly **not** the main conversation's auto memory, and not the parent's output style. (Built-in `Explore` and `Plan` subagents additionally skip CLAUDE.md and git status.)

## 9. Things I looked for and could not find in the docs

- No documented JSONL line-level schema (field names/types per record) for session or subagent transcripts — only that they contain messages/tool-calls/tool-results, plus the one `compact_boundary` example shown.
- No explicit statement of a hard per-file size limit for CLAUDE.md beyond "4 MiB, skipped if larger" and "target under 200 lines for adherence" — no enforced hard line cap the way `MEMORY.md` has (200 lines / 25KB).
- No mention of an event named `SubagentPreToolUse` or similar subagent-scoped tool events beyond the standard `PreToolUse`/`PostToolUse` (subagents just fire the same events as the main session, disambiguated by `agent_id`).
- Did not find a documented maximum size or count limit for auto-memory topic files (only `MEMORY.md`'s 200-line/25KB index limit is documented).
