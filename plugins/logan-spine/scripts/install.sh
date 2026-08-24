#!/usr/bin/env bash
# Install logan-spine on this machine: build the engine, place the binary, make sure it is on PATH, and register this repository as a plugin marketplace. It deliberately does NOT enable the plugin anywhere: enabling is a per-repository decision and the whole point of this version.
# Usage: plugins/logan-spine/scripts/install.sh   (env LSM_BIN_DIR overrides ~/.local/bin)
set -euo pipefail
: "${HOME:?HOME must be set}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BIN_DIR="${LSM_BIN_DIR:-$HOME/.local/bin}"

# The graph visualizer ships as assets compiled INTO the binary, so whether it exists at all is decided here at build time, not later by a config flag. Off by default because embedding it needs node and npm and adds an npm ci plus a vite build to a cold build; a binary without it still runs everything else, and `logan-spine-mcp --ui=true` on such a binary refuses with its own message rather than half-working. Turn it on per install, then toggle the listener itself with plugins/logan-spine/scripts/visualizer.sh.
WITH_UI="${LSM_WITH_UI:-}"
for arg in "$@"; do
  case "$arg" in
    --with-ui) WITH_UI=1 ;;
    *) echo "install.sh: unknown option '$arg' (only --with-ui is accepted)" >&2; exit 2 ;;
  esac
done

# Prefer the main checkout, because a worktree is deleted at merge and a marketplace pinned to a path that no longer exists stops resolving. But only if it actually holds the marketplace file: while this work is on a branch, it does not.
COMMON_GIT="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)"
MAIN_CHECKOUT="$(dirname "$COMMON_GIT")"
if [ -f "$MAIN_CHECKOUT/.claude-plugin/marketplace.json" ]; then
  MARKET="$MAIN_CHECKOUT"
else
  MARKET="$ROOT"
  echo "note: $MAIN_CHECKOUT has no .claude-plugin/marketplace.json, registering $ROOT instead. Re-run this script from the main checkout once this branch is merged." >&2
fi

echo "[1/6] build (cold ≈10 min without ccache)"
if [ -n "$WITH_UI" ]; then
  echo "  with the graph visualizer embedded (needs node and npm)"
  "$ROOT/spine/scripts/build.sh" --with-ui --version "$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)"
else
  echo "  without the graph visualizer; re-run with --with-ui to embed it"
  "$ROOT/spine/scripts/build.sh" --version "$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)"
fi

