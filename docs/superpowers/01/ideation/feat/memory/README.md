---
title: feat/memory — ideas about what a memory is and how it moves
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner-requirements.md, what-went-wrong.md, docs/wiki/seed-repos/, docs/wiki/retrieval-methods-primer.md]
---

# feat/memory

- Scratchpad for the core: what a memory is, which kinds exist, how one gets written without a human, where it lives, and how an agent finds it again.
- Files: `memory-kinds.md` (what kinds to capture), `capture-paths.md` (how memories get written automatically), `storage-and-sharing.md` (where they live, who sees them), `verified-flag.md` (human review as a flag).

## The shape of one memory (idea, not schema)

- A markdown file. YAML frontmatter on top, plain prose body.
- Frontmatter ideas: `id`, `kind`, `title`, `created`, `scope` (repo / user / team), `repo` (if repo-scoped), `author` (who or what wrote it: a person's handle, or `agent`), `human_verified` (true / false), `status` (active / superseded / deactivated), `supersedes` (id), `anchors` (files, symbols, or topics it is about).
- Body: the lesson, in the words an agent would want to read next time. Two to ten sentences. "What happened, what to do instead, how we know."
- Nothing else in 01. Every field the old system had that nobody filled (`born_in.effort: unknown`) is a warning.

## The loop, in one picture

```
session runs ──► something worth keeping happens
                      │
                      ▼
        automatic capture (no human)  ──► memory file written, live immediately
                      │
                      ▼
        next session starts ──► relevant memories injected + searchable on demand
                      │
                      ▼
        owner glances weekly ──► flips human_verified on the good ones
```

- The old build had every box in this picture except the arrow from "capture" to "live immediately". That missing arrow is the whole failure.
