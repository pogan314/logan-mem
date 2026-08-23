#!/usr/bin/env bash
# Tests for the logan-spine plugin. Runs with no Claude Code session active, and never touches the real $HOME: every test that reads or writes agent configuration points HOME at a fixture directory first.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUGIN="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PLUGIN/../.." && pwd)"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
fail=0
check() { if [ "$1" = "$2" ]; then echo "ok   $3"; else echo "FAIL $3: expected [$2] got [$1]"; fail=1; fi; }

# ---------- JSON validity and cross-file name agreement ----------
for f in "$REPO/.claude-plugin/marketplace.json" "$PLUGIN/.claude-plugin/plugin.json" "$PLUGIN/.mcp.json"; do
  jq -e . "$f" >/dev/null 2>&1
  check "$?" "0" "valid JSON: ${f#$REPO/}"
done
check "$(jq -r .name "$PLUGIN/.claude-plugin/plugin.json")" "logan-spine" "plugin.json name"
check "$(jq -r .name "$REPO/.claude-plugin/marketplace.json")" "logan-mem" "marketplace name"
check "$(jq -r '.plugins[0].name' "$REPO/.claude-plugin/marketplace.json")" "logan-spine" "marketplace entry name matches plugin"
check "$(jq -r '.plugins[0].source' "$REPO/.claude-plugin/marketplace.json")" "./plugins/logan-spine" "marketplace entry source"
check "$(jq -r '.mcpServers.spine.command' "$PLUGIN/.mcp.json")" '${CLAUDE_PLUGIN_ROOT}/bin/spine-launch.sh' "mcp command names the launcher"

# ---------- lsm_bin ----------
# A set LOGAN_SPINE_BIN is authoritative: set-and-valid wins, set-and-invalid fails outright rather than falling through to some other binary.
stub="$tmp/stub"; printf '#!/bin/sh\necho STUB\n' > "$stub"; chmod +x "$stub"

out="$(LOGAN_SPINE_BIN="$stub" bash -c ". '$PLUGIN/hooks/lib.sh'; lsm_bin")"; rc=$?
check "$rc" "0" "lsm_bin returns 0 for a valid LOGAN_SPINE_BIN"
check "$out" "$stub" "lsm_bin prints the LOGAN_SPINE_BIN path"

out="$(LOGAN_SPINE_BIN="$tmp/nope" bash -c ". '$PLUGIN/hooks/lib.sh'; lsm_bin" 2>/dev/null)"; rc=$?
check "$rc" "1" "lsm_bin returns 1 for a set-but-invalid LOGAN_SPINE_BIN"
check "$out" "" "lsm_bin prints nothing for a set-but-invalid LOGAN_SPINE_BIN"

# With LOGAN_SPINE_BIN unset and a fixture HOME holding the binary, HOME wins over PATH.
mkdir -p "$tmp/fakehome/.local/bin" "$tmp/empty"; cp "$stub" "$tmp/fakehome/.local/bin/logan-spine-mcp"
out="$(env -u LOGAN_SPINE_BIN HOME="$tmp/fakehome" PATH=/usr/bin:/bin bash -c ". '$PLUGIN/hooks/lib.sh'; lsm_bin")"; rc=$?
check "$rc" "0" "lsm_bin finds the binary under HOME"
check "$out" "$tmp/fakehome/.local/bin/logan-spine-mcp" "lsm_bin prefers HOME/.local/bin"

# Nothing anywhere: return 1, silent. PATH stays usable — blanking it would stop `env` finding bash at all, and every assertion below would then be measuring env's failure instead of ours.
if PATH=/usr/bin:/bin command -v logan-spine-mcp >/dev/null 2>&1; then
  echo "skip absent-binary tests: logan-spine-mcp is on the sanitised PATH"
else
  out="$(env -u LOGAN_SPINE_BIN HOME="$tmp/empty" PATH=/usr/bin:/bin bash -c ". '$PLUGIN/hooks/lib.sh'; lsm_bin" 2>/dev/null)"; rc=$?
  check "$rc" "1" "lsm_bin returns 1 when no binary exists"
  check "$out" "" "lsm_bin is silent when no binary exists"

  out="$(env -u LOGAN_SPINE_BIN HOME="$tmp/empty" PATH=/usr/bin:/bin "$PLUGIN/bin/spine-launch.sh" 2>"$tmp/err")"; rc=$?
  check "$rc" "127" "spine-launch exits 127 when the binary is missing"
  check "$(grep -c . "$tmp/err")" "1" "spine-launch prints exactly one line on stderr"
  check "$(grep -c 'engine binary not found' "$tmp/err")" "1" "spine-launch's one line is its own message"
fi

# HOME unset must not produce a bash diagnostic: the hooks promise silence.
out="$(env -u LOGAN_SPINE_BIN -u HOME PATH=/usr/bin:/bin bash -c "set -u; . '$PLUGIN/hooks/lib.sh'; lsm_bin" 2>"$tmp/err")"; rc=$?
check "$(grep -c 'unbound variable' "$tmp/err")" "0" "lsm_bin does not trip set -u when HOME is unset"

# ---------- spine-launch.sh ----------
out="$(LOGAN_SPINE_BIN="$stub" "$PLUGIN/bin/spine-launch.sh" 2>"$tmp/err")"; rc=$?
check "$rc" "0" "spine-launch execs the resolved binary"
check "$out" "STUB" "spine-launch passes the binary's stdout through"

# ---------- no machine paths ----------
# --untracked matters: every task writes files and runs this suite before its `git add`, so a tracked-only search would be blind to exactly the files under test. All three alternatives are written with a bracket expression so that this block cannot match itself: a bracketed character matches exactly the same text as the bare character, but neither of these lines then contains any of the literal substrings being searched for. Excluding this file from the pathspec instead would blind the check to every later append to the suite, which five tasks make.
hits="$(git -C "$REPO" grep --untracked -lE '/[h]ome/|/[U]sers/|[C]:\\' -- plugins .claude-plugin | wc -l | tr -d ' ')"
check "$hits" "0" "no absolute machine path under plugins/ or .claude-plugin/"

# ---------- claude plugin validate ----------
# This validates the manifest only, not hooks.json, agents or skills, so it is a floor and not a ceiling.
if command -v claude >/dev/null 2>&1; then
  claude plugin validate "$PLUGIN" --strict >/dev/null 2>&1
  check "$?" "0" "claude plugin validate --strict on the plugin"
  claude plugin validate "$REPO" >/dev/null 2>&1
  check "$?" "0" "claude plugin validate on the marketplace root"
else
  echo "skip claude plugin validate: claude not on PATH"
fi

exit $fail
