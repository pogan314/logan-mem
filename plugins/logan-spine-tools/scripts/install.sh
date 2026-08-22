#!/usr/bin/env bash
# Install logan-spine on this machine: build, place the binary, run upstream's Claude Code
# installer, enable auto-index, and copy the plugin into ~/.claude/skills/.
# Usage: plugins/logan-spine-tools/scripts/install.sh        (env LSM_BIN_DIR overrides ~/.local/bin)
set -euo pipefail
: "${HOME:?HOME must be set}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BIN_DIR="${LSM_BIN_DIR:-$HOME/.local/bin}"

echo "[1/5] build (cold ≈10 min without ccache)"
"$ROOT/spine/scripts/build.sh" --version "$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)"

echo "[2/5] binary -> $BIN_DIR/logan-spine-mcp"
mkdir -p "$BIN_DIR"
cp "$ROOT/spine/build/c/logan-spine-mcp" "$BIN_DIR/logan-spine-mcp"
chmod +x "$BIN_DIR/logan-spine-mcp"
case ":$PATH:" in *":$BIN_DIR:"*) ;; *) echo "warning: $BIN_DIR is not on PATH" >&2 ;; esac

echo "[3/5] upstream installer (Claude Code only)"
"$BIN_DIR/logan-spine-mcp" install --clients=claude -y

echo "[4/5] auto-index on"
"$BIN_DIR/logan-spine-mcp" config set auto_index true

echo "[5/5] plugin -> ~/.claude/skills/logan-spine-tools"
rm -rf "$HOME/.claude/skills/logan-spine-tools"
mkdir -p "$HOME/.claude/skills"
cp -r "$ROOT/plugins/logan-spine-tools" "$HOME/.claude/skills/logan-spine-tools"
command -v jq >/dev/null || echo "warning: jq not found — the docstring hook will do nothing" >&2
echo "done — start a new Claude Code session (or /reload-plugins) to load the hook"
