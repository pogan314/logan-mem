#!/usr/bin/env bash
# SessionStart on startup, resume, clear, compact and fork: tell the session which graph project is indexed and which evidence tier is in force.
#
# Fail-open: it never blocks a session and never logs hook or prompt content.
set -u
. "$(dirname "$0")/lib.sh"
bin="$(lsm_bin)" || exit 0
"$bin" hook-augment 2>/dev/null
exit 0
