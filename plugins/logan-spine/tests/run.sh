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

# ---------- hooks.json shape ----------
H="$PLUGIN/hooks/hooks.json"
jq -e . "$H" >/dev/null 2>&1; check "$?" "0" "hooks.json is valid JSON"
check "$(jq -r '[.hooks[][] | .hooks[]] | length' "$H")" "5" "hooks.json declares five handlers"
check "$(jq -r '.hooks.PreToolUse[0].matcher' "$H")" "Grep|Glob" "PreToolUse matcher"
check "$(jq -r '.hooks.SessionStart[0].matcher' "$H")" "startup|resume|clear|compact|fork" "SessionStart matcher includes fork"
check "$(jq -r '.hooks.SubagentStart[0].matcher' "$H")" "*" "SubagentStart matcher"
check "$(jq -r '[.hooks.PostToolUse[].matcher] | sort | join(",")' "$H")" "Edit|Write,Read" "both PostToolUse matchers"
for c in $(jq -r '[.hooks[][] | .hooks[].command] | unique[]' "$H"); do
  case "$c" in *'${CLAUDE_PLUGIN_ROOT}'*) ;; *) echo "FAIL hook command not rooted in CLAUDE_PLUGIN_ROOT: $c"; fail=1 ;; esac
  s="$PLUGIN/${c##*\}\"/}"
  [ -x "$s" ]; check "$?" "0" "hook script executable: ${s##*/}"
done

# ---------- graph hooks are fail-open and pass output through ----------
marker="$tmp/marker"; printf '#!/bin/sh\necho GRAPHTEXT\n' > "$marker"; chmod +x "$marker"
for h in code-discovery-gate session-reminder subagent-reminder; do
  out="$(LOGAN_SPINE_BIN="$tmp/nope" bash -c "printf '{}' | '$PLUGIN/hooks/$h.sh'" 2>"$tmp/err")"; rc=$?
  check "$rc" "0" "$h exits 0 when the binary is missing"
  check "$out$(cat "$tmp/err")" "" "$h is silent when the binary is missing"
  out="$(LOGAN_SPINE_BIN="$marker" bash -c "printf '{\"hook_event_name\":\"SubagentStart\",\"agent_type\":\"logan-spine:scout\"}' | '$PLUGIN/hooks/$h.sh'" 2>/dev/null)"
  check "$out" "GRAPHTEXT" "$h passes the engine's output through"
done

