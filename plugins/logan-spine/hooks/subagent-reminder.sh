#!/usr/bin/env bash
# SubagentStart: give the child the graph context and its evidence tier.
#
# The engine picks the tier by exact string match on agent_type against "scout", "logan-spine-scout", "auditor" and "logan-spine-auditor", and falls back to Tier 2 for anything else (spine/src/cli/hook_augment.c). Claude Code reports a plugin agent's type as "<plugin>:<agent>" — measured 2026-08-23 — which matches none of those, so strip our own prefix before handing the payload on. Without this every subagent would silently receive Tier 2 guidance.
#
# Fail-open: any failure exits 0 with no output.
set -u
. "$(dirname "$0")/lib.sh"
bin="$(lsm_bin)" || exit 0
payload="$(cat)"
if command -v jq >/dev/null 2>&1; then
  payload="$(printf '%s' "$payload" | jq -c 'if (.agent_type? // "") | startswith("logan-spine:") then .agent_type |= sub("^logan-spine:"; "") else . end' 2>/dev/null || printf '%s' "$payload")"
fi
printf '%s' "$payload" | "$bin" hook-augment 2>/dev/null
exit 0
