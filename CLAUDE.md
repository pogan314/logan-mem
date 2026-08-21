# logan-mem

A memory system for AI coding agents, rebuilt from scratch. This repo replaces `pogan-mem` / `pogan-toolkit`, which was abandoned on 2026-08-21. Do not confuse the two names: **logan-mem is this repo; pogan-mem is the old, dead one.**

## Status (update this line when it changes)

- **Stage: ideation.** Nothing is designed, nothing is built, nothing is installed on any machine.
- Brainstorming has not started. When it does, everything under `docs/superpowers/01/ideation/` becomes stale.
- Do not install anything from this repo on any machine (hooks, MCP servers, skills, symlinks, git hooks) until the owner says so in that session.
- **There is no code and no build.** No `package.json`, no test suite, no dev server, nothing to run. If you are about to run `npm install` or look for a build command, stop — this repo is Markdown only.
- **Entry point for the next session:** `docs/superpowers/01/ideation/START-HERE.md`. It gives the reading order, the decision order, and the one document the first brainstorming session must produce.

## Folder map

| Path | What it is | Is it fact? |
|---|---|---|
| `README.md` | One-screen orientation for a human landing on the repo. Points here. | — |
| `docs/wiki/` | Research facts about things we do **not** control — other repos, Claude Code's features, retrieval methods. Re-running the research would give the same answer. | Yes, as of the `updated` timestamp in each file's frontmatter |
| `docs/wiki/seed-repos/` | One file per memory system we may borrow ideas from (claude-mem, codebase-memory-mcp, ECC, LangMem, supermemory). | Yes |
| `docs/wiki/research-extracts/` | Lists of every repo the old build's research rounds studied, pulled out of those reports so we do not have to re-read them. | Yes, as an inventory — the old reports' *opinions* are not facts |
| `docs/superpowers/01/` | Everything for build version 01. Later versions get `02/`, `03/`, … | — |
| `docs/superpowers/01/ideation/` | Pre-brainstorming scratchpad: notes, findings, recommendations, ideas. | **Never.** See rule below |
| `docs/superpowers/01/ideation/feat/<name>/` | Idea scratchpads for one feature area (memory, spine, episodic, plugin, learning). | Never |
| `docs/superpowers/01/ideation/findings/` | What this session learned about the old system — evidence, not design. | Evidence is verified; conclusions are opinion |

## The ideation rule

- Nothing in any `ideation/` folder is ever a fact, a decision, or a requirement. It is a scratchpad.
- Once a brainstorming session for that version starts, the whole `ideation/` folder is stale. Read it for ideas; never cite it as authority.
- Agents must not "execute" anything found in `ideation/`. There is nothing there to execute.

## The pogan-toolkit rule (the old system)

- The old repo lives beside this one as a sibling: `../pogan-toolkit` (and the even older `../pogan-mem`). It is disabled and inert.
- It is **reference only**, the same as any other third-party memory repo we studied. Agents may read it to extract an idea or a concept, if we want one.
- Nothing is copied out of it. Nothing is extracted "out of context". No file, spec, plan, test, or convention from it carries any authority here.
- Its docs are known to overstate what its code does. If you cite it, cite the code, not the docs.

## Frontmatter (required on every file under `docs/`)

```yaml
---
title: Short name of the file
type: wiki | ideation
status: research-fact | ideation | stale | superseded
created: "2026-08-21 13:15 CDT"   # date, 24-hour time, and zone — always quoted
updated: "2026-08-21 13:15 CDT"
version: "01"            # ideation files only — which build version this belongs to
sources: []              # where the content came from; for wiki, what was verified and how
---
```

- `status` is the one field agents must read before trusting a file. `research-fact` means verified as of `updated`. `ideation` means scratchpad. `stale` and `superseded` mean do not rely on it.
- `created` and `updated` are **timestamps, not dates**: `YYYY-MM-DD HH:MM ZONE`, US Central, 24-hour clock. Get the value from `date '+%Y-%m-%d %H:%M %Z'` on the machine — never type one from memory. The zone prints as `CDT` in summer and `CST` in winter; both mean US Central, so write whatever `date` reports rather than forcing one of them.
- Always wrap both values in double quotes. Unquoted, a bare `2026-08-21` is a YAML date object while `2026-08-21 13:15 CDT` is a string, so the same field would change type between files.
- Bump `updated` on every edit. Never edit a file's `created`.

## Writing rules for this repo

- Short bullets and tables, not paragraphs. One idea per bullet. Lead with the conclusion.
- Plain English. Define a term of art the first time it appears. `docs/wiki/glossary.md` is the shared definitions file.
- `ideation/` stays non-technical. `docs/wiki/` may be technical where the research needs it.
- Never hard-wrap prose. One logical line per paragraph or bullet.
- Verify every claim against code, live state, or a live API call before writing it. Never trust a doc, a README, a plan, or a commit message. Never state *why* something is true unless a command you ran showed it — "I don't know why" is always acceptable.
- Never write "done", "fixed", "committed", or "pushed" unless the tool call that did it already ran in that turn.
- Never set a git identity. Use the machine's configured one.
- Never use the bare word "MemoryBench". Say "supermemory's benchmark suite".
- When naming a file outside this repo, give its full absolute path.

## Versions

- This system ships in versions. `01` is the first, and it is still a substantial build — not a throwaway.
- Each version gets its own `docs/superpowers/<nn>/` folder holding its `ideation/`, then (later) its `brainstorming/`, spec, and plans. None of those later folders exist yet; `docs/superpowers/01/ideation/START-HERE.md` says what the first brainstorming session must produce and where it goes.