# ---------- the scoped agent_type is normalised ----------
# ha_active_tier matches "scout"/"logan-spine-scout" and "auditor"/"logan-spine-auditor" literally and defaults everything else to Tier 2, and Claude Code reports a plugin agent as "<plugin>:<agent>", so the prefix has to go before the payload reaches the engine.
echoer="$tmp/echoer"; printf '#!/bin/sh\ncat\n' > "$echoer"; chmod +x "$echoer"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{\"agent_type\":\"logan-spine:scout\"}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -r .agent_type)"
check "$out" "scout" "subagent-reminder strips the logan-spine: prefix"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{\"agent_type\":\"general-purpose\"}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -r .agent_type)"
check "$out" "general-purpose" "subagent-reminder leaves other agent types alone"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{\"agent_type\":\"other-plugin:scout\"}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -r .agent_type)"
check "$out" "other-plugin:scout" "subagent-reminder only strips its own plugin prefix"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -c .)"
check "$out" "{}" "subagent-reminder is a no-op on a payload with no agent_type"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{\"agent_type\":null}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -c .)"
check "$out" '{"agent_type":null}' "subagent-reminder passes a null agent_type through unchanged"
out="$(LOGAN_SPINE_BIN="$echoer" bash -c "printf '{\"agent_type\":123}' | '$PLUGIN/hooks/subagent-reminder.sh'" 2>/dev/null | jq -c .)"
check "$out" '{"agent_type":123}' "subagent-reminder passes a non-string agent_type through unchanged"

# ---------- docstring fixtures, shared with task 6 ----------
printf 'export function f() {}\n' > "$tmp/bad.js"
printf '/** @file doc */\n/** ok */\nexport function f() {}\n' > "$tmp/good.js"
printf 'hello\n' > "$tmp/note.txt"
dstub="$tmp/dstub"
cat > "$dstub" <<'STUB'
#!/bin/sh
# Stands in for `logan-spine-mcp docstrings [--all] <file>...`: one finding per `export function` whose preceding line is not a `/**` comment, across every file given. Exit 1 if any finding, 0 otherwise. It must accept many files because docstring-coverage.sh passes the whole list in one xargs invocation, and it must honour the doc comment because the "clean file" fixture has one.
shift
[ "${1:-}" = "--all" ] && shift
rc=0
for f in "$@"; do
  n=$(awk '/^export function/ { if (prev !~ /^\/\*\*/) c++ } { prev=$0 } END { print c+0 }' "$f" 2>/dev/null || echo 0)
  i=1
  while [ "$i" -le "$n" ]; do echo "$f: function f$i"; rc=1; i=$((i+1)); done
done
exit $rc
STUB
chmod +x "$dstub"
# Prove the stub itself behaves, or every assertion below is measuring the wrong thing.
"$dstub" docstrings "$tmp/good.js" >/dev/null 2>&1; check "$?" "0" "the stub calls the documented fixture clean"
"$dstub" docstrings "$tmp/bad.js" >/dev/null 2>&1; check "$?" "1" "the stub calls the undocumented fixture dirty"
"$dstub" docstrings "$tmp/good.js" "$tmp/bad.js" > "$tmp/multi.out" 2>&1
check "$(grep -c 'bad.js' "$tmp/multi.out")" "1" "the stub reports findings across multiple files in one invocation"

# ---------- docstring-check contract ----------
out="$(LOGAN_SPINE_BIN="$dstub" bash -c "printf '{\"tool_input\":{\"file_path\":\"$tmp/bad.js\"}}' | '$PLUGIN/hooks/docstring-check.sh'" 2>"$tmp/err")"; rc=$?
check "$rc" "2" "docstring-check exits 2 on a finding"
check "$out" "" "docstring-check prints nothing on stdout"
check "$(head -1 "$tmp/err")" "logan-spine: add docstrings before moving on:" "docstring-check header line"
# Assert the content of the findings, not only the line counts: a hook that printed the right number of generic placeholder lines would otherwise pass every check in this block.
check "$(grep -c ' function f1$' "$tmp/err")" "1" "docstring-check names the undocumented function"
out="$(LOGAN_SPINE_BIN="$dstub" bash -c "printf '{\"tool_input\":{\"file_path\":\"$tmp/good.js\"}}' | '$PLUGIN/hooks/docstring-check.sh'" 2>"$tmp/err")"; rc=$?
check "$rc" "0" "docstring-check exits 0 on a clean file"
check "$(cat "$tmp/err")" "" "docstring-check is silent on a clean file"
out="$(LOGAN_SPINE_BIN="$dstub" bash -c "printf '{\"tool_input\":{\"file_path\":\"$tmp/note.txt\"}}' | '$PLUGIN/hooks/docstring-check.sh'" 2>"$tmp/err")"; rc=$?
check "$rc" "0" "docstring-check exits 0 on an unparsed .txt file"
check "$(cat "$tmp/err")" "" "docstring-check is silent on an unparsed .txt file"
LOGAN_SPINE_BIN="$tmp/nope" bash -c "printf '{\"tool_input\":{\"file_path\":\"$tmp/bad.js\"}}' | '$PLUGIN/hooks/docstring-check.sh'" 2>/dev/null; rc=$?
check "$rc" "0" "docstring-check exits 0 when the binary is missing"

# ---------- cap at 10 findings plus a remainder line ----------
{ for i in $(seq 1 15); do printf 'export function f%s() {}\n' "$i"; done; } > "$tmp/many.js"
LOGAN_SPINE_BIN="$dstub" bash -c "printf '{\"tool_input\":{\"file_path\":\"$tmp/many.js\"}}' | '$PLUGIN/hooks/docstring-check.sh'" 2>"$tmp/err"
check "$(grep -c 'and 5 more' "$tmp/err")" "1" "docstring-check reports the remainder"
check "$(grep -c . "$tmp/err")" "12" "docstring-check prints header + 10 findings + remainder"
check "$(grep -c ' function f' "$tmp/err")" "10" "docstring-check lists ten named findings before the remainder"

# ---------- agents ----------
for a in scout verify auditor; do
  f="$PLUGIN/agents/$a.md"
  [ -f "$f" ]; check "$?" "0" "agent file exists: $a.md"
  check "$(awk '/^name:/{print $2; exit}' "$f")" "$a" "$a.md name matches its filename"
  check "$(grep -c 'logan-spine-mcp' "$f")" "0" "$a.md never names the old server, in tools or in prose"
  check "$(grep -c '^mcpServers:' "$f")" "0" "$a.md drops mcpServers (ignored for plugin agents)"
  check "$(grep -c '^permissionMode:' "$f")" "0" "$a.md drops permissionMode (ignored for plugin agents)"
  check "$(grep -c '  - mcp__' "$f")" "$(grep -c '  - mcp__plugin_logan-spine_spine__' "$f")" "$a.md: every mcp__ tool uses the plugin prefix"
  check "$(awk '/^skills:/{print; exit}' "$f")" "skills: [graph]" "$a.md preloads the graph skill"
  # No write tool is reachable. This is what replaces the permissionMode: plan that plugin agents ignore.
  check "$(grep -cE '^  - (Write|Edit|Bash|NotebookEdit|Task|Agent)$' "$f")" "0" "$a.md grants no write or spawn tool"
done
check "$(grep -c 'mcp__plugin_logan-spine_spine__' "$PLUGIN/agents/verify.md")" "11" "verify.md carries 11 graph tools"
check "$(grep -c 'mcp__plugin_logan-spine_spine__' "$PLUGIN/agents/auditor.md")" "11" "auditor.md carries 11 graph tools"
check "$(grep -c 'mcp__plugin_logan-spine_spine__' "$PLUGIN/agents/scout.md")" "7" "scout.md carries 7 graph tools"
# The graph-unavailable paragraph exists only on the installed files and in no commit; losing it in the move is the specific regression this guards.
for a in scout verify auditor; do
  check "$(grep -c 'do not stall and do not guess' "$PLUGIN/agents/$a.md")" "1" "$a.md keeps the graph-unavailable fallback paragraph"
done

# ---------- skill ----------
S="$PLUGIN/skills/graph/SKILL.md"
[ -f "$S" ]; check "$?" "0" "SKILL.md exists at skills/graph/"
# A plugin skill's frontmatter name overrides its directory name, so a stale name: logan-spine here would address the skill as /logan-spine:logan-spine and silently break every agent's skills: entry.
check "$(awk '/^name:/{print $2; exit}' "$S")" "graph" "SKILL.md name matches its directory"
check "$(grep -c 'Use the codebase knowledge graph for structural code queries' "$S")" "1" "SKILL.md keeps its trigger description"
# Derive the expected value from SKILL.md rather than repeating the literal, so this check verifies the relationship its description claims instead of relying on two independent literals staying in sync.
skillname="$(awk '/^name:/{print $2; exit}' "$S")"
for a in scout verify auditor; do
  check "$(awk '/^skills:/{print; exit}' "$PLUGIN/agents/$a.md")" "skills: [$skillname]" "$a.md names the skill by the name SKILL.md declares"
done

# ---------- docstring-coverage ----------
COV="$PLUGIN/scripts/docstring-coverage.sh"
[ -x "$COV" ]; check "$?" "0" "docstring-coverage.sh is executable"
covdir="$tmp/cov"; mkdir -p "$covdir"
cp "$tmp/bad.js" "$tmp/good.js" "$tmp/note.txt" "$covdir/"
( cd "$covdir" && git init -q && git add bad.js good.js note.txt ) >/dev/null 2>&1

# It resolves through lsm_bin now, not PATH, so a missing binary is exit 2 even when PATH would have found one. This must run against a real git work tree: the script also exits 2 when `git ls-files` fails, so pointing it at a non-repository would make the assertion pass for the wrong reason and a silent PATH fallback would go undetected.
LOGAN_SPINE_BIN="$tmp/nope" "$COV" "$covdir" >/dev/null 2>&1
check "$?" "2" "coverage exits 2 when the binary is missing"

LOGAN_SPINE_BIN="$dstub" "$COV" "$covdir" > "$tmp/covout" 2>&1
check "$?" "1" "coverage exits 1 when something is missing"
check "$(grep -c 'bad.js' "$tmp/covout")" "1" "coverage lists bad.js"
check "$(grep -c 'good.js' "$tmp/covout")" "0" "coverage lists nothing for good.js"

cleandir="$tmp/clean"; mkdir -p "$cleandir"; cp "$tmp/good.js" "$cleandir/"
( cd "$cleandir" && git init -q && git add good.js ) >/dev/null 2>&1
out="$(LOGAN_SPINE_BIN="$dstub" "$COV" "$cleandir" 2>&1)"; rc=$?
check "$rc" "0" "coverage exits 0 on a clean tree"
check "$out" "" "coverage is silent on a clean tree"

# A non-git directory is a real error, never a false green.
nogit="$tmp/nogit"; mkdir -p "$nogit"; printf 'export function f() {}\n' > "$nogit/x.js"
LOGAN_SPINE_BIN="$dstub" "$COV" "$nogit" >/dev/null 2>&1; rc=$?
if [ "$rc" != "0" ] && [ "$rc" != "1" ]; then echo "ok   coverage errors on a non-git dir"; else echo "FAIL coverage errors on a non-git dir: got rc=$rc"; fail=1; fi

# ---------- the engine's real docstrings contract ----------
# Everything above uses a stub, so without this block nothing in the repo would test the shape the engine actually emits.
realbin="$(env -u LOGAN_SPINE_BIN bash -c ". '$PLUGIN/hooks/lib.sh'; lsm_bin" 2>/dev/null)"
if [ -n "$realbin" ] && [ -x "$realbin" ]; then
  "$realbin" docstrings "$tmp/good.js" >/dev/null 2>&1
  check "$?" "0" "the real engine calls the documented fixture clean"
  out="$("$realbin" docstrings "$tmp/bad.js" 2>/dev/null)"; rc=$?
  check "$rc" "1" "the real engine calls the undocumented fixture dirty"
  check "$(printf '%s\n' "$out" | grep -c ' function f$')" "1" "the real engine names the undocumented function"
  # The engine emits a file line as well as a function line per finding. Assert both, so the guarded block covers the whole documented shape rather than half of it.
  check "$(printf '%s\n' "$out" | grep -c 'bad\.js')" "2" "the real engine emits both a file line and a function line"
else
  echo "skip real-engine docstrings tests: no logan-spine-mcp resolvable"
fi

# ---------- logan-spine-tools is gone ----------
[ ! -e "$REPO/plugins/logan-spine-tools" ]; check "$?" "0" "logan-spine-tools is removed"

exit $fail
