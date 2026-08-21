---
title: Glossary
type: wiki
status: research-fact
created: "2026-08-21 13:15 CDT"
updated: "2026-08-21 13:15 CDT"
sources: [live Claude Code docs via claude-code-docs MCP (code.claude.com/docs/en/glossary, /en/hooks.mdx, /en/skills.mdx, /en/plugins-reference.mdx) verified 2026-08-21, general knowledge of established database/IR/CS terms, /home/ubuntu/projects/org/logan-mem/CLAUDE.md for this repo's own usage of "memory" and "spine"]
---

# Glossary

Plain-English definitions for terms this repo's docs use. Alphabetical.

| Term | Plain-English meaning |
|---|---|
| Agent hook | A hook of type `"agent"` — instead of running a shell command, it spawns a full agent with its own tools to verify or judge something too complex for a script. |
| BM25 | The ranking formula full-text search uses to score how well a document matches a query — words that appear often in that document but rarely elsewhere count for more. |
| Claude Code | Anthropic's command-line coding agent — it reads your files, runs shell commands, and edits code inside your project directory. |
| CLAUDE.md | A markdown file of persistent instructions for Claude, loaded fresh into every session as a message right after the system prompt. |
| Command hook | A hook of type `"command"` — the most common kind — that runs a shell command or script when its event fires. |
| Context window | The model's working memory for a session: everything it can currently "see" at once (conversation history, file contents, instructions). It fills up as you work, and old content gets summarized once it's full. |
| Cross-encoder | A reranker model that reads the query and one candidate result together and scores how well they match — more accurate than comparing two separate embeddings, and much slower, so it is only run on the top few results. |
| Embedding | A list of numbers a model produces from a piece of text, positioned so texts with similar meaning end up numerically close together — the basis of vector search. |
| Frontmatter | A block of YAML at the top of a file, between `---` markers, that carries metadata about the file (like a title or status) separate from its main content. |
| FTS5 | SQLite's built-in full-text search engine — it indexes the words in a table's text so a keyword search doesn't have to scan every row. |
| Hook | A handler that runs automatically at a fixed point in Claude Code's lifecycle (before a tool runs, after a file edit, at session start) — it fires by rule, not by the model's choice. |
| JSONL | "JSON Lines" — a text file format where each line is its own complete JSON object, so a file can be appended to one line at a time without parsing the whole thing. |
| MCP | Model Context Protocol — an open standard that lets Claude Code connect to external services (a database, Slack, a custom server) as new tools it can call. |
| MCP server | A program that speaks MCP and exposes tools, prompts, or resources to Claude Code — it can run locally as a process or be reached over the network. |
| MCP tool | One specific action an MCP server offers, named `mcp__<server>__<tool>` — like a single function Claude Code can call. |
| Memory (in this repo) | One stored fact, decision, or lesson an agent can search for and retrieve later. In this repo the word names the general idea, not any one specific built system. |
| MRR (mean reciprocal rank) | A search-quality score: for each test query, 1 divided by the position of the first correct result (1st place = 1, 2nd = 0.5, 3rd = 0.33), averaged over all queries. Higher is better. |
| Mutagen | A file-synchronisation tool that keeps a folder identical across two machines in real time. The owner uses it to mirror the `projects` folder between a Windows PC and the EC2 box. |
| ONNX | A standard file format for saving a trained machine-learning model so any runtime can load it. When a doc says "the ONNX weights", it means the downloaded model file an embedder or reranker needs before it can run. |
| Plugin | A bundle of skills, hooks, subagents, and MCP servers packaged together as one installable unit. |
| Progressive disclosure | Returning short summaries first from a search, and fetching the full text of one specific result only when it's actually needed — saves context tokens. |
| Prompt hook | A hook of type `"prompt"` — instead of running code, it hands the event to an LLM and asks it to decide whether to allow or block the action. |
| Recall@5 | A search-quality score: of the results a query *should* have returned, what fraction appeared in the top 5? 1.0 means everything relevant was in the top five. |
| Reranker | A second, usually slower model that re-sorts a short list of already-found search results by how well each one actually matches the query. |
| RRF | Reciprocal rank fusion — a way to merge two separately-ranked lists of search results into one list, by combining how well each item ranked in each list. |
| SessionStart / SessionEnd / Stop | Three hook events. SessionStart fires when a session begins or resumes. SessionEnd fires when a session terminates. Stop fires at the end of each turn, when Claude finishes responding. |
| Skill | A `SKILL.md` file of instructions, knowledge, or a workflow that Claude can load automatically when relevant, or that you invoke directly with `/name`. |
| Spine (in this repo) | The idea of a map of a codebase — its files, its functions, and what calls what — kept as structured data instead of prose, so an agent can look up "what touches this function" without re-reading every file. |
| SQLite | A small, file-based database engine — the whole database lives in one file on disk, with no separate server process to run. |
| Subagent | A specialized AI assistant that runs in its own separate context window with its own tools and permissions, works on one delegated task, and reports a summary back to whoever spawned it. |
| Transcript | The JSONL file recording everything that happened in a session — every message, tool call, and result — saved under `~/.claude/projects/`. |
| Transformers.js | A JavaScript library that runs machine-learning models (including embedding models) locally inside Node.js or a browser, with no API call. It is how `obra/episodic-memory` and the old build compute embeddings. |
| Tree-sitter | A parsing library that turns source code into a structured tree of its actual syntax (functions, classes, calls), so a tool can reason about code precisely instead of just matching text. |
| ULID | "Universally Unique Lexicographically sortable IDentifier" — an ID like a UUID, but one that sorts in the same order it was created, so an alphabetical sort is also a chronological sort. |
| Vector search | Searching by meaning instead of by exact words: embed the query, then find which stored embeddings are numerically closest to it. |
| YAML | A human-readable text format for structured data (lists, key-value pairs) — commonly used for config files and for frontmatter blocks at the top of a Markdown file. |