echo "[2/6] binary -> $BIN_DIR/logan-spine-mcp"
mkdir -p "$BIN_DIR"
# Publish through the engine's own install path rather than a bare copy-and-rename.
#
# The engine runs ONE daemon per OS account and refuses any process whose build fingerprint differs from the resident daemon's ("crash-safe exact-build admission", spine/src/daemon/version_cohort.h; the comparison is spine/src/daemon/service.c). A plain file swap leaves that daemon running from its already-open inode, so every NEW session on the machine is refused until the last client of the old build disconnects. Measured 2026-08-23: that lasted about sixteen hours and wrote 63 records into ~/.cache/logan-spine-mcp/logs/daemon-conflicts.ndjson. Reproduced deterministically in 30 seconds: hold a live client on one build, invoke another, and the second is refused.
#
# `install` coordinates instead of colliding: it asks the resident daemon to drain its sessions, takes the version-cohort mutation lock so no new admission races the swap, stages the candidate out of line, verifies it by running --version on it, and publishes atomically.
#
# --skip-config is what makes this safe for version 02. Without it the engine recreates the whole version-01 global footprint that unregister-global.sh exists to remove: three agent files under ~/.claude/agents/, the mcpServers entry in ~/.claude.json, and the PreToolUse, PostToolUse, SessionStart and SubagentStart hooks. Verified by diffing two --dry-run runs 2026-08-24: those eleven lines are present without the flag and absent with it, leaving "Would install binary" as the only action. --clients needs a value even so, because the parser rejects both an empty list and a "none" token.
#
# Two things this has to work around, both hit on 2026-08-24 with a 002 umask.
#
# 1. The engine refuses to publish OUT OF a directory that is group- or world-writable, and refuses to publish INTO one, and refuses when the file it is replacing is group-writable (activation_transaction.c: `(st_mode & 0022) != 0` on the install dir, the source dir, and the target entry). A 002 umask makes every directory 0775, so the repo's own build/c is refused as a source and `~/.local/bin` as a destination. The check is right — anything group-writable lets another account swap the executable between validation and exec — so this satisfies it rather than bypassing it.
# 2. Staging through a private `mktemp -d` (0700 by default) is what fixes the source side without fighting the machine's umask or chmod-ing the repo's build tree.
STAGE="$(mktemp -d)"
chmod 700 "$STAGE"
cp "$ROOT/spine/build/c/logan-spine-mcp" "$STAGE/logan-spine-mcp"
chmod 755 "$STAGE/logan-spine-mcp"
# The destination side: tighten only what the engine requires, and say so, because silently changing permissions on a directory outside the repo would be worse than the failure it prevents.
if [ -d "$BIN_DIR" ] && [ "$(( $(stat -c '%a' "$BIN_DIR" 2>/dev/null || echo 0) & 22 ))" -ne 0 ]; then
  echo "  note: removing group/other write from $BIN_DIR (the engine refuses to publish into a shared-writable directory)"
  chmod go-w "$BIN_DIR"
fi
if [ -f "$BIN_DIR/logan-spine-mcp" ] && [ "$(( $(stat -c '%a' "$BIN_DIR/logan-spine-mcp" 2>/dev/null || echo 0) & 22 ))" -ne 0 ]; then
  echo "  note: removing group/other write from the existing $BIN_DIR/logan-spine-mcp for the same reason"
  chmod go-w "$BIN_DIR/logan-spine-mcp"
fi
"$STAGE/logan-spine-mcp" install \
  --force --skip-config --clients=claude --dir="$BIN_DIR" -y
rm -rf "$STAGE"

echo "[3/6] PATH"
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

echo "[4/6] auto-index on"
# Non-fatal on purpose. Under `set -e` a failure here would abort the script before step 5, leaving the binary installed but the marketplace unregistered — and a repository whose .claude/settings.json already commits enabledPlugins is then silently inert, with nothing on screen explaining why. auto_index is read fresh by the daemon at the start of each session (spine/src/daemon/application.c), not cached at install time, so setting it later costs nothing.
if ! "$BIN_DIR/logan-spine-mcp" config set auto_index true; then
  echo "  warning: could not set auto_index; set it later with: logan-spine-mcp config set auto_index true" >&2
fi

echo "[5/6] warm daemon"
# Without this the plugin's hooks are silent on a fresh machine, which looks like the hooks being broken.
#
# A hook is contractually fail-open and time-bounded: it CONNECTS to a daemon but never spawns one, giving up after 250 ms (MAIN_HOOK_CONNECT_TIMEOUT_MS, spine/src/main.c). The session's own MCP server does start a daemon, but measured 2026-08-24 it takes about 6.3 s to come up — 25x the hook's budget — so the SessionStart hook of a fresh session fires and gives up long before its own MCP server is ready. Measured both ways: with no daemon the hook prints nothing, and with one warm the same script on the same payload returns the graph context immediately.
#
# `daemon start` creates a PERMANENT daemon that survives idle periods and session ends, so this is a once-per-machine step rather than something a session has to win a race against. Non-fatal: a machine without one still works, its hooks are just quiet until an MCP session warms it.
if ! "$BIN_DIR/logan-spine-mcp" daemon start; then
  echo "  warning: could not start the daemon; hooks stay quiet until an MCP session warms one. Retry with: logan-spine-mcp daemon start" >&2
fi

echo "[6/6] marketplace -> $MARKET"
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
