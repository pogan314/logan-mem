---
title: Version 02 follow-ups, deferred at merge
type: wiki
status: research-fact
created: "2026-08-24 10:39 CDT"
updated: "2026-08-24 10:51 CDT"
sources:
  - "Whole-branch final review of dev/version-02-plugin-packaging, 2026-08-24, findings 2 and 3 and its triage table"
  - "Run ledger: .superpowers/sdd/2026-08-23-plugin-packaging-plan/progress.md"
---

Everything here was found by review, reproduced, and deliberately not fixed before merge. None of it blocks the plugin working. Each entry names what was measured, so the next session does not have to rediscover it.

## 1. `install.sh` gates the essential step behind the fragile one

`plugins/logan-spine/scripts/install.sh` sets `set -euo pipefail`, then runs `config set auto_index true` at step 4 and `claude plugin marketplace add` at step 5. If step 4 exits non-zero the script dies having placed the binary but never registered the marketplace, and a repository whose `.claude/settings.json` commits `enabledPlugins` is then inert with nothing explaining why.

This is not hypothetical. Step 4 was refused for about sixteen hours on 2026-08-23 because an unrelated long-running session held the engine's per-account daemon with a different build, and `install.sh` aborted there every time. It first completed at 09:36 CDT on 2026-08-24, once that daemon had ended.

**Raised in severity on 2026-08-24, once the cause of the stranding was established.** `install.sh` rebuilds the engine and replaces `~/.local/bin/logan-spine-mcp`. The engine refuses to start a process whose build differs from a resident daemon's — `spine/src/daemon/version_cohort.h:1`, "crash-safe exact-build admission" — so running `install.sh` while any other session still holds a live connection to the old build strands **every** new session's MCP server until the last of those clients disconnects. That is not a corner case: it is what happens whenever the owner installs an update without closing their other Claude sessions first, and on 2026-08-23 it lasted about sixteen hours and produced 63 logged conflicts.

The installer says nothing about this. At minimum it should check for a resident daemon before replacing the binary and tell the operator what will happen; better, it should offer to wait or to name the sessions holding it. Diagnosis, when it does happen, is `~/.cache/logan-spine-mcp/logs/daemon-conflicts.ndjson`.

Fix: swap steps 4 and 5, or make step 4 tolerate failure with a warning. What is verified: the `set -e`, the ordering, and that `lsm_cmd_config` at `spine/src/cli/cli.c:6836` can return non-zero. What is not: that a daemon-refused `config set` specifically returns non-zero, because the state could not be reproduced afterwards.

## 2. `rewrite()` destroys a symlinked config file, and never checks `mv`

Both live on the same line of `plugins/logan-spine/scripts/unregister-global.sh`, so fix them together.

If `~/.claude/settings.json` is a symlink into a dotfiles tree, `mv "$file.logan-spine-new" "$file"` replaces the **link** with a regular file. Reproduced during final review: after a `--yes` run against such a fixture, the path is a regular file and the real file in the dotfiles tree still contains the handler — while the script reports the file edited and names a backup. That is the one property this script exists to hold, so it matters even though it is latent here: `ls -l` shows both config files are currently regular files. `CLAUDE.md` records that these files are Mutagen-synced between two machines.

Separately, `mv`'s exit status is never checked, and the `EDITED` list that drives the operator-facing closing message is appended from that same unchecked line — so a failed `mv` would report a file as edited when it was not. A reviewer could not reach that as uid 1000: `chattr +i` needs root, and `mv` ignores the destination's own permission bits.

Fix: resolve `$file` through `readlink -f` before the `mv`, and check the `mv`.

## 3. No agent has been observed reaching the graph on the shared runtime

The spec's Verified table row 5 carries this. The server serves a real graph call on the shared runtime — row 4 records `tools/call` for `list_projects` returning ten projects from the real index — and the three agents receive their correct tiers there. The two facts have not been joined by a measurement of an *agent* making a graph call outside the isolated runtime, and `logan-spine:verify` has not been observed reaching the graph anywhere at all.

What would settle it: dispatch each of the three agents with a task that forces a graph tool call, and capture the tool-use records.

## 4. Machine state, not branch content: one historical `~/.claude/settings.json` anomaly, and a hypothesis that is now disproved

**The tally is one, not three, and it is closed for practical purposes.**

**The 2026-08-22 `lsm-*` disappearance is moot.** Those seven handlers are this repository's own hooks, installed by version 01's global footprint. Task 10 deliberately removes them, which is the entire point of version 02. Their vanishing was an anomaly when version 01 was the shipped state and they were meant to be present; it is not an open problem now, because the thing that went missing is a thing that should no longer exist. No snapshot of it survives, so it cannot be characterised, and there is nothing left to protect.

**The 2026-08-23 17:18 revert was real** — the file returned to its 10:55 state, losing five tmux status handlers written between 10:57 and 11:16 — and its cause is still unnamed. What follows narrows it considerably.

### The writer is Claude Code, and its write is safe

An `auditctl` watch armed on 2026-08-24 recorded 102 write events across `~/.claude/settings.json` and `~/.claude.json` in about an hour. Read back with `sudo ausearch -k claudesettings -i`: **every event is a `rename` syscall from `exe=/home/ubuntu/.local/share/claude/versions/2.1.241`**. Nothing else on the machine writes these files — no cron job, no systemd timer, no sync agent, no script. The pattern is write-to-temp-then-rename, the temp file stamped with the writing process's pid, from **33 distinct Claude Code processes** in that hour, interactive and headless, overlapping.

**A hypothesis this document previously carried is now disproved.** It said that pattern was "exactly the mechanism a whole-file revert needs" — many processes each replacing the file from a stale in-memory copy. Measurement contradicts it:

| Snapshot | tmux handlers | `lsm-` handlers | Time |
|---|---|---|---|
| `settings.json.bak-pre-tmux-restore` | 0 | 7 | 08:40:11 |
| `file-history …@v5` | 5 | 7 | 08:43:32 |
| `file-history …@v6` | 5 | 7 | 09:01:23 |
| `settings.json.logan-spine-backup` | 5 | 7 | 09:05:12 |
| live `settings.json` | 5 | 0 | 10:25 |

The owner restored the five tmux handlers externally at about 08:41. The cutover script removed the seven `lsm-` handlers externally at 09:05:12. Claude Code then wrote the file at 09:07:30 from pid 2478392, a `claude --resume` session that had been running since before **both** of those edits. Both survived. A long-running session's write did not discard changes made after that session started, so Claude Code re-reads the file before writing rather than replacing it from a stale copy.

That does not explain the 2026-08-23 event, and nothing here should be read as explaining it. It removes the leading candidate.

### What to do about it

Nothing, in this repository. The watch stays armed — `sudo ausearch -k claudesettings -i` — so a recurrence arrives with a pid and a timestamp instead of a guess.

**A rule for anyone working in this repository:** never restore, revert, or "repair" a hook in `~/.claude/settings.json` that this repository did not create. The tmux status handlers, and every other entry, belong to the owner. The only entries this repository has ever owned are the seven `lsm-*` ones, and Task 10 removed them on purpose.

The durable mitigation this version delivers stands regardless: hooks that live in a plugin's own `hooks/hooks.json` are not part of the user settings object at all, so a whole-file rewrite of `settings.json` cannot reach them.
