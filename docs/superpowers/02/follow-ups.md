---
title: Version 02 follow-ups, deferred at merge
type: wiki
status: research-fact
created: "2026-08-24 10:25 CDT"
updated: "2026-08-24 10:25 CDT"
sources:
  - "Whole-branch final review of dev/version-02-plugin-packaging, 2026-08-24, findings 2 and 3 and its triage table"
  - "Run ledger: .superpowers/sdd/2026-08-23-plugin-packaging-plan/progress.md"
---

Everything here was found by review, reproduced, and deliberately not fixed before merge. None of it blocks the plugin working. Each entry names what was measured, so the next session does not have to rediscover it.

## 1. `install.sh` gates the essential step behind the fragile one

`plugins/logan-spine/scripts/install.sh` sets `set -euo pipefail`, then runs `config set auto_index true` at step 4 and `claude plugin marketplace add` at step 5. If step 4 exits non-zero the script dies having placed the binary but never registered the marketplace, and a repository whose `.claude/settings.json` commits `enabledPlugins` is then inert with nothing explaining why.

This is not hypothetical. Step 4 was refused for about sixteen hours on 2026-08-23 because an unrelated long-running session held the engine's per-account daemon with a different build, and `install.sh` aborted there every time. It first completed at 09:36 CDT on 2026-08-24, once that daemon had ended.

Fix: swap steps 4 and 5, or make step 4 tolerate failure with a warning. What is verified: the `set -e`, the ordering, and that `lsm_cmd_config` at `spine/src/cli/cli.c:6836` can return non-zero. What is not: that a daemon-refused `config set` specifically returns non-zero, because the state could not be reproduced afterwards.

## 2. `rewrite()` destroys a symlinked config file, and never checks `mv`

Both live on the same line of `plugins/logan-spine/scripts/unregister-global.sh`, so fix them together.

If `~/.claude/settings.json` is a symlink into a dotfiles tree, `mv "$file.logan-spine-new" "$file"` replaces the **link** with a regular file. Reproduced during final review: after a `--yes` run against such a fixture, the path is a regular file and the real file in the dotfiles tree still contains the handler — while the script reports the file edited and names a backup. That is the one property this script exists to hold, so it matters even though it is latent here: `ls -l` shows both config files are currently regular files. `CLAUDE.md` records that these files are Mutagen-synced between two machines.

Separately, `mv`'s exit status is never checked, and the `EDITED` list that drives the operator-facing closing message is appended from that same unchecked line — so a failed `mv` would report a file as edited when it was not. A reviewer could not reach that as uid 1000: `chattr +i` needs root, and `mv` ignores the destination's own permission bits.

Fix: resolve `$file` through `readlink -f` before the `mv`, and check the `mv`.

## 3. No agent has been observed reaching the graph on the shared runtime

The spec's Verified table row 5 carries this. The server serves a real graph call on the shared runtime — row 4 records `tools/call` for `list_projects` returning ten projects from the real index — and the three agents receive their correct tiers there. The two facts have not been joined by a measurement of an *agent* making a graph call outside the isolated runtime, and `logan-spine:verify` has not been observed reaching the graph anywhere at all.

What would settle it: dispatch each of the three agents with a task that forces a graph tool call, and capture the tool-use records.

## 4. Machine state, not branch content: two unexplained losses from `~/.claude/settings.json`

Raised with the owner rather than fixed, because it is not this repository's code.

- 2026-08-22: all seven `lsm-*` hook entries went missing. Recorded in `CLAUDE.md`. A controlled replay did not reproduce it.
- 2026-08-23: the whole file reverted to a snapshot from six hours earlier, losing five tmux status handlers written in a 21-minute window. Established: the current file was byte-identical to the earlier snapshot except for four model and effort lines; the file was replaced with a new inode; no Claude session's Edit or Write tool did it, across 414 transcripts; no cron job, systemd timer, sync agent or script on the machine does it. Four open upstream Claude Code issues describe the same defect class. **The cause is not established.** An `auditctl` watch is armed on the file.
- ~~Observed 2026-08-24: `enabledPlugins` lists six plugins where a harvest on 2026-08-23 recorded eight.~~ **Not a loss. Disproved twice, independently, and withdrawn.** The count has only ever grown, and never reached eight:

