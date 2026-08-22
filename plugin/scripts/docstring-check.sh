#!/usr/bin/env bash
# PostToolUse hook (Edit|Write): report missing docstrings in the file just written.
# Exit 2 shows stderr to Claude without failing the tool; anything else exits 0 silently.
set -u
command -v logan-spine-mcp >/dev/null 2>&1 || exit 0
command -v jq >/dev/null 2>&1 || exit 0
file="$(jq -r '.tool_input.file_path // empty' 2>/dev/null)" || exit 0
[ -n "$file" ] && [ -f "$file" ] && [ -r "$file" ] || exit 0
out="$(logan-spine-mcp docstrings "$file" 2>/dev/null)"; rc=$?
[ "$rc" -eq 1 ] || exit 0
total=$(printf '%s\n' "$out" | grep -c .)
{
  echo "logan-spine: add docstrings before moving on:"
  printf '%s\n' "$out" | head -n 10
  if [ "$total" -gt 10 ]; then echo "… and $((total - 10)) more"; fi
} >&2
exit 2
