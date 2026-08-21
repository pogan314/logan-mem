---
title: Wiki — what goes here
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
sources: [owner instruction 2026-08-21]
---

# docs/wiki/

- This folder holds **research facts about things we do not control**: other people's repos, Claude Code's own features, how retrieval methods work.
- Test for whether something belongs here: if we re-ran the research, would we get the same answer? If yes, it is a wiki fact. If it depends on what *we* decide, it belongs in `docs/superpowers/<nn>/`.
- Every file carries frontmatter with an `updated` date. A wiki fact is true **as of that date**. Star counts, issue counts, and "last pushed" dates drift; re-verify before relying on them.
- Technical detail is allowed here where the research needs it. Plain English still wins.

## Files

| File | What it covers |
|---|---|
| `glossary.md` | Plain-English definitions of every term of art used in this repo |
| `memory-systems-survey.md` | One table of every open-source memory system studied, with live star counts |
| `seed-repos/` | Deep profiles of the repos the owner originally pointed at (ECC, codebase-memory-mcp, claude-mem), the ones added later (LangMem/LangGraph), and supermemory — with every cited file path re-checked live |
| `obra-episodic-memory.md` | Live profile of `obra/episodic-memory` — the closest existing thing to what the owner wants |
| `claude-code-harness-facts.md` | What Claude Code actually provides — hook events, agent hooks, plugin anatomy — verified against the live docs |
| `retrieval-methods-primer.md` | How keyword, vector, hybrid, graph, and rerank retrieval work, and what each is bad at |
| `research-extracts/` | Raw compiled extracts of the old project's repo research (rounds 1–9 and its top-level docs), preserved so the survey and seed-repo files stay traceable. Evidence, not conclusions |
