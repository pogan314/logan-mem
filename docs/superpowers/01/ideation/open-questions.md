---
title: Open questions — things the owner still needs to decide
type: ideation
status: ideation
created: "2026-08-21 13:14 CDT"
updated: "2026-08-21 13:14 CDT"
version: "01"
sources: [owner messages 2026-08-21, what-went-wrong.md, starting-recommendations.md]
---

# Open questions

- Each row is a decision the owner has not made yet, with the options as I see them and a leaning. The leaning is an opinion.
- Questions the owner already answered on 2026-08-21 are listed at the bottom so nobody re-asks them.

## Still open

| # | Question | Options | Leaning |
|---|---|---|---|
| 1 | Where does the version 01 line budget sit? | ~3,000 / ~4,000 / ~6,000 / ~8,000 source lines | ~4,000. Big enough for a real loop, too small for a doctor module |
| 2 | Adopt `obra/episodic-memory` as-is for conversation search, or build our own thin version? | adopt plugin as-is / fork / build our own / skip in 01 | Adopt as-is in 01, revisit when our own store exists. It has 0 commits in 90 days and 58 open issues, so "adopt" means "use, watch, do not depend on fixes" |
| 3 | Where do per-repo memories live? | a committed folder in the repo / a user-level store keyed by repo / both | Committed folder in the repo, so clone = share and branches = scope. Needs a name that will not collide (`.logan-mem/`?) |
| 4 | Where do cross-repo and personal memories live? | one user-level folder / the old three-store split / GitHub repo per person | One user-level folder, synced however the owner already syncs (Mutagen), with git only as backup |
| 5 | Which hook writes lessons at the end of a session? | `Stop` agent hook / `Stop` prompt hook / `SessionEnd` command hook that shells out to `claude -p` | `Stop` agent hook first (it has tool access); command-hook fallback if the experimental status bites |
| 6 | What is the starter list of memory kinds? | see `feat/memory/memory-kinds.md` | Start with ~8, as a plain string, grow freely |
| 7 | Does a coordinator's in-run learning (concern #5 in owner-requirements) write to the same store as durable memories, or a run-scoped scratch file? | same store / run-scoped / both (run-scoped, promoted at end) | Run-scoped during the run; a Stop-time pass decides what becomes durable |
| 8 | How does a human mark a memory verified? | edit the YAML field by hand / a one-line CLI / an MCP tool the agent calls when the owner says "yes, keep that" | All three eventually; the MCP tool first, because the owner says it in chat |
| 9 | Spine in version 01 or 02? | 01 / 02 | 02, unless it can be done in under ~500 lines. It is the feature most likely to balloon |
| 10 | Does the spine's committed file live at the repo root or under the memory folder? | `.logan-mem/spine.md` / `SPINE.md` / `docs/spine/` | Inside the memory folder, so one folder holds everything the system owns in a repo |
| 11 | Cross-tool (Codex, Cursor, Gemini) in 01? | yes / no / store-format only | Store-format only: plain files any tool could read. Hooks are Claude Code only in 01 |
| 12 | What replaces the two-person approval for team rules? | nothing (a rule is a memory with `scope: team`) / a single-reviewer flag / keep PRs but one approver | Nothing. A team-wide rule is a memory with a scope field and `human_verified: true` |
| 13 | Should the old 29 inbox drafts be imported as seed memories? | yes / no / read once for ideas then discard | Read once to learn what good capture looks like; do not import (different schema, different repo) |
| 14 | Repo visibility on GitHub | private / public | Private until the owner says otherwise |

## Already answered by the owner on 2026-08-21 (do not re-ask)

| Question | Owner's answer |
|---|---|
| Rebuild or refactor? | Rebuild, new repo (`logan-mem`), old repo is reference only |
| One version or versions? | Versions. 01 is still substantial |
| Approval gate in front of memories? | No. Every memory is automatic. Human review is a YAML flag that adds weight |
| Two-person approval on shared rules? | Not necessary |
| Per-person folders? | Not needed; git author or a field can attribute |
| Shared vs org stores? | Owner sees no real difference; collapse them |
| Encryption of memories? | Owner does not hate it but sees no value; memories never leave internal use. Treat as dropped unless a reason appears |
| Spine as a cache? | No. Spine lives in the repo branch with notes and descriptions, as its own subsystem |
| Was the build-failure memory kind bad? | No. The problem was that it was the **only** kind written |
| Is `obra/episodic-memory` the right idea? | Owner is interested; packaging (separate plugin vs built in) is open — see #2 above |
| Single plugin install? | Yes, non-negotiable |
