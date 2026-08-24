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


# ---------- unregister-global ----------
UG="$PLUGIN/scripts/unregister-global.sh"
[ -x "$UG" ]; check "$?" "0" "unregister-global.sh is executable"

make_fixture() {
  local F="$1"
  mkdir -p "$F/.claude/hooks" "$F/.claude/agents" "$F/.claude/skills/logan-spine" "$F/.claude/skills/logan-spine-tools" "$F/.claude/skills/unrelated"
  for h in lsm-code-discovery-gate lsm-session-reminder lsm-subagent-reminder; do printf '#!/bin/sh\nexit 0\n' > "$F/.claude/hooks/$h"; chmod +x "$F/.claude/hooks/$h"; done
  printf 'x\n' > "$F/.claude/hooks/unrelated-hook"
  for a in logan-spine logan-spine-scout logan-spine-auditor unrelated-agent; do printf -- '---\nname: %s\n---\nbody\n' "$a" > "$F/.claude/agents/$a.md"; done
  printf -- '---\nname: logan-spine\n---\n' > "$F/.claude/skills/logan-spine/SKILL.md"
  printf -- '---\nname: unrelated\n---\n' > "$F/.claude/skills/unrelated/SKILL.md"
  cat > "$F/.claude/settings.json" <<'JSON'
{
  "hooks": {
    "PreToolUse": [ { "matcher": "Grep|Glob", "hooks": [ { "type": "command", "command": "\"$HOME/.claude/hooks/lsm-code-discovery-gate\"", "timeout": 5 } ] } ],
    "PostToolUse": [ { "matcher": "Read", "hooks": [ { "type": "command", "command": "\"$HOME/.claude/hooks/lsm-code-discovery-gate\"", "timeout": 5 } ] } ],
    "SessionStart": [
      { "matcher": "startup", "hooks": [ { "type": "command", "command": "\"$HOME/.claude/hooks/lsm-session-reminder\"", "timeout": 5 } ] },
      { "matcher": "resume", "hooks": [ { "type": "command", "command": "\"$HOME/.claude/hooks/lsm-session-reminder\"", "timeout": 5 } ] }
    ],
    "SubagentStart": [ { "matcher": "*", "hooks": [
      { "type": "command", "command": "\"$HOME/.claude/hooks/lsm-subagent-reminder\"", "timeout": 5 },
      { "type": "command", "command": "tmux-counter-placeholder", "timeout": 5 }
    ] } ],
    "Stop": [ { "matcher": "*", "hooks": [ { "type": "command", "command": "unrelated-stop-hook", "timeout": 5 } ] } ]
  },
  "enabledPlugins": { "something@else": true }
}
JSON
  cat > "$F/.claude.json" <<'JSON'
{ "mcpServers": { "logan-spine-mcp": { "command": "/somewhere/logan-spine-mcp", "args": [] }, "other-server": { "command": "other", "args": [] } }, "projects": { "/some/path": { "allowedTools": [] } } }
JSON
}

# Dry run changes nothing and lists everything.
F1="$tmp/fx1"; make_fixture "$F1"
before="$(find "$F1" -type f | sort | xargs md5sum | md5sum)"
out="$("$UG" --home "$F1" 2>&1)"; rc=$?
after="$(find "$F1" -type f | sort | xargs md5sum | md5sum)"
check "$rc" "0" "dry run exits 0"
check "$before" "$after" "dry run changes nothing"
check "$(printf '%s' "$out" | grep -c '^remove: .*/hooks/lsm-code-discovery-gate$')" "1" "dry run names the gate script file"
check "$(printf '%s' "$out" | grep -c '^remove: .*/agents/logan-spine-auditor.md$')" "1" "dry run names the auditor agent"
check "$(printf '%s' "$out" | grep -c 'mcpServers.logan-spine-mcp')" "1" "dry run names the MCP entry"
check "$(printf '%s' "$out" | grep -ci 'to restore')" "1" "dry run prints the restore command"

