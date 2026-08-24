#!/usr/bin/env bash
# Turn the engine's graph visualizer on or off, and report where it is.
# Usage: visualizer.sh [on|off|status] [--port N]
# Exit 0 on success, 1 if the visualizer is off (for `status`), 2 could not run.
#
# Scope, so the output is not misleading: the visualizer is a listener owned by the ONE per-account daemon, not by a repository. Turning it on turns it on for every indexed project on this machine, and there is one port for all of them. This script lives in the repository because that is where you run it from, not because the setting is per-repository. `status` therefore reports the machine's state, and the URL it prints serves whichever projects the daemon has indexed.
set -u
. "$(dirname "$0")/../hooks/lib.sh"
bin="$(lsm_bin)" || { echo "logan-spine: engine binary not found; see the plugin README" >&2; exit 2; }

action="${1:-status}"
[ $# -gt 0 ] && shift
port=""
while [ $# -gt 0 ]; do
  case "$1" in
    --port) port="${2:-}"; shift 2 || exit 2 ;;
    --port=*) port="${1#--port=}"; shift ;;
    *) echo "visualizer.sh: unknown argument '$1'" >&2; exit 2 ;;
  esac
done
if [ -n "$port" ]; then
  case "$port" in (*[!0-9]*|'') echo "visualizer.sh: --port needs a number, got '$port'" >&2; exit 2 ;; esac
fi

# Read the persisted values. `config list` is the only surface that reports both, and it never contacts the daemon, so it still answers when no daemon is running.
ui_state() { "$bin" config list 2>/dev/null | awk -v k="$1" '$1==k {print $3}'; }

case "$action" in
  on)
    # --ui/--port go through the daemon mutation path (parse_ui_flags in spine/src/main.c), which pushes the change to a running daemon rather than only persisting it.
    if [ -n "$port" ]; then set -- "--ui=true" "--port=$port"; else set -- "--ui=true"; fi
    # NOT a command substitution: `--ui=true` can leave a daemon running, and a daemon inherits the pipe that $( ) waits on, so capturing this way hangs until the daemon exits. A temp file has no such reader, and stdin is closed so nothing can block on it either.
    log="$(mktemp)"; trap 'rm -f "$log"' EXIT
    "$bin" "$@" > "$log" 2>&1 < /dev/null; rc=$?
    out="$(cat "$log")"
    # The engine's own refusal names an upstream make target, which is the wrong instruction for anyone who got this binary from install.sh. Translate it; leave every other failure to speak for itself.
    case "$out" in
      *"without UI support"*)
        echo "logan-spine: this engine binary has no visualizer embedded." >&2
        echo "  Re-install with it: plugins/logan-spine/scripts/install.sh --with-ui" >&2
        exit 2
        ;;
    esac
    [ "$rc" -eq 0 ] || { [ -n "$out" ] && printf '%s\n' "$out" >&2; exit 2; }
    p="$(ui_state ui_port)"
    echo "visualizer on -> http://127.0.0.1:${p:-9749}"
    echo "  it serves every project this machine has indexed, not just this repository"
    ;;
  off)
    "$bin" --ui=false || exit 2
    echo "visualizer off"
    ;;
  status)
    e="$(ui_state ui_enabled)"
    p="$(ui_state ui_port)"
    if [ "$e" = "true" ]; then
      echo "visualizer on -> http://127.0.0.1:${p:-9749}"
      exit 0
    fi
    echo "visualizer off (turn it on with: $0 on)"
    exit 1
    ;;
  *)
    echo "Usage: visualizer.sh [on|off|status] [--port N]" >&2
    exit 2
    ;;
esac
