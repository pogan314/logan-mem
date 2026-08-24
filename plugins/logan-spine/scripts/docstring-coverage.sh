#!/usr/bin/env bash
# Report every tracked file in a repository whose symbols lack docstrings.
# Usage: docstring-coverage.sh [--all] [dir]
# Exit 0 clean, 1 findings, 2 could not run.
# Portability: no `xargs -r` and no dependence on xargs's exit code for a failed child (GNU documents 123, FreeBSD/macOS documents only 0/1/126/127 and leaves that case unspecified). The empty-input case is handled before xargs runs, and any other nonzero xargs status is read as "findings".
set -u
. "$(dirname "$0")/../hooks/lib.sh"
bin="$(lsm_bin)" || { echo "logan-spine: engine binary not found" >&2; exit 2; }
all=""
if [ "${1:-}" = "--all" ]; then all="--all"; shift; fi
dir="${1:-.}"
list="$(mktemp)"
trap 'rm -f "$list"' EXIT
git -C "$dir" ls-files -z > "$list" || exit 2
[ -s "$list" ] || exit 0
( cd "$dir" && xargs -0 "$bin" docstrings $all ) < "$list"
rc=$?
case "$rc" in
  0) ;;
  126|127) rc=2 ;;
  *) rc=1 ;;
esac
exit "$rc"