# --yes removes ours and only ours.
F2="$tmp/fx2"; make_fixture "$F2"
"$UG" --home "$F2" --yes > "$tmp/ugout" 2>&1
check "$?" "0" "--yes exits 0"
check "$(jq '[.hooks[]?[]?.hooks[]? | select((.command // "") | test("lsm-"))] | length' "$F2/.claude/settings.json")" "0" "no lsm- handler survives"
check "$(jq -r '.hooks.SubagentStart[0].hooks[0].command' "$F2/.claude/settings.json")" "tmux-counter-placeholder" "the unrelated tmux handler survives in place"
check "$(jq '.hooks.SubagentStart | length' "$F2/.claude/settings.json")" "1" "the SubagentStart group is kept, not dropped"
# Note the .hooks prefix: has() on the root object would be false before the script ever ran, and the assertion would be vacuous.
check "$(jq '.hooks | has("SessionStart")' "$F2/.claude/settings.json")" "false" "an event left with no groups is dropped"
check "$(jq '[.hooks[] | .[] | select((.hooks|length) == 0)] | length' "$F2/.claude/settings.json")" "0" "no empty matcher group survives"
check "$(jq -r '.hooks.Stop[0].hooks[0].command' "$F2/.claude/settings.json")" "unrelated-stop-hook" "an unrelated event is untouched"
check "$(jq -r '.enabledPlugins["something@else"]' "$F2/.claude/settings.json")" "true" "unrelated settings keys survive"
check "$(jq '.mcpServers | has("logan-spine-mcp")' "$F2/.claude.json")" "false" "the MCP entry is gone"
check "$(jq -r '.mcpServers["other-server"].command' "$F2/.claude.json")" "other" "the unrelated MCP server survives"
check "$(jq -r '.projects["/some/path"] | type' "$F2/.claude.json")" "object" "per-project state in .claude.json survives"
for f in hooks/lsm-code-discovery-gate hooks/lsm-session-reminder hooks/lsm-subagent-reminder agents/logan-spine.md agents/logan-spine-scout.md agents/logan-spine-auditor.md skills/logan-spine skills/logan-spine-tools; do
  [ ! -e "$F2/.claude/$f" ]; check "$?" "0" "removed: $f"
done
for f in hooks/unrelated-hook agents/unrelated-agent.md skills/unrelated/SKILL.md; do
  [ -e "$F2/.claude/$f" ]; check "$?" "0" "kept: $f"
done
check "$(ls "$F2/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "1" "settings.json was backed up"
check "$(ls "$F2/.claude.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "1" "claude.json was backed up"

# Running it twice is safe.
"$UG" --home "$F2" --yes >/dev/null 2>&1
check "$?" "0" "a second --yes run is a no-op that still exits 0"

# The malformation has to be one that actually aborts the filter. Measured 2026-08-23 against the filter's own text: deleting a group's `hooks` key does NOT abort, because the filter's `(. // [])` guard absorbs it, so the run succeeds and every assertion below would pass for the wrong reason. Setting `hooks` to a string aborts with "Cannot iterate over string".
F3="$tmp/fx3"; make_fixture "$F3"
jq '.hooks.PostToolUse[0].hooks = "malformed"' "$F3/.claude/settings.json" > "$F3/.claude/settings.tmp" && mv "$F3/.claude/settings.tmp" "$F3/.claude/settings.json"
before3="$(md5sum < "$F3/.claude/settings.json")"
"$UG" --home "$F3" --yes >/dev/null 2>&1; rc=$?
check "$rc" "1" "a failed rewrite makes the script exit non-zero rather than reporting success"
check "$(md5sum < "$F3/.claude/settings.json")" "$before3" "a failed rewrite leaves settings.json byte-identical, not merely non-empty"
[ -s "$F3/.claude/settings.json" ] && jq -e . "$F3/.claude/settings.json" >/dev/null 2>&1
check "$?" "0" "settings.json still parses after a failed rewrite"
check "$(ls "$F3/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "1" "a failed rewrite still leaves the backup it named"

# It refuses without a HOME.
( env -u HOME "$UG" --yes >/dev/null 2>&1 ); check "$?" "1" "refuses when HOME is unset and --home is absent"

