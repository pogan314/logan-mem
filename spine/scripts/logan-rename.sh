#!/usr/bin/env bash
# Renames upstream codebase-memory-mcp identifiers to logan-spine-mcp across this tree.
# Idempotent: running it on an already-renamed tree changes nothing.
# Run it on a fresh upstream checkout BEFORE `git subtree pull` so the rename never conflicts.
# Usage: scripts/logan-rename.sh [tree-root]   (default: the directory containing scripts/)
set -euo pipefail
root="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$root"

# Files this script must never edit (they legitimately contain the old names).
skip='^(scripts/logan-rename\.sh|LOGAN-CHANGES\.md)$'

# 1. Content. Order matters only in that every rule is applied in one perl pass per file.
#    The arXiv paper title "Codebase-Memory: ..." is a citation and is left alone (no rule matches it).
rules='
s/codebase-memory/logan-spine/g;
s/codebase_memory/logan_spine/g;
s/CodebaseMemory/LoganSpine/g;
s/Codebase Memory/Logan Spine/g;
s/codebase memory/logan spine/g;
s/\bCBM_/LSM_/g;
s/\bcbm_/lsm_/g;
s/\bCBM\b/LSM/g;
s/\bcbm\b/lsm/g;
s/\bCbm/Lsm/g;
s/\bCBM(?=[A-Z0-9])/LSM/g;
s/\bcbm(?=[a-z0-9])/lsm/g;
s/cbmignore/lsmignore/g;
'
git ls-files -z \
  | grep -zvE "$skip" \
  | xargs -0 grep -lIE 'codebase.memory|Codebase.Memory|CodebaseMemory|\b[Cc][Bb][Mm]\b|\bCBM_|\bcbm_|\bCbm|\b[Cc][Bb][Mm]\w|cbmignore' 2>/dev/null \
  | { while IFS= read -r f; do perl -pi -e "$rules" "$f"; done; } || true

# 2. Paths. Deepest first so a renamed parent does not invalidate a child path.
rename_path() {
  printf '%s' "$1" | perl -pe 's{([^/]*)$}{ my $b=$1; $b =~ s/codebase-memory/logan-spine/g; $b =~ s/codebase_memory/logan_spine/g; $b =~ s/CodebaseMemory/LoganSpine/g; $b =~ s/\bcbm\b/lsm/g; $b =~ s/cbmignore/lsmignore/g; $b }e'
}
git ls-files -z | tr '\0' '\n' | perl -ne 'chomp; my @p=split m{/}; for my $i (0..$#p){ print join("/",@p[0..$i]),"\n" }' \
  | sort -u | awk -F/ '{print NF"\t"$0}' | sort -rn | cut -f2- \
  | while IFS= read -r p; do
      [ -e "$p" ] || continue
      n="$(rename_path "$p")"
      [ "$n" = "$p" ] && continue
      mkdir -p "$(dirname "$n")"
      if git ls-files --error-unmatch "$p" >/dev/null 2>&1 || [ -d "$p" ]; then git mv -k "$p" "$n" 2>/dev/null || mv "$p" "$n"; fi
    done
echo "logan-rename: done"
