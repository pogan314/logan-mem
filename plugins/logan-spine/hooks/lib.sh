# Shared by every script this plugin ships. Sourced, never executed.
#
# Resolve the engine binary. Print its absolute path and return 0, or print nothing and return 1.
#
# An explicitly set LOGAN_SPINE_BIN is authoritative: if it is set and not executable, that is an error, not a reason to look elsewhere. Falling through to a different binary than the one the operator named would make the override untestable and its failures invisible.
lsm_bin() {
  if [ -n "${LOGAN_SPINE_BIN:-}" ]; then
    [ -x "$LOGAN_SPINE_BIN" ] || return 1
    printf '%s\n' "$LOGAN_SPINE_BIN"
    return 0
  fi
  if [ -n "${HOME:-}" ] && [ -x "${HOME}/.local/bin/logan-spine-mcp" ]; then
    printf '%s\n' "${HOME}/.local/bin/logan-spine-mcp"
    return 0
  fi
  command -v logan-spine-mcp 2>/dev/null && return 0
  return 1
}
