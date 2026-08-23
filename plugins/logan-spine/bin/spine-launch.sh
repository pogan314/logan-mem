#!/usr/bin/env bash
# The MCP server's entry point, named by the plugin's .mcp.json. Resolves the engine binary and replaces this process with it, so stdio passes straight through untouched.
set -u
. "$(dirname "$0")/../hooks/lib.sh"
bin="$(lsm_bin)" || { echo "logan-spine: engine binary not found; see the plugin README" >&2; exit 127; }
exec "$bin"
