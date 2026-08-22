#!/usr/bin/env bash
# Docstring coverage over a git checkout: every tracked (or staged) file through
# `logan-spine-mcp docstrings`. Usage: docstring-coverage.sh [--all] [dir]
# Exit 0 = nothing missing, 1 = findings (xargs reports a child exit of 1 as 123; mapped back), anything else = error (e.g. not a git checkout).
set -u -o pipefail
all=""
if [ "${1:-}" = "--all" ]; then all="--all"; shift; fi
dir="${1:-.}"
git -C "$dir" ls-files -z | ( cd "$dir" && xargs -0r logan-spine-mcp docstrings $all )
rc=$?
[ "$rc" -eq 123 ] && rc=1
exit "$rc"