# A failed configuration edit must stop before disk. The configuration still names these hook commands, so deleting the files anyway leaves Claude Code invoking commands that no longer exist — and the run used to print "Done." while doing it.
F4="$tmp/fx4"; make_fixture "$F4"
jq '.hooks.PostToolUse[0].hooks = "malformed"' "$F4/.claude/settings.json" > "$F4/.claude/t" && mv "$F4/.claude/t" "$F4/.claude/settings.json"
before4json="$(md5sum < "$F4/.claude.json")"
"$UG" --home "$F4" --yes > "$tmp/ug4" 2>&1; rc=$?
check "$rc" "1" "a failed settings rewrite makes the whole run exit non-zero"
# The second configuration edit must not run once the first has failed: a run that prints "nothing on disk was removed" while having deleted a key from .claude.json is lying to the operator.
check "$(jq '.mcpServers | has("logan-spine-mcp")' "$F4/.claude.json")" "true" "a failed settings rewrite leaves the MCP entry in .claude.json"
check "$(md5sum < "$F4/.claude.json")" "$before4json" ".claude.json is byte-identical after a failed settings rewrite"
check "$(ls "$F4/.claude.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "0" "a failed settings rewrite never backs up .claude.json, because it never edits it"
for f in hooks/lsm-code-discovery-gate hooks/lsm-session-reminder hooks/lsm-subagent-reminder agents/logan-spine.md agents/logan-spine-scout.md skills/logan-spine; do
  [ -e "$F4/.claude/$f" ]; check "$?" "0" "a failed settings rewrite leaves $f on disk"
done
check "$(grep -c '^Done\.$' "$tmp/ug4")" "0" "a failed run never prints Done."
# The closing block names what it actually did to each thing it touched. On this route the rewrite aborted mid-filter, so no configuration file changed; the sentence about the hook, agent and skill files is the one that used to be a blanket "nothing on disk was removed".
check "$(grep -c '^No configuration file was changed\.$' "$tmp/ug4")/$(grep -ci 'no hook, agent or skill file was removed' "$tmp/ug4")" "1/1" "a mid-filter abort says no configuration file was changed and no hook, agent or skill file was removed"
# rewrite() copied the file before jq aborted, so a backup really is on disk and really was named. Making the no-backup case honest must not silence the pointer in the case where it is true.
check "$(ls "$F4/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')/$(grep -ci 'restore from the backup' "$tmp/ug4")" "1/1" "a mid-filter abort that did write a backup still points the operator at it"
# The skip line is printed in act mode and in dry run alike, so its reason has to be true in both. "the settings.json edit failed" is false in a dry run, where no edit was attempted.
check "$(grep -c '^\.claude\.json: skipped, because settings\.json could not be edited$' "$tmp/ug4")" "1" "an act run that skips .claude.json gives a reason that is true in act mode"

# The handler match must be anchored to the installed path. Unanchored, each of these three unrelated commands merely contains a handler name and was deleted. The middle one uses /opt rather than a home directory only because this suite's own machine-path check forbids that literal anywhere under plugins/.
F5="$tmp/fx5"; make_fixture "$F5"
jq '.hooks.Stop[0].hooks += [
  {"type":"command","command":"/opt/vendor/lsm-session-reminder-v2 --other","timeout":5},
  {"type":"command","command":"node /opt/me/tools/not-lsm-subagent-reminder-clone.mjs","timeout":5},
  {"type":"command","command":"echo lsm-code-discovery-gateway","timeout":5}
]' "$F5/.claude/settings.json" > "$F5/.claude/t" && mv "$F5/.claude/t" "$F5/.claude/settings.json"
"$UG" --home "$F5" --yes >/dev/null 2>&1
check "$(jq '.hooks.Stop[0].hooks | length' "$F5/.claude/settings.json")" "4" "commands that merely contain a handler name are not deleted"
check "$(jq -r '[.hooks.Stop[0].hooks[].command] | sort | join("|")' "$F5/.claude/settings.json")" "/opt/vendor/lsm-session-reminder-v2 --other|echo lsm-code-discovery-gateway|node /opt/me/tools/not-lsm-subagent-reminder-clone.mjs|unrelated-stop-hook" "each near-miss command survives verbatim"
check "$(jq '[.hooks[]?[]?.hooks[]? | select((.command // "") | test("/[.]claude/hooks/lsm-"))] | length' "$F5/.claude/settings.json")" "0" "the genuinely installed handlers are still all removed"

# A removal that fails must not report success. A directory the process cannot write is the cheapest way to make rm fail.
F6="$tmp/fx6"; make_fixture "$F6"
chmod a-w "$F6/.claude/agents"
"$UG" --home "$F6" --yes >/dev/null 2>&1; rc6=$?
chmod u+w "$F6/.claude/agents"
if [ -e "$F6/.claude/agents/logan-spine.md" ]; then
  check "$rc6" "1" "a removal that fails makes the run exit non-zero"
