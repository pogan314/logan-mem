---
title: Storage and sharing — where memories live and who sees them
type: ideation
status: ideation
created: "2026-08-21 13:00 CDT"
updated: "2026-08-21 13:00 CDT"
version: "01"
sources: [owner-requirements.md #7 #11 #12 #13, what-went-wrong.md, findings/what-was-built.md]
---

# Storage and sharing

- The old system had three stores (`personal`, `org`, `shared`) with three different write rules, per-person folders enforced by git hooks, three GitHub repos, and encryption for a fourth case. The owner could not tell two of them apart. Collapse it.

## Two places, one rule each (idea)

| Place | What goes there | Who sees it | How it is shared |
|---|---|---|---|
| **In the repo** — a committed folder, e.g. `.logan-mem/` at the repo root | Anything about *this* codebase: decisions, gotchas, conventions, fixes, status, the spine | Everyone who clones the repo, on the branch they are on | `git clone` / `git pull`. Nothing extra. Branches give per-branch memory for free |
| **User-level** — one folder in the home directory, e.g. `~/.logan-mem/` | Anything about *the person* or spanning repos: preferences, environment facts, recipes for machine setup, cross-project lessons | That person's machines | However the owner already syncs machines (Mutagen). Git only as a backup if wanted |

- Team-wide rules are just in-repo memories with `scope: team` in a repo the team shares (or a tiny dedicated "team memory" repo if a rule truly spans every repo). No second approval. `human_verified: true` is the signal that a human stood behind it.
- Attribution is the `author` field plus the git commit author. No per-person folders.

## Why "in the repo" answers three owner concerns at once

- Per-repo memories exist as a first-class thing (concern #7).
- Sharing needs no new machinery — whoever works on the repo has the memories (concern #12).
- Client work: the memories sit inside the client's repo, which is already private to the people allowed to see it. No separate encrypted store, no missing-key gaps in search results (concern #13).

## Things to decide later (not now)

- Folder name. Must not collide with anything common. `.logan-mem/` is the placeholder.
- Whether the in-repo folder is committed on every branch or only on long-lived ones. Default: every branch; it is text.
- Whether a search index (SQLite) is built per repo or one global index over all known repos. Default: one global index, rebuilt from files, disposable.
- Whether a "team memory" repo exists at all in 01. Default: no; see if the need appears.
