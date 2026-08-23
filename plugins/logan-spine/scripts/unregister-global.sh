#!/usr/bin/env bash
# Remove the pre-plugin logan-spine footprint from Claude Code's user-level configuration, and nothing else.
#
# This exists because the engine's own `uninstall` takes no --clients flag (spine/src/cli/cli.c), so running it would also strip logan-spine from every other agent client configured on this machine.
#
# Usage:
#   unregister-global.sh                 # dry run: print what would change
#   unregister-global.sh --yes           # act
#   unregister-global.sh --home DIR ...  # target a fixture instead of $HOME
set -uo pipefail
YES=0
H="${HOME:-}"
while [ $# -gt 0 ]; do
  case "$1" in
    --yes) YES=1 ;;
    --home) shift; H="${1:-}" ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done
[ -n "$H" ] || { echo "HOME is not set and --home was not given; refusing" >&2; exit 1; }
command -v jq >/dev/null 2>&1 || { echo "jq is required" >&2; exit 1; }

SETTINGS="$H/.claude/settings.json"
CLAUDEJSON="$H/.claude.json"
STAMP="$(date +%Y%m%d-%H%M%S)"
# Anchored to the installed path, not a bare substring. Unanchored, `/opt/vendor/lsm-session-reminder-v2 --other`, `not-lsm-subagent-reminder-clone.mjs` and `echo lsm-code-discovery-gateway` all matched and were deleted.
LSM_PATTERN='/[.]claude/hooks/(lsm-code-discovery-gate|lsm-session-reminder|lsm-subagent-reminder)"?$'
# Three-level pruning: remove individual HANDLER objects, then drop a matcher group only once its hooks array is empty, then drop an event only once its group array is empty. Operating on matcher groups instead would destroy unrelated handlers that share a group with one of ours.
PRUNE='
  .hooks |= (
    with_entries(
      .value |= (
        map(.hooks |= ((. // []) | map(select((.command // "") | test($pat) | not))))
        | map(select((.hooks | length) > 0))
      )
    )
    | with_entries(select((.value | length) > 0))
  )
'
say() { printf '%s\n' "$1"; }
act() { [ "$YES" -eq 1 ]; }

# Rewrite FILE through a jq filter, via a temporary, leaving FILE untouched if jq fails. A direct redirect would truncate FILE before jq ran. Any arguments after the filter are passed to jq.
rewrite() {
  local file="$1" filter="$2"; shift 2
  local backup="$file.logan-spine-backup-$STAMP"
  cp "$file" "$backup" || return 1
  if jq "$@" "$filter" "$backup" > "$file.logan-spine-new"; then
    mv "$file.logan-spine-new" "$file"
    say "  backup: $backup"
    say "  note: jq reformats the whole file; the backup is the byte-exact original"
    return 0
  fi
  rm -f "$file.logan-spine-new"
  say "  jq failed; $file left unchanged. Backup: $backup"
  return 1
}

rc=0

# --- settings.json ----------------------------------------------------------
if [ -f "$SETTINGS" ]; then
  n="$(jq --arg pat "$LSM_PATTERN" '[.hooks[]?[]?.hooks[]? | select((.command // "") | test($pat))] | length' "$SETTINGS" 2>/dev/null)"; jqrc=$?
  if [ "$jqrc" -ne 0 ]; then
    # A file jq cannot read is not a file with nothing in it. Saying "no lsm- handlers" here would report an unreadable configuration as clean and let the removals run against it.
    say "settings.json: cannot parse $SETTINGS; refusing to edit it"
    rc=1
  elif [ "${n:-0}" -gt 0 ]; then
    say "settings.json: remove $n lsm- hook handler(s)"
    jq -r --arg pat "$LSM_PATTERN" '.hooks | to_entries[] | .key as $e | .value[] | .hooks[]? | select((.command // "") | test($pat)) | "  \($e): \(.command)"' "$SETTINGS" 2>/dev/null
    say "  note: the rewrite also drops any matcher group left with no handlers and any event left with no groups, including any that were already empty before this run"
    if act; then
      rewrite "$SETTINGS" "$PRUNE" --arg pat "$LSM_PATTERN" || rc=1
    else
      # Exercise the filter now, against a throwaway copy outside the target tree, so a dry run cannot promise an edit that would abort.
      preview="$(mktemp)"
      if jq --arg pat "$LSM_PATTERN" "$PRUNE" "$SETTINGS" > "$preview" 2>/dev/null; then
        say "  the filter was exercised against a temporary copy and would succeed"
      else
        say "  the filter aborts on this file: the rewrite would FAIL and settings.json would be left unchanged"
        rc=1
      fi
      rm -f "$preview"
    fi
  else
    say "settings.json: no lsm- handlers"
  fi
fi

# --- .claude.json: exactly one key ------------------------------------------
# This file also holds per-project state for every project on the machine, so nothing else in it is touched.
if [ -f "$CLAUDEJSON" ]; then
  has="$(jq 'has("mcpServers") and (.mcpServers | has("logan-spine-mcp"))' "$CLAUDEJSON" 2>/dev/null)"; jqrc=$?
  if [ "$jqrc" -ne 0 ]; then
    say ".claude.json: cannot parse $CLAUDEJSON; refusing to edit it"
    rc=1
  elif [ "$has" = "true" ]; then
    say ".claude.json: remove mcpServers.logan-spine-mcp"
    if act; then
      rewrite "$CLAUDEJSON" 'del(.mcpServers["logan-spine-mcp"])' || rc=1
    fi
  else
    say ".claude.json: no logan-spine-mcp entry"
  fi
else
  say ".claude.json: not present"
fi

# A failed configuration edit means the machine is not in the state the removals assume. Stop before touching disk: the configuration still names these files, and deleting them would leave Claude Code invoking commands that no longer exist.
if [ "$rc" -ne 0 ]; then
  say ""
  say "A configuration edit failed. Nothing on disk was removed. Restore from the backup named above and re-run."
  exit "$rc"
fi

# --- files ------------------------------------------------------------------
for p in \
  "$H/.claude/hooks/lsm-code-discovery-gate" \
  "$H/.claude/hooks/lsm-session-reminder" \
  "$H/.claude/hooks/lsm-subagent-reminder" \
  "$H/.claude/agents/logan-spine.md" \
  "$H/.claude/agents/logan-spine-scout.md" \
  "$H/.claude/agents/logan-spine-auditor.md" \
  "$H/.claude/skills/logan-spine" \
  "$H/.claude/skills/logan-spine-tools" ; do
  if [ -e "$p" ]; then
    say "remove: $p"
    if act; then rm -rf "$p" || rc=1; fi
  fi
done

# Deliberately left alone: the engine binary, ~/.bashrc, the index cache at ~/.cache/logan-spine-mcp/, and every other agent client's configuration. Removing the cache would force a full re-index of every project for nothing.

if act; then
  say ""
  if [ "$rc" -eq 0 ]; then say "Done."; else say "FAILED — see the messages above."; fi
else
  say ""
  say "Dry run. Nothing was changed. Re-run with --yes to act."
fi
say ""
say "To restore the old global footprint at any time:"
say "  ~/.local/bin/logan-spine-mcp install --clients=claude -y"
exit "$rc"