else
  echo "skip failed-removal test: rm succeeded in an unwritable directory"
fi

# An unparseable configuration file must be reported as unparseable, never as clean, and must stop the run before anything is deleted.
F7="$tmp/fx7"; make_fixture "$F7"
printf 'not json at all\n' > "$F7/.claude/settings.json"
"$UG" --home "$F7" --yes > "$tmp/ug7" 2>&1; rc=$?
check "$rc" "1" "an unparseable settings.json makes the run exit non-zero"
check "$(grep -ci 'cannot parse' "$tmp/ug7")" "1" "an unparseable settings.json is reported as unparseable"
check "$(grep -c 'no lsm- handlers' "$tmp/ug7")" "0" "an unparseable settings.json is never called clean"
[ -e "$F7/.claude/hooks/lsm-session-reminder" ]; check "$?" "0" "an unparseable settings.json stops the run before any removal"
# A cannot-parse route returns before rewrite() is ever called, so no backup was written and none was named. Telling the operator to restore "the backup named above" sends them after a file that does not exist.
check "$(find "$F7" -name '*logan-spine-backup*' | wc -l | tr -d ' ')/$(grep -ci 'restore from the backup' "$tmp/ug7")" "0/0" "an unparseable settings.json writes no backup and never tells the operator to restore one"
check "$(grep -ci 'no backup was made' "$tmp/ug7")" "1" "an act run that reached no rewrite says no backup was made"

