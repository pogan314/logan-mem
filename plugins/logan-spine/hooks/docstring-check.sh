#!/usr/bin/env bash
# PostToolUse on Edit|Write: report symbols in the file just written that have no docstring.
#
# Exit 2 shows stderr to Claude without failing the tool that already ran. Anything else exits 0 silently, so a missing binary or a missing jq is a no-op rather than an error the operator has to see.
set -u
. "$(dirname "$0")/lib.sh"
bin="$(lsm_bin)" || exit 0
command -v jq >/dev/null 2>&1 || exit 0
file="$(jq -r '.tool_input.file_path // empty' 2>/dev/null)" || exit 0
[ -n "$file" ] && [ -f "$file" ] && [ -r "$file" ] || exit 0
out="$("$bin" docstrings "$file" 2>/dev/null)"; rc=$?
[ "$rc" -eq 1 ] || exit 0
total=$(printf '%s\n' "$out" | grep -c .)
{
  echo "logan-spine: add docstrings before moving on:"
  printf '%s\n' "$out" | head -n 10
  if [ "$total" -gt 10 ]; then echo "… and $((total - 10)) more"; fi
} >&2
exit 2
