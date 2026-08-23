#!/usr/bin/env bash
# Install logan-spine on this machine: build the engine, place the binary, make sure it is on PATH, and register this repository as a plugin marketplace. It deliberately does NOT enable the plugin anywhere: enabling is a per-repository decision and the whole point of this version.
# Usage: plugins/logan-spine/scripts/install.sh   (env LSM_BIN_DIR overrides ~/.local/bin)
set -euo pipefail
: "${HOME:?HOME must be set}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BIN_DIR="${LSM_BIN_DIR:-$HOME/.local/bin}"

# Prefer the main checkout, because a worktree is deleted at merge and a marketplace pinned to a path that no longer exists stops resolving. But only if it actually holds the marketplace file: while this work is on a branch, it does not.
COMMON_GIT="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)"
MAIN_CHECKOUT="$(dirname "$COMMON_GIT")"
if [ -f "$MAIN_CHECKOUT/.claude-plugin/marketplace.json" ]; then
  MARKET="$MAIN_CHECKOUT"
else
  MARKET="$ROOT"
  echo "note: $MAIN_CHECKOUT has no .claude-plugin/marketplace.json, registering $ROOT instead. Re-run this script from the main checkout once this branch is merged." >&2
fi

echo "[1/5] build (cold ≈10 min without ccache)"
"$ROOT/spine/scripts/build.sh" --version "$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)"

echo "[2/5] binary -> $BIN_DIR/logan-spine-mcp"
mkdir -p "$BIN_DIR"
# Copy to a sibling name and rename over the destination. A rename succeeds over a running binary where a direct copy fails with "Text file busy", and unlike moving the build output it leaves spine/build/c/logan-spine-mcp in place for smoke-local.sh, smoke-invariants.sh, soak-legs.sh, benchmark-search-graph.sh and setup.sh, all of which take it as an argument.
cp "$ROOT/spine/build/c/logan-spine-mcp" "$BIN_DIR/logan-spine-mcp.new"
chmod +x "$BIN_DIR/logan-spine-mcp.new"
mv "$BIN_DIR/logan-spine-mcp.new" "$BIN_DIR/logan-spine-mcp"

echo "[3/5] PATH"
# The engine's installer used to write this line; we stop calling it, so we own it. Idempotent, and it names whatever BIN_DIR actually is rather than assuming the default.
case ":$PATH:" in
  *":$BIN_DIR:"*) echo "  already on PATH" ;;
  *)
    if grep -qF "# Added by logan-spine install" "$HOME/.bashrc" 2>/dev/null; then
      echo "  already in ~/.bashrc; open a new shell"
    else
      if [ "$BIN_DIR" = "$HOME/.local/bin" ]; then p='$HOME/.local/bin'; else p="$BIN_DIR"; fi
      printf '\n# Added by logan-spine install\nexport PATH="%s:$PATH"\n' "$p" >> "$HOME/.bashrc"
      echo "  appended to ~/.bashrc; open a new shell"
    fi
    ;;
esac

echo "[4/5] auto-index on"
"$BIN_DIR/logan-spine-mcp" config set auto_index true

echo "[5/5] marketplace -> $MARKET"
# `add` is idempotent and self-healing on claude 2.1.241, measured against fixture homes: it exits 0 whether registering fresh, confirming an unchanged registration, or re-pointing an existing name at a new path — which is exactly what a post-merge re-run needs. `update` fails when the name is not registered yet. The elif is defensive only, for a claude version where `add` does fail on a duplicate name.
if claude plugin marketplace add "$MARKET" 2>&1; then
  echo "  registered"
elif claude plugin marketplace update logan-mem 2>&1; then
  echo "  already registered; refreshed"
else
  echo "  could not register the marketplace; see the messages above" >&2
  exit 1
fi

cat <<EOF

done. The plugin is registered but not enabled anywhere.

To enable it for one repository, from that repository's root:
  claude plugin install logan-spine@logan-mem --scope project
then restart Claude Code, or run /reload-plugins.

A repository whose .claude/settings.json already commits "enabledPlugins": { "logan-spine@logan-mem": true } needs nothing further: that entry loads the plugin now that this marketplace is registered under \$HOME, and it was inert before.

To turn it off for that repository again:
  claude plugin disable logan-spine@logan-mem --scope project
EOF