| Snapshot | Date | `enabledPlugins` |
|---|---|---|
| `settings.json.bak-pre-growthbook-fix` | 2026-07-22 16:57 | 3 |
| `settings.json.bak-20260802-205622` | 2026-08-02 15:56 | 5 |
| `file-history …@v1` | 2026-08-23 10:55 | 6 |
| `file-history …@v4` | 2026-08-23 11:16 | 6 |
| `settings.json.bak-pre-tmux-restore` | 2026-08-24 08:40 | 6 |
| `settings.json.logan-spine-backup` | 2026-08-24 09:05 | 6 |
| live `settings.json` | 2026-08-24 | 6 |

The same six names appear in every snapshot from 2026-08-23 onward: `claude-code-setup`, `claude-md-management`, `frontend-design`, `typescript-lsp` and `superpowers` from `claude-plugins-official`, and `global-plugin` from `global-plugins`. `enabledPlugins` is also absent from `settings.local.json` and `~/.claude.json`, so there is no second source the figure could have come from. The number eight appears exactly once, in a subagent's prose at `task-2-report.md:62`, and no artefact supports it. Two plugins were never enabled and therefore never lost.

So **the real tally is two events, not three.**

### The writer is now identified, by audit trail

An `auditctl` watch was armed on both files on 2026-08-24 (`-w /home/ubuntu/.claude/settings.json -p wa -k claudesettings`, and the same for `~/.claude.json`). Within about one hour it recorded **102 write events**. Read back with `sudo ausearch -k claudesettings -i`:

- **Every single event is `syscall=rename`**, and every one comes from `exe=/home/ubuntu/.local/share/claude/versions/2.1.241` — Claude Code itself. Nothing else on the machine writes these files. No cron job, no systemd timer, no sync agent, no shell script.
- The write pattern is write-then-rename: a temporary file stamped with the writing process's own pid, `settings.json.tmp.<pid>.<random>`, renamed over `settings.json`. That is a **whole-file replace**, not an in-place patch of the key being changed.
- **33 distinct Claude Code processes** performed those writes in that hour, both interactive sessions (`tty=pts4`, `tty=pts6`) and headless `claude -p` children (`tty=(none)`), overlapping in time.

What that establishes: the only writer is Claude Code, and it rewrites the entire file from whatever it holds in memory. What it does **not** establish: that a stale in-memory copy caused the specific 2026-08-23 revert. The watch was armed after that event, so it did not capture it. But the mechanism the audit trail shows — many concurrent processes each replacing the whole file — is exactly the shape that produces one: a process that read the file at time T and writes at time T+n discards every change made in between, and no locking is visible in the trace.

**One counter-observation worth keeping.** On 2026-08-24 the cutover script wrote `settings.json` at 09:05:12 via its own `mv`, and a Claude Code process wrote the same file at 09:07:30, two minutes eighteen seconds later. The cutover's change survived that write — the seven `lsm-` handlers stayed removed and all five tmux handlers stayed present. So in the one instance anybody has observed directly, Claude Code's writer did **not** clobber a concurrent external edit. That is a single data point, not a property.

**The current file reconciles completely.** Diffed against the cutover's own backup, the only differences are the seven `lsm-` handlers the cutover removed and one `effortLevel` value the owner changed. Top-level keys, permissions, `enabledPlugins` and all five tmux handlers are byte-identical.

The watch stays armed, so the next occurrence will name the process and the time. `sudo ausearch -k claudesettings -i` is the command.

The durable mitigation is the one this version delivers: hooks that live in a plugin's own `hooks/hooks.json` are not part of the user settings object, so a whole-file rewrite of `settings.json` cannot reach them.