F8="$tmp/fx8"; make_fixture "$F8"
printf 'not json at all\n' > "$F8/.claude.json"
"$UG" --home "$F8" --yes > "$tmp/ug8" 2>&1; rc=$?
check "$rc" "1" "an unparseable .claude.json makes the run exit non-zero"
check "$(grep -ci 'cannot parse' "$tmp/ug8")" "1" "an unparseable .claude.json is reported as unparseable"
check "$(grep -c 'no logan-spine-mcp entry' "$tmp/ug8")" "0" "an unparseable .claude.json is never called clean"
[ -e "$F8/.claude/hooks/lsm-session-reminder" ]; check "$?" "0" "an unparseable .claude.json stops the run before any removal"
# This route reaches .claude.json only after the settings.json rewrite succeeded, so a backup of settings.json is on disk and was named: here the pointer is true and must survive.
check "$(ls "$F8/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')/$(grep -ci 'restore from the backup' "$tmp/ug8")" "1/1" "an unparseable .claude.json after a successful settings rewrite still points at that backup"
# The state that makes the wording below load-bearing, asserted on disk rather than on the output: a backup exists because rewrite() ran, and the handlers this fixture started with are gone from settings.json. Without this pin a fixture that never reached the rewrite would satisfy the message assertions vacuously.
check "$(ls "$F8/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')/$(jq '[.hooks[]?[]?.hooks[]? | select((.command // "") | test("/[.]claude/hooks/lsm-"))] | length' "$F8/.claude/settings.json")" "1/0" "on this route settings.json really was rewritten: a backup exists and no lsm- handler survives in it"
# settings.json is a file on disk and its handlers are gone, so a blanket "nothing on disk was removed" is false here. The closing block must name settings.json as changed instead of denying it.
check "$(grep -ci 'nothing on disk was removed' "$tmp/ug8")/$(grep -ci 'no configuration file was changed' "$tmp/ug8")" "0/0" "a run that rewrote settings.json never claims nothing on disk was removed or that no configuration file changed"
check "$(grep -c "^Already rewritten before the failure, and left that way: $F8/.claude/settings.json\$" "$tmp/ug8")" "1" "a run that rewrote settings.json names it, with its path, as left rewritten"
check "$(grep -ci 'no hook, agent or skill file was removed' "$tmp/ug8")" "1" "that run still says the hook, agent and skill files were not removed"

# The other cannot-parse route: settings.json parses and holds no lsm- handlers, so nothing is ever rewritten, and then .claude.json cannot be parsed. No backup exists anywhere.
F10="$tmp/fx10"; make_fixture "$F10"
jq '.hooks = {"Stop": .hooks.Stop}' "$F10/.claude/settings.json" > "$F10/.claude/t" && mv "$F10/.claude/t" "$F10/.claude/settings.json"
printf 'not json at all\n' > "$F10/.claude.json"
"$UG" --home "$F10" --yes > "$tmp/ug10" 2>&1; rc=$?
check "$rc" "1" "a clean settings.json with an unparseable .claude.json makes the run exit non-zero"
# The route pin travels inside the assertion: if the fixture ever stopped taking the clean-settings route, the backup claim below would be measuring the wrong path and would still pass on its own.
check "$(grep -c 'no lsm- handlers' "$tmp/ug10")/$(find "$F10" -name '*logan-spine-backup*' | wc -l | tr -d ' ')/$(grep -ci 'restore from the backup' "$tmp/ug10")" "1/0/0" "an unparseable .claude.json with nothing to rewrite writes no backup and never tells the operator to restore one"
check "$(grep -ci 'no backup was made' "$tmp/ug10")" "1" "that run says no backup was made"
[ -e "$F10/.claude/hooks/lsm-session-reminder" ]; check "$?" "0" "that run stops before any removal"

# The dry run has to exercise the filter rather than promise an edit that cannot happen, and it has to name the collateral pruning it will do.
F9="$tmp/fx9"; make_fixture "$F9"
jq '.hooks.PostToolUse[0].hooks = "malformed"' "$F9/.claude/settings.json" > "$F9/.claude/t" && mv "$F9/.claude/t" "$F9/.claude/settings.json"
before9="$(find "$F9" -type f | sort | xargs md5sum | md5sum)"
"$UG" --home "$F9" > "$tmp/ug9" 2>&1; rc=$?
check "$rc" "1" "a dry run whose rewrite would fail exits non-zero"
# Anchored on the preview line's own text rather than counting the phrase anywhere in the output: an unanchored count makes this assertion a veto over the wording of every other line the script prints.
check "$(grep -c 'the filter aborts on this file: the rewrite would FAIL' "$tmp/ug9")" "1" "a dry run's preview line names the rewrite that would fail"
check "$(find "$F9" -type f | sort | xargs md5sum | md5sum)" "$before9" "a dry run that exercises the filter still changes nothing"
# A dry run writes nothing, so there is no backup to restore from; telling the operator to restore one sends them looking for a file that does not exist.
check "$(grep -ci 'restore from the backup' "$tmp/ug9")" "0" "a failed dry run does not tell the operator to restore a backup"
check "$(grep -ci 'no backup was made' "$tmp/ug9")" "1" "a failed dry run says no backup was made"
# A dry run attempts no edit at all, so it cannot report one as having failed — the run's own next line says "Nothing was changed". The reason given for skipping .claude.json has to hold in both modes.
check "$(grep -c '^\.claude\.json: skipped, because settings\.json could not be edited$' "$tmp/ug9")/$(grep -ci 'the settings.json edit failed' "$tmp/ug9")" "1/0" "a dry run that skips .claude.json does not say an edit failed, because none was attempted"
"$UG" --home "$F1" > "$tmp/ug1" 2>&1
check "$(grep -ci 'already empty' "$tmp/ug1")" "1" "the preview names the already-empty groups it will also drop"

# --home with no usable value is a usage error, and saying "HOME is not set and --home was not given" gets both halves wrong: HOME is set in this suite's own environment, and --home was given.
out="$("$UG" --home "" --yes 2>&1)"; rc=$?
check "$rc" "1" "--home with an empty value exits non-zero"
check "$(printf '%s' "$out" | grep -ci 'home is not set')/$(printf '%s' "$out" | grep -c 'requires a directory')" "0/1" "--home with an empty value is reported as a bad --home, not as an unset HOME"
out="$("$UG" --home 2>&1)"; rc=$?
check "$rc" "1" "a trailing bare --home exits non-zero"
check "$(printf '%s' "$out" | grep -ci 'home is not set')/$(printf '%s' "$out" | grep -c 'requires a directory')" "0/1" "a trailing bare --home is reported as a bad --home, not as an unset HOME"

# A --home that names nothing usable must be rejected before anything is examined. Measured 2026-08-24 against the pre-fix script: each of the three forms below printed ".claude.json: not present" and exited 0, so an operator who mistyped the flag was told their footprint was gone from a tree the run never looked at.
out="$("$UG" --home "$tmp/does-not-exist-xyz" --yes 2>&1)"; rc=$?
check "$rc" "1" "--home naming a missing directory exits non-zero"
check "$(printf '%s\n' "$out" | grep -cxF -e "--home is not a directory: $tmp/does-not-exist-xyz")/$(printf '%s\n' "$out" | grep -c '^Done\.$')" "1/0" "--home naming a missing directory names the offending value and never reaches Done."
printf 'x\n' > "$tmp/regfile"
out="$("$UG" --home "$tmp/regfile" --yes 2>&1)"; rc=$?
check "$rc" "1" "--home naming a regular file exits non-zero"
check "$(printf '%s\n' "$out" | grep -cxF -e "--home is not a directory: $tmp/regfile")/$(printf '%s\n' "$out" | grep -c '^Done\.$')" "1/0" "--home naming a regular file names the offending value and never reaches Done."
# The value check alone would accept this one on any machine that happens to hold a directory named --yes, and its message would name a directory rather than the mistake the operator made. Note the closing line asserted here: `--yes` is consumed as the value, so YES stays 0 and the pre-fix run ended in the dry-run sentence, not in Done.
out="$("$UG" --home --yes 2>&1)"; rc=$?
check "$rc" "1" "--home immediately followed by another flag exits non-zero"
check "$(printf '%s\n' "$out" | grep -cxF -e "--home requires a directory, not the option: --yes")/$(printf '%s\n' "$out" | grep -ci 'nothing was changed')" "1/0" "--home swallowing the next flag is reported as that, and the run stops at the argument"
# The guard must not be over-tight: a directory that exists is still accepted even with nothing in it, and the run reports each absent file rather than passing over it in silence. The silence on settings.json is what made a --home that examined nothing look like an ordinary clean run, since the .claude.json block already named its absence.
F11="$tmp/fx11"; mkdir -p "$F11"
out="$("$UG" --home "$F11" --yes 2>&1)"; rc=$?
check "$rc/$(printf '%s\n' "$out" | grep -c '^settings\.json: not present$')/$(printf '%s\n' "$out" | grep -c '^\.claude\.json: not present$')/$(printf '%s\n' "$out" | grep -c '^Done\.$')" "0/1/1/1" "a valid --home with an empty tree names both absent files and still finishes"

# ---------- unregister-global: a symlinked settings.json is not destroyed ----------
# Reproduces the reviewer's finding: `mv "$file.logan-spine-new" "$file"` unlinks whatever is AT $file. When $file is a symlink into a dotfiles tree, that replaces the link itself with a plain file, and the real file the link pointed at is never touched — while the run still reports the edit and names a backup. The fix must write through the resolved real target instead, so the link survives and the real file's content changes.
FSA="$tmp/fxsymA"; make_fixture "$FSA"
mkdir -p "$FSA/dotfiles"
mv "$FSA/.claude/settings.json" "$FSA/dotfiles/settings.json"
ln -s "$FSA/dotfiles/settings.json" "$FSA/.claude/settings.json"
before_symA="$(md5sum < "$FSA/dotfiles/settings.json")"
"$UG" --home "$FSA" --yes > "$tmp/ugsymA" 2>&1; rc=$?
check "$rc" "0" "a --yes run against a symlinked settings.json exits 0"
check "$(jq '[.hooks[]?[]?.hooks[]? | select((.command // "") | test("lsm-"))] | length' "$FSA/dotfiles/settings.json")" "0" "the real file in the dotfiles tree has its handlers removed"
[ -L "$FSA/.claude/settings.json" ]; check "$?" "0" "the path that was a symlink is still a symlink after the run"
check "$(readlink "$FSA/.claude/settings.json")" "$FSA/dotfiles/settings.json" "the symlink still points at the same real file"
check "$(ls "$FSA/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "1" "a backup was written next to the symlink"
backupA="$(ls "$FSA/.claude/settings.json".logan-spine-backup-* 2>/dev/null | head -1)"
[ -n "$backupA" ] && [ -f "$backupA" ] && [ ! -L "$backupA" ]; check "$?" "0" "the backup is a plain file, not itself a symlink"
check "$(md5sum < "$backupA" 2>/dev/null)" "$before_symA" "the backup holds the real target's original content, byte-exact"

# ---------- unregister-global: a failed rewrite of a symlinked file is reported honestly ----------
# The real target's directory is made read-only after capturing its original content, so cp (writing the backup next to the symlink, in a writable directory) and jq (writing the scratch temp file, also next to the symlink) both succeed, and only the final write onto the real target — now genuinely relocated there by the fix — fails. This is the only construction that can make that specific write fail without cp failing first: cp and the scratch temp file share $file's directory, never the target's, so this exercises the fix's own new code path rather than a pre-existing one.
FSB="$tmp/fxsymB"; make_fixture "$FSB"
mkdir -p "$FSB/dotfiles"
mv "$FSB/.claude/settings.json" "$FSB/dotfiles/settings.json"
ln -s "$FSB/dotfiles/settings.json" "$FSB/.claude/settings.json"
before_symB="$(md5sum < "$FSB/dotfiles/settings.json")"
chmod a-w "$FSB/dotfiles"
"$UG" --home "$FSB" --yes > "$tmp/ugsymB" 2>&1; rc=$?
chmod u+w "$FSB/dotfiles"
check "$rc" "1" "a write blocked by a read-only real-target directory makes the whole run exit non-zero"
[ -L "$FSB/.claude/settings.json" ]; check "$?" "0" "the symlink survives a failed write onto its target"
check "$(md5sum < "$FSB/dotfiles/settings.json")" "$before_symB" "a failed write leaves the real target byte-identical"
check "$(grep -ci 'mv failed' "$tmp/ugsymB")" "1" "the failure is reported by name, not folded into the jq-failed message"
check "$(grep -c '^No configuration file was changed\.$' "$tmp/ugsymB")" "1" "a failed write is not counted as an edit"
check "$(grep -c '^Already rewritten' "$tmp/ugsymB")" "0" "a failed write is never listed as already rewritten"
check "$(grep -ci 'restore from the backup' "$tmp/ugsymB")" "1" "the run still points the operator at the backup cp already wrote before the write failed"
check "$(ls "$FSB/.claude/settings.json".logan-spine-backup-* 2>/dev/null | wc -l | tr -d ' ')" "1" "the backup written before the failed write is left on disk"
check "$(grep -c '^\.claude\.json: skipped, because settings\.json could not be edited$' "$tmp/ugsymB")" "1" ".claude.json is skipped after the settings.json write failure"
[ ! -e "$FSB/.claude/settings.json.logan-spine-new" ]; check "$?" "0" "the failed write's scratch temp file is cleaned up, not left behind"

# ---------- install.sh publishes the binary through the engine, not a bare copy ----------
# A plain copy-and-rename leaves a resident daemon of the previous build running from its open inode, and the engine then refuses every NEW process whose build differs (spine/src/daemon/version_cohort.h). Measured 2026-08-23: sixteen hours of refusals and 63 records in daemon-conflicts.ndjson. The engine's own `install` drains sessions and takes the mutation lock first, so the script must call it rather than move the file itself.
INS="$PLUGIN/scripts/install.sh"
check "$(grep -c 'logan-spine-mcp" install' "$INS")" "1" "install.sh publishes the binary through the engine's own install subcommand"
check "$(grep -cE '^(cp|mv) .*logan-spine-mcp' "$INS")" "0" "install.sh no longer swaps the binary with a bare cp or mv"

# --skip-config is what keeps that call compatible with this version. Without it the engine recreates the entire version-01 global footprint that unregister-global.sh exists to remove. Verified 2026-08-24 by diffing two --dry-run runs: without the flag the plan names three agent files, the ~/.claude.json mcp entry and four hook events; with it, none of them.
# Anchored to the command line itself. Counting mentions across the file would also match the comment above the call that explains why the flag is there, which is prose rather than behaviour.
check "$(grep -cE '^ *--force --skip-config --clients=claude --dir="\$BIN_DIR" -y$' "$INS")" "1" "the install call passes --force and --skip-config, so a resident daemon is drained and no agent configuration is written"

# A failure setting auto_index must not abort the script before the marketplace is registered: the binary would be installed, the marketplace absent, and a repository whose committed enabledPlugins entry then resolves to nothing would look broken for no visible reason.
check "$(grep -c 'if ! "$BIN_DIR/logan-spine-mcp" config set auto_index true; then' "$INS")" "1" "a failed auto_index setting is tolerated rather than aborting under set -e"
check "$(awk '/config set auto_index/{f=1} f&&/marketplace add/{print "after"; exit}' "$INS")" "after" "the marketplace registration is reached after the auto_index step, not gated on it"

exit $fail
