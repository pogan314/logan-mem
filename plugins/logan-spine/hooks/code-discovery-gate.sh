#!/usr/bin/env bash
# PreToolUse on Grep|Glob and PostToolUse on Read: add graph context to a search the model is about to run, or has just run.
#
# This never blocks a tool call. Every failure is silent: exit 0, no output.
set -u
. "$(dirname "$0")/lib.sh"
bin="$(lsm_bin)" || exit 0
"$bin" hook-augment 2>/dev/null
exit 0
