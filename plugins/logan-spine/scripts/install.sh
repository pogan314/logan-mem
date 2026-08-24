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
# Publish through the engine's own install path rather than a bare copy-and-rename.
#
# The engine runs ONE daemon per OS account and refuses any process whose build fingerprint differs from the resident daemon's ("crash-safe exact-build admission", spine/src/daemon/version_cohort.h; the comparison is spine/src/daemon/service.c). A plain file swap leaves that daemon running from its already-open inode, so every NEW session on the machine is refused until the last client of the old build disconnects. Measured 2026-08-23: that lasted about sixteen hours and wrote 63 records into ~/.cache/logan-spine-mcp/logs/daemon-conflicts.ndjson. Reproduced deterministically in 30 seconds: hold a live client on one build, invoke another, and the second is refused.
#
# `install` coordinates instead of colliding: it asks the resident daemon to drain its sessions, takes the version-cohort mutation lock so no new admission races the swap, stages the candidate out of line, verifies it by running --version on it, and publishes atomically.
#
# --skip-config is what makes this safe for version 02. Without it the engine recreates the whole version-01 global footprint that unregister-global.sh exists to remove: three agent files under ~/.claude/agents/, the mcpServers entry in ~/.claude.json, and the PreToolUse, PostToolUse, SessionStart and SubagentStart hooks. Verified by diffing two --dry-run runs 2026-08-24: those eleven lines are present without the flag and absent with it, leaving "Would install binary" as the only action. --clients needs a value even so, because the parser rejects both an empty list and a "none" token.
"$ROOT/spine/build/c/logan-spine-mcp" install \
  --force --skip-config --clients=claude --dir="$BIN_DIR" -y

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
# Non-fatal on purpose. Under `set -e` a failure here would abort the script before step 5, leaving the binary installed but the marketplace unregistered — and a repository whose .claude/settings.json already commits enabledPlugins is then silently inert, with nothing on screen explaining why. auto_index is read fresh by the daemon at the start of each session (spine/src/daemon/application.c), not cached at install time, so setting it later costs nothing.
if ! "$BIN_DIR/logan-spine-mcp" config set auto_index true; then
  echo "  warning: could not set auto_index; set it later with: logan-spine-mcp config set auto_index true" >&2
fi

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
