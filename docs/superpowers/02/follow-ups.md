---
title: Version 02 follow-ups, deferred at merge
type: wiki
status: research-fact
created: "2026-08-24 09:55 CDT"
updated: "2026-08-24 09:55 CDT"
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
- ~~Observed 2026-08-24: `enabledPlugins` lists six plugins where a harvest on 2026-08-23 recorded eight.~~ **Not a loss — investigated 2026-08-24 and withdrawn.** Six independent snapshots of the file spanning 2026-08-23 10:55 to 2026-08-24 09:01 (four in `~/.claude/file-history/`, plus the `pre-tmux-restore` and `logan-spine-backup` backups) all hold exactly **six** entries, and the same six names in every one: `claude-code-setup`, `claude-md-management`, `frontend-design`, `typescript-lsp` and `superpowers` from `claude-plugins-official`, and `global-plugin` from `global-plugins`. The figure of eight appears once, in a subagent's prose at `task-2-report.md:62`, and is not supported by any snapshot. It was a miscount, not a disappearance.

The durable mitigation is the one this version delivers: hooks that live in a plugin's own `hooks/hooks.json` are not part of the user settings object, so a whole-file rewrite of `settings.json` cannot reach them.
