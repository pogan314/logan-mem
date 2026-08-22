#!/usr/bin/env bash
# Docstring coverage over a git checkout: every tracked (or staged) file through
# `logan-spine-mcp docstrings`. Usage: docstring-coverage.sh [--all] [dir]
# Exit 0 = nothing missing, 1 = something missing, 2 = error (not a git checkout, or the binary is missing).
# Portability: no `xargs -r` and no dependence on xargs's exit code for a failed child (GNU documents 123,
# FreeBSD/macOS documents only 0/1/126/127 and leaves that case unspecified). The empty-input case is
# handled before xargs runs, and any other nonzero xargs status is read as "findings".
set -u
all=""
if [ "${1:-}" = "--all" ]; then all="--all"; shift; fi
dir="${1:-.}"
list="$(mktemp)"
trap 'rm -f "$list"' EXIT
git -C "$dir" ls-files -z > "$list" || exit 2
[ -s "$list" ] || exit 0
( cd "$dir" && xargs -0 logan-spine-mcp docstrings $all ) < "$list"
rc=$?
case "$rc" in
  0) ;;
  126|127) rc=2 ;;
  *) rc=1 ;;
esac
exit "$rc"
