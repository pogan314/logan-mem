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
LSM_PATTERN='lsm-code-discovery-gate|lsm-session-reminder|lsm-subagent-reminder'
say() { printf '%s\n' "$1"; }
act() { [ "$YES" -eq 1 ]; }

# Rewrite FILE through a jq filter, via a temporary, leaving FILE untouched if jq fails. A direct redirect would truncate FILE before jq ran.
rewrite() {
  local file="$1" filter="$2" backup="$1.logan-spine-backup-$STAMP"
  cp "$file" "$backup" || return 1
  if jq "$filter" "$backup" > "$file.logan-spine-new"; then
    mv "$file.logan-spine-new" "$file"
    say "  backup: $backup"
    return 0
  fi
  rm -f "$file.logan-spine-new"
  say "  jq failed; $file left unchanged. Backup: $backup"
  return 1
}

rc=0

# --- settings.json: three-level pruning -------------------------------------
# Remove individual HANDLER objects, then drop a matcher group only once its hooks array is empty, then drop an event only once its group array is empty. Operating on matcher groups instead would destroy unrelated handlers that share a group; on the author's machine the SubagentStart "*" group also holds a tmux subagent counter.
if [ -f "$SETTINGS" ]; then
  n="$(jq "[.hooks[]?[]?.hooks[]? | select((.command // \"\") | test(\"$LSM_PATTERN\"))] | length" "$SETTINGS" 2>/dev/null || echo 0)"
  if [ "${n:-0}" -gt 0 ]; then
    say "settings.json: remove $n lsm- hook handler(s)"
    jq -r ".hooks | to_entries[] | .key as \$e | .value[] | .hooks[]? | select((.command // \"\") | test(\"$LSM_PATTERN\")) | \"  \(\$e): \(.command)\"" "$SETTINGS" 2>/dev/null
    if act; then
      rewrite "$SETTINGS" "
        .hooks |= (
          with_entries(
            .value |= (
              map(.hooks |= ((. // []) | map(select((.command // \"\") | test(\"$LSM_PATTERN\") | not))))
              | map(select((.hooks | length) > 0))
            )
          )
          | with_entries(select((.value | length) > 0))
        )
      " || rc=1
      say "  note: jq reformats the whole file; the backup is the byte-exact original"
    fi
  else
    say "settings.json: no lsm- handlers"
  fi
fi

# --- .claude.json: exactly one key ------------------------------------------
# This file also holds per-project state for every project on the machine, so nothing else in it is touched.
if [ -f "$CLAUDEJSON" ] && [ "$(jq 'has("mcpServers") and (.mcpServers | has("logan-spine-mcp"))' "$CLAUDEJSON" 2>/dev/null)" = "true" ]; then
  say ".claude.json: remove mcpServers.logan-spine-mcp"
  if act; then
    rewrite "$CLAUDEJSON" 'del(.mcpServers["logan-spine-mcp"])' || rc=1
  fi
else
  say ".claude.json: no logan-spine-mcp entry"
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
    act && rm -rf "$p"
  fi
done

# Deliberately left alone: the engine binary, ~/.bashrc, the index cache at ~/.cache/logan-spine-mcp/, and every other agent client's configuration. Removing the cache would force a full re-index of every project for nothing.

if act; then say ""; say "Done."; else say ""; say "Dry run. Nothing was changed. Re-run with --yes to act."; fi
say ""
say "To restore the old global footprint at any time:"
say "  ~/.local/bin/logan-spine-mcp install --clients=claude -y"
exit "$rc"
