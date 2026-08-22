#!/usr/bin/env bash
# Tests for plugin/scripts. Needs the built binary: pass its path as $1 or have logan-spine-mcp on PATH.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUGIN="$(cd "$HERE/.." && pwd)"
BIN="${1:-}"
if [ -n "$BIN" ]; then BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"; export PATH="$(dirname "$BIN"):$PATH"; fi
command -v logan-spine-mcp >/dev/null || { echo "logan-spine-mcp not on PATH (pass path as \$1)"; exit 2; }
command -v jq >/dev/null || { echo "jq not installed — the hook no-ops without it"; exit 2; }
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
fail=0
check() { if [ "$1" = "$2" ]; then echo "ok   $3"; else echo "FAIL $3: expected [$2] got [$1]"; fail=1; fi; }

printf 'export function f() {}\n' > "$tmp/bad.js"
printf '/** @file doc */\n/** ok */\nexport function f() {}\n' > "$tmp/good.js"
printf 'hello\n' > "$tmp/note.txt"

# hook: missing docstrings -> exit 2, header + findings on stderr, nothing on stdout
set +e
out="$(printf '{"tool_name":"Edit","tool_input":{"file_path":"%s"}}' "$tmp/bad.js" | "$PLUGIN/scripts/docstring-check.sh" 2>"$tmp/err")"; rc=$?
set -e
check "$rc" "2" "hook exits 2 on missing docstrings"
check "$out" "" "hook prints nothing on stdout"
check "$(head -1 "$tmp/err")" "logan-spine: add docstrings before moving on:" "hook header line"
check "$(grep -c ' function f$' "$tmp/err")" "1" "hook lists the undocumented function"

# hook: complete file -> exit 0, silent (needs B5b: the doc comment precedes `export`)
set +e
out="$(printf '{"tool_name":"Write","tool_input":{"file_path":"%s"}}' "$tmp/good.js" | "$PLUGIN/scripts/docstring-check.sh" 2>"$tmp/err")"; rc=$?
set -e
check "$rc" "0" "hook exits 0 on complete file"
check "$(cat "$tmp/err")" "" "hook silent on complete file"

# hook: unknown language -> exit 0, silent
set +e
printf '{"tool_name":"Write","tool_input":{"file_path":"%s"}}' "$tmp/note.txt" | "$PLUGIN/scripts/docstring-check.sh" 2>"$tmp/err"; rc=$?
set -e
check "$rc" "0" "hook exits 0 on .txt"

# hook: cap at 10 lines (1 file + 9 functions) plus a remainder line; 15 findings total
{ for i in $(seq 1 14); do printf 'export function f%s() {}\n' "$i"; done; } > "$tmp/many.js"
set +e
printf '{"tool_input":{"file_path":"%s"}}' "$tmp/many.js" | "$PLUGIN/scripts/docstring-check.sh" 2>"$tmp/err"; rc=$?
set -e
check "$rc" "2" "hook exits 2 on many findings"
check "$(grep -c ' function f' "$tmp/err")" "9" "hook shows 9 functions after the file line"
check "$(grep -c 'and 5 more' "$tmp/err")" "1" "hook reports the remainder"
check "$(grep -c . "$tmp/err")" "12" "header + 10 findings + remainder"

# coverage: runs over git ls-files (staged is enough; no commit, no identity)
( cd "$tmp" && git init -q && git add bad.js good.js note.txt ) >/dev/null 2>&1
set +e
"$PLUGIN/scripts/docstring-coverage.sh" "$tmp" > "$tmp/cov" 2>&1; rc=$?
set -e
check "$rc" "1" "coverage exits 1 when something is missing"
check "$(grep -c 'bad.js' "$tmp/cov")" "2" "coverage lists bad.js twice (file + function)"
check "$(grep -c 'good.js' "$tmp/cov")" "0" "coverage lists nothing for good.js"

# coverage: non-git directory -> not 0, not 1 (a real error, not a false green)
nogit="$(mktemp -d)"; trap 'rm -rf "$tmp" "$nogit"' EXIT
printf 'export function f() {}\n' > "$nogit/x.js"
set +e
"$PLUGIN/scripts/docstring-coverage.sh" "$nogit" >/dev/null 2>&1; rc=$?
set -e
if [ "$rc" != "0" ] && [ "$rc" != "1" ]; then echo "ok   coverage errors on non-git dir"; else echo "FAIL coverage errors on non-git dir: got rc=$rc"; fail=1; fi

# timing: one hook invocation, for the record
TIMEFORMAT='hook wall time: %R s'; time ( printf '{"tool_input":{"file_path":"%s"}}' "$tmp/bad.js" | "$PLUGIN/scripts/docstring-check.sh" >/dev/null 2>&1 || true )
exit $fail
