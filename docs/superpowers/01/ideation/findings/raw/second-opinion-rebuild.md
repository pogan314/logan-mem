---
title: Raw — independent rebuild-vs-refactor analysis
type: ideation
status: ideation
created: "2026-08-21 13:14 CDT"
updated: "2026-08-21 13:14 CDT"
version: "01"
sources: [../pogan-toolkit/src and github.com/obra/episodic-memory, analysed 2026-08-21 by an agent with no prior context]
---

> Raw analysis, preserved verbatim from the 2026-08-21 session so the condensed docs that cite it stay traceable. Written by an agent with no prior context, before the owner set this repo's rules. **Read with these corrections in mind:**
>
> - It calls the new system "pogan-mem v2" and proposes `.pogan/` folders. The new system is **logan-mem**; the placeholder folder name is `.logan-mem/`. See `CLAUDE.md`.
> - It proposes copying ~1,900 lines of specific files out of the old repo. `CLAUDE.md` overrides this: nothing is copied out of the old repo. Treat those as "mechanisms worth knowing about", not a salvage list.
> - Its numbered design sketch and its "hard 4,000-line budget / no spec document" lines are **one reviewer's opinion**, not a plan or a decision. The owner has decided nothing about the budget (see `../../open-questions.md` #1).
> - Its closing figures ("five memories", "four days") are wrong: the live store holds **6** filed records and capture fired on **2** days. See `../drift-analysis.md` and `../live-store-evidence.md`.
> - It is paragraph-heavy, against this repo's style; it was not rewritten because it is a record.

# Second opinion: refactor pogan-toolkit, or rebuild from scratch?

Independent review, 2026-08-21. No prior context on the project. Everything below was checked against the code, the live store, live GitHub API calls, or the live Claude Code docs. Where I am guessing, I say so.

## Recommendation

**REBUILD.** Start a new repo. Keep the old one checked out as a reference during planning, copy roughly six named files across, and delete nothing from the old repo — leave it as a read-only archive.

This is not a judgement that the code is bad. Large parts of it are competent. It is a judgement that the thing the owner now wants is a different system, that the existing system's shape actively fights that want, and that a refactor of this specific codebase is the highest-risk way to get there given the owner's stated history with agent-driven refactors.

## What is actually in the repo

Verified counts (my own commands):

| Thing | Count |
| --- | --- |
| Source files (non-test) | 97 |
| Source lines (non-test) | 33,945 |
| Of which comment lines | 8,982 (26%) |
| Test files / test lines | 97 / 46,391 |
| Markdown lines in repo | 26,584 |
| Plan + spec doc lines | 18,096 |
| CLI subcommands registered | 40 `register*` calls in `src/cli/main.ts` |
| Commits / span | 490 over 2026-07-31 → 2026-08-21 |
| Records ever filed to the live store | 5 (verified on disk) |
| Unreviewed drafts in the inbox | 28 |
| Events in the log | 110, spanning 2026-08-08 → 2026-08-12 |

Two numbers matter more than the rest.

**First: 14,000 of the 33,945 source lines (41%) are machinery a single user does not need.** I summed the non-test line counts of the crypto, multi-host shred, SSH, GitHub-PR, git-gate, registry, identity, manifest, ladder, gate, knobs, eval-rig, scale-harness, doctor, weekly-report, and replay modules plus the `key`/`shred`/`member`/`publish`/`ratify`/`encryptProject`/`restore`/`host`/`upgrade`/`eval` commands and `gateCheck`. That is 14,000 lines. `src/core/doctorChecks.ts` alone is 2,856 lines — a health checker for a system that has stored five records.

**Second: 88 of the 97 source files cite spec sections in their comments** — 1,402 `§N` references, plus 36 distinct plan-task IDs like `P4.T13`. The code is not merely documented; it is bound to an 18,096-line planning corpus. I return to this under concern 6, because it is the single most important fact in this decision.

## Is the code salvageable? Yes in pieces, no as a system

A subagent built the import graph from the actual relative imports. Highest fan-in: `src/core/config.ts` (38 importers), `src/core/events.ts` (35), `src/core/index/build.ts` (33), `src/core/record.ts` (30). I separately confirmed 21 files import the org registry and 37 import `config.ts`.

**The storage layer does not have a clean seam.** SQLite opens at `src/core/index/build.ts:52`; the schema is at `build.ts:107-137` (`records`, an FTS5 mirror, `links`, `counters`, `meta`, `pending_embed`, `bad_file_ids`, a `vec0` table at 384 dimensions, `contradiction_candidates`). But `build.ts` imports `../ladder.js` and calls `recordContradictionCandidatesAtWrite` on every single write (`build.ts:11`, `build.ts:616`). Org-triage business logic runs inside the storage write path. Worse, the tuning constants that `src/core/knobs.ts` re-exports are *defined inside* `build.ts:369-550` — configuration lives in the storage module and is imported backwards.

**The search layer does not have a clean seam either.** `src/core/gate.ts:36` imports injection-budget bookkeeping from `session.js`, and `gate.ts:40` imports the code-spine database. Retrieval gating is wired to both session accounting and an unrelated code-navigation cache.

**A note on encryption**, because the package name misleads: the dependency is `better-sqlite3-multiple-ciphers`, but a grep for cipher pragmas found none. Actual encryption shells out to the `age` CLI (`src/core/crypto.ts:65-75`). Owned/client records skip the on-disk DB entirely and build an in-memory index at session time (`src/core/index/clientIndex.ts:1-9`).

**Genuinely good and worth carrying over**, verified by reading them:

- `src/core/index/query.ts` (897 lines) — reciprocal-rank fusion of full-text and vector hits, graph expansion over `links` with hop decay, a rerank window, per-layer weights. This is real information-retrieval work and it is the best thing in the repo.
- `src/core/embed.ts` (382 lines) — a self-contained wrapper over `@huggingface/transformers`, model `Xenova/bge-small-en-v1.5` (384-dim) plus `Xenova/bge-reranker-base`. Runs locally, no API key.
- `src/core/spine/parse.ts` (263 lines) — the tree-sitter walker for TypeScript and Python.
- `src/core/record.ts` (234 lines) — the zod schema. Good bones, but see concern 4.
- `src/core/crypto.ts` (76 lines), `src/core/ids.ts` (26 lines), `src/core/runProcess.ts` — clean leaf utilities with no coupling to memory concepts.

That is roughly 1,900 lines worth keeping out of 33,945. Call it 6%.

**Dead weight**: `doctorChecks.ts` (2,856 lines, only meaningful as a whole-system auditor of a system you are replacing), `ladder.ts` (org/member rules disguised as a matching module — `ladder.ts:93-99` filters `WHERE records.world = 'org'` and assigns cross-member ownership at `ladder.ts:119-122`), the 589-line god-function inside `src/plugin/mcp/writeTools.ts:565-1154` where four tool handlers each inline schema validation, business rules, a git commit, and an index write, and the entire key/shred/ratify/publish column.

## The six concerns

### 1. It must learn automatically

**The owner's diagnosis is close but not exact, and the truth is worse than his version.**

A subagent grepped for `SKILL.md`, `skills/`, `.claude/agents`, and skill-generation logic across `src/core`, `src/cli`, and `src/plugin`. **There is no code anywhere that turns a memory into a skill, hook, or agent.** Every hit is about installing pogan's own plugin. So the conflation the owner remembers is not present in the code as he describes it.

What *is* there is a promotion ladder between storage tiers — personal → member/project → org → `shared/` — and a record stays a Markdown file the whole way. But the top of that ladder is startling: `pogan ratify` (`src/cli/commands/ratify.ts:546-684`) clones a `pogan-shared` GitHub repo into a throwaway directory, writes the record there, and **opens a pull request** (`sharedRepoPr`, `ratify.ts:637-663`). A human reviews and merges it on GitHub, then runs `pogan ratify --finish <PR>`. Ratification is also frozen for a solo user — it requires at least two org writers (`ratify.ts:587-589`).

So the owner's instinct is right even if his wording isn't: learning a lesson was made to look like shipping a code change through review.

Now the decisive finding. **There is exactly one capture path that runs without the model choosing to run it.** `src/plugin/hooks/postToolUse.ts:401-453` counts consecutive failing Bash commands and, on the Nth resolving success, calls `writeNudgeDraft` (`src/core/nudgeDraft.ts:112-218`), which mints a `kind: 'failed'` record with `placement.kind: null` — meaning it lands in an inbox, not in the store (`nudgeDraft.ts:169-178`). `src/plugin/sessionEnd.ts:158-193` handles the streak that never resolved.

That is the whole automatic loop: **a Bash-failure-streak counter.** Everything else requires either the model to voluntarily call the `mem_log` MCP tool (`writeTools.ts:568-820` — no hook fires it) or a human to type a CLI command.

This directly explains the owner's complaint that it "only captured build failures and nothing genuinely useful." It captured build failures because build failures are the only thing it was built to notice on its own. That is not a tuning problem. It is the architecture.

Counting the manual steps between "an agent notices something" and "that knowledge is available next session": **one** step for the automatic nudge path (run `pogan inbox`, which is TTY-gated and spawns an editor at `src/cli/commands/capture.ts:863`), and **four** if the lesson is meant to reach the shared tier.

And here is the part that makes rebuild the cheaper path. I checked the live Claude Code hooks documentation, because my memory of the harness is older than the current release. Claude Code today exposes 41 hook events, including `Stop`, `StopFailure`, `PostToolUseFailure`, `PostToolBatch`, `PermissionDenied`, `TaskCompleted`, `SubagentStop`, `PreCompact`, `PostCompact`, and `SessionEnd`. Crucially, thirteen of those — including `Stop`, `PostToolUse`, `PostToolUseFailure`, `SubagentStop`, and `TaskCompleted` — support `type: "prompt"` hooks (an LLM, Haiku by default, evaluates a prompt and returns structured JSON) and `type: "agent"` hooks (a real agent with tool access is spawned).

pogan uses none of this. Its `hooks/hooks.json` registers five hooks and every one is `type: "command"` (verified by grep).

A `Stop` hook of `type: "agent"` that reads the session and writes two or three lessons *is* automatic learning, in about forty lines of JSON and prompt. It needs no model discipline, no inbox, and no human gate. Building that on top of the existing ladder/gate/publish/ratify apparatus means fighting every one of those subsystems. Building it fresh means it is the first thing you write.

### 2. Spines must be committed, not a throwaway cache

**Verified: what exists is the exact opposite of what the owner wants, and the gap is structural.**

A spine is a per-repo SQLite database with five tables — `files`, `symbols`, `edges`, `deps`, `meta` — schema at `src/core/spine/db.ts:20-27`. It is built by `buildSpine()` in `src/core/spine/parse.ts:170-240` using tree-sitter, at file-and-top-level-symbol granularity only (the walker stops at `depth <= 2`, `parse.ts:54-62`). There are no call-graph edges.

Three findings kill the current implementation for the owner's purpose:

1. **It is stored outside the repo.** Path decided at `src/cli/commands/spine.ts:34-36` and again inline at `sessionStart.ts:722`, `sessionAsync.ts:345-347`, `readTools.ts:45` — with two different key schemes for the same artifact. On disk right now: `~/.pogan/cache/spine/%2Fhome%2Fubuntu%2Fprojects%2Forg%2Fpogan-toolkit.db`, 450 KB, in a directory literally named `cache`.
2. **It is wiped and rebuilt wholesale.** `parse.ts:185` opens the insert transaction with `db.exec('DELETE FROM files; DELETE FROM symbols; DELETE FROM edges; DELETE FROM deps;')`.
3. **There is no annotation column.** `db.ts:20-27` is the complete schema. The one field that looks like a description, `files.one_liner`, is scraped automatically from the file's own leading comment (`parse.ts:74-79`) and regenerated every build.

What the owner wants — a per-branch, committed, human-annotatable code map — needs a text format that diffs and merges in git, a stable key so a human note survives regeneration, and a merge step instead of a wipe. SQLite in a cache directory gets none of that. About 260 lines of tree-sitter walker and 250 lines of rendering survive; `freshness.ts`, the 336-line CLI wrapper, and the six integration points that read the DB directly (grep-counted references: `gate.ts` 16, `sessionStart.ts` 14, `readTools.ts` 14, `userPromptSubmit.ts` 10) are rework or bin.

### 3. obra/episodic-memory

**Verified live via `gh api` and a shallow clone.** 469 stars, MIT, TypeScript, default branch `main`. **28 source files, 5,719 lines** in `src/`; 35 test files, 4,525 lines, 504 assertions. Last release `v1.4.2` on 2026-05-21, and the last commit is the same day — **zero commits in three months.** 58 open issues.

What it does, checked in the code rather than the README: reads `~/.claude/projects`, `~/.claude/transcripts`, and `~/.codex/sessions` for `*.jsonl` (`src/paths.ts:44-50`); embeds locally with `Xenova/bge-small-en-v1.5` (`src/embeddings.ts:20-34`), using an asymmetric BGE prefix on queries only (`embeddings.ts:58-74`); stores in SQLite with `sqlite-vec`, table `vec_exchanges USING vec0(id TEXT PRIMARY KEY, embedding FLOAT[384])` (`src/db.ts:126-177`); exposes `search` and `read` as MCP tools (`src/mcp-server.ts:139-181`). Indexing is automatic via a `SessionStart` hook that forks a background sync (`hooks/hooks.json:1-13`, `src/sync-cli.ts:58-66`). It genuinely handles Codex as well as Claude Code, with separate parsers (`src/parser.ts:77`, `:100`, `:329-411`). No API key needed for the index/search path.

Two things follow.

**It uses the identical stack pogan does** — better-sqlite3, sqlite-vec, `@huggingface/transformers`, the same 384-dimension bge-small model, the MCP SDK. That is a striking independent convergence and it means the two systems can share a database file or sit side by side without friction.

**It is a size proof.** 5,719 lines does the entire transcript-indexing job. pogan is 33,945 lines and has never indexed a transcript.

**But do not adopt it wholesale, and do not fork it.** It is stalled, and the open issues include real correctness and safety defects: #152 (sync re-embeds every exchange because a high-water mark was never ported, so it never converges), #139 (a 3 MB payload indexed as a user message, 97% of a 3.04 GB database), #140 (no VACUUM or retention, the index only grows), and #134 — a background summarizer agent resumed a *live* session with full tool access and committed to the user's git repo. Install it as a separate plugin, treat its database as read-only from your side, and keep it at arm's length. That answers concern 3 for roughly a day of work instead of a month, and it gives you a working reference implementation of the `SessionStart`-triggered background index you will want anyway.

### 4. The memory types must be much better

**Verified.** `src/core/record.ts:78`: `kind: z.enum(['decision', 'fact', 'failed', 'recipe'])`. Four, closed.

They are not merely labels — two of them change behaviour. `kind === 'fact'` routes through a compare-and-swap overwrite-in-place instead of a create (`writeTools.ts:695-721`, `capture.ts:242`), and `kind === 'failed'` requires a `source` field and has its body truncated by `capFailureBody` (`writeTools.ts:615`, `writeTools.ts:659`, `record.ts:164`). `decision` and `recipe` carry no behavioural branch I could find.

The problem is not the number four. It is that adding a fifth kind is a schema migration. `record.ts` is 234 lines of which more than a hundred are comments citing spec clauses; a single field, `created`, carries a 25-line comment explaining a normalization decision and its two load-bearing behaviours. The schema is versioned (`schema_version: z.union([z.literal(1), z.literal(2)])`) with a reader-opacity ceiling, and there is a `pogan upgrade` command whose job includes forcing canonicalization rewrites. Adding kinds means touching the schema, the FTS mirror, the write branches, the render budget, the ladder, and the upgrade path.

In a rebuild, kinds are a plain string with a documented starter list and a validation warning rather than a hard enum, so adding one costs nothing.

### 5. Nothing about per-repo memories being shared

**The owner is half right, and the half he is wrong about is the interesting half.**

Sharing does exist, and it is automatic within a member's own machines: `src/plugin/hooks/sessionAsync.ts:263-327` pulls and pushes org-world git repos every session with no per-session manual step. The `shared/` tier is a read-only snapshot, hard-reset from origin hourly (`sessionAsync.ts:164-216`).

What does not exist is the thing he actually means. **Memories are stored in `~/.pogan/`, keyed by member and project — they are never stored in the project repo itself.** So cloning a repo does not bring its memories. Sharing requires the recipient to be enrolled in the same pogan org, holding an `age` key, with a registry entry. That is why he "sees nothing about per-repo memories being shared": the sharing that exists is per-*person*, routed through a private org store and a GitHub PR, not per-repo.

He is also right that this is the same root as concern 1. Both come from one design premise — that a memory is a controlled artifact that must be reviewed before it propagates. Automatic capture was gated behind an inbox for the same reason sharing was gated behind a PR.

The rebuild inverts that premise: **a repo's memories live in the repo, committed.** Then sharing needs no mechanism at all. `git clone` is the sharing mechanism, code review is the ratification mechanism, and branches give you per-branch memory for free — which also solves concern 2, because the spine lives in the same directory.

### 6. Refactors with agents turn into month-long painful failures

**This is the concern that decides the answer, and the evidence in this repo supports the owner rather than contradicting him.**

First, honesty about what the history does *not* show. A subagent checked every commit. There is no runaway rewrite spiral here: additions beat deletions roughly 10:1 (130,917 added, 12,859 deleted overall; 88,889 added in `src/`), the keyword grep for refactor/rewrite/rebuild yields 0/5/3 commits and the biggest of those is 694 lines changed, there is no "start over" commit anywhere, and `main` and `build/pogan-mem-v11` sit at the identical commit with zero divergence. The most-touched source file was touched 24 times in three weeks, which is ordinary iteration. **This project has never actually been through the refactor the owner fears.** That is not reassurance — it means the pain he describes came from elsewhere and this repo has not yet paid it.

Second, what the history *does* show, and it is the strongest argument for rebuilding. This repo went from nothing to 33,945 source lines and 46,391 test lines in **three weeks**, in bursty agent-driven batches — 112 commits on 08-04, 77 on 08-05, 119 on 08-09, three days producing 63% of all 490 commits. 46% of commits touched only `.ts` files under `src/`. **Greenfield construction from a plan is a demonstrated strength of this owner's setup.** The failure mode he describes is specific to refactoring, and the thing he is good at is exactly the thing I am recommending he do.

Third, the mechanism by which a refactor of *this* repo would go wrong. I can name it concretely rather than hand-waving about agent drift:

- **1,402 spec citations across 88 of 97 source files.** An agent refactoring `record.ts` reads a comment saying `§7` requires a behaviour and treats it as a constraint it must preserve. It cannot tell a live requirement from a dead one.
- **18,096 lines of plan and spec documents**, of which 13 files still contain literal "execute this plan" imperative language. An agent that greps for context finds instructions and follows them.
- **The citations are known to be wrong.** The owner's own commit `236b60f` found that 8 of 33 source citations in a single HTML doc were incorrect and fixed them. If a third of the citations in one audited file were wrong, the 1,402 in the source are not trustworthy either — but an agent has no way to know which.
- **46,391 lines of tests that encode the architecture you want to delete.** This is normally the best argument for refactoring, and here it inverts. The tests assert ladder promotion, gate floors, publish triage, ratify PR shapes, and shred behaviour. In a refactor they are 46,000 lines of tripwires defending the design you are trying to remove, and every one that fails presents an agent with a choice between "fix the code" and "fix the test" that it will get wrong roughly half the time.

Put together: a refactor asks an agent to hold in its head a 34k-line codebase, an 18k-line doc corpus that contradicts it in unknown places, and a 46k-line test suite that defends the old design. That is precisely the setup that produces the skipping, hallucinating, and drifting the owner described. A rebuild asks it to write 3,000 lines against a one-page brief.

## A smaller design that satisfies concerns 1-5

```
pogan-mem v2 — one plugin, target 3,000-4,000 source lines, no spec document.

1. STORE   One SQLite file per repo at <repo>/.pogan/mem.db is WRONG — use text.
           <repo>/.pogan/notes/*.md, committed. One file per note, YAML frontmatter.
           A derived index (fts5 + vec0 384d) is rebuilt from those files into
           ~/.cache/pogan/<repo>.db and is genuinely disposable.
2. CAPTURE Automatic, no model discipline. hooks.json, type:"agent", cheap model:
             Stop            -> read session, emit 0-3 notes as JSON, write directly.
             PostToolUseFailure -> one structured note per novel failure.
           Nothing lands in an inbox. Nothing waits for a human.
3. QUALITY Curate at read time, not write time. A note retrieved and not contradicted
           gains confidence; a note never retrieved in 60 days decays out.
           No publish, no ratify, no PR, no ladder, no gate, no org, no keys, no shred.
4. KINDS   Open string, not an enum. Starter set: gotcha, api-shape, convention,
           preference, failed-approach, recipe, decision, fact, glossary, env, perf,
           debt. Adding one must never require a code change.
5. RECALL  SessionStart injects a small budget of top notes for this repo+branch.
           UserPromptSubmit adds query-matched notes. mem_search MCP tool on demand.
           Reuse query.ts's RRF + graph expansion + rerank almost verbatim.
6. SPINE   <repo>/.pogan/spine.jsonl, committed, one line per file/symbol, with a
           human-editable "note" field. Regeneration MERGES by symbol key: keep human
           notes, mark orphans, never wipe. Reviewable as a normal PR diff.
7. SHARING git clone. That is the whole mechanism. Branches give per-branch memory free.
8. TRANSCRIPTS Install obra/episodic-memory as a separate plugin. Read-only. Do not fork.
```

Concern 1 is answered by item 2, concern 2 by item 6, concern 3 by item 8, concern 4 by item 4, concern 5 by item 7. Concern 6 is answered by the total: this is one week of greenfield agent work, in a repo with no spec corpus to drift against.

## Which path actually finishes?

**Refactor path.** Delete 14,000 lines of org/crypto/host machinery that 21 files import. Rewrite the storage write path to remove the ladder call at `build.ts:616`. Unpick `gate.ts` from session accounting and the spine DB. Move knobs out of `build.ts`. Replace the inbox with direct writes, which invalidates the publish and ratify commands and their tests. Re-home the spine from a cache DB to a committed text file and repoint six integration sites. Open up the kind enum and migrate the schema. Reconcile 1,402 spec citations, some unknown fraction of which are wrong. Delete or rewrite most of 46,391 test lines. My estimate: **six to ten weeks, with a high chance of stalling**, because there is no point in the middle where the system works — you are between two architectures for the entire duration. That is the exact shape of the failure the owner described.

**Rebuild path.** New repo. Copy `query.ts`, `embed.ts`, `spine/parse.ts`, `ids.ts`, `runProcess.ts` and the good half of `record.ts` — about 1,900 lines. Write the store, the Stop-hook capture, the SessionStart injection, and the MCP search tool fresh. Install episodic-memory alongside. My estimate: **one to two weeks to a system that works end to end**, and — the part that matters — **something demonstrably learning by roughly day three**. Every day after that is improvement to a working thing rather than repair of a broken one.

The asymmetry is not really about lines of code. It is that the rebuild has a working system early and the refactor does not have one until the end.

## The biggest risk of my own recommendation

**The rebuild becomes pogan-mem v2 at 30,000 lines with the same disease.**

That is not a hypothetical. It is what happened the first time, and the mechanism is visible in the history: 18,096 lines of plan documents came first, and the agents built every line of them, faithfully and fast. The planning process is what produced a 2,856-line doctor, an SSH shred protocol, and a GitHub PR ratification flow for a single user who had filed five memories. The agents did not fail. They succeeded at building the wrong specification.

So the rebuild only works if the plan is small. Concretely, three guardrails:

- **A hard line budget of 4,000 source lines.** If a feature does not fit, it does not go in.
- **No spec document.** A one-page README of intent, and nothing in the repo that an agent can mistake for an instruction. No `§` citations in comments, ever.
- **A working automatic loop before anything else.** Capture on `Stop`, store, inject on `SessionStart`. If it is not learning something useful within a week, stop and rethink rather than adding features.

A secondary risk worth naming: the same six concerns will not all be visible in the new system on day one, and the owner may read that as regression. It is worth agreeing up front that v2 will do less and do it automatically, and that "less" is the point.

## The strongest argument against me, stated fairly

The codebase is in excellent technical health. All of this is verified by actually running the commands:

- `npm run typecheck` exits 0. Zero errors.
- `npm run build` exits 0. Zero errors.
- The full suite: **2,535 tests across 100 files, all passing, zero failures, zero skipped**, in 87 seconds wall time.
- Zero TODO/FIXME/HACK/XXX comments in `src`. Zero `@ts-ignore` or `@ts-expect-error`. One genuine `any`, a narrow statement-type shim at `src/core/index/build.ts:147`. Zero `console.log` outside the CLI.
- The tests are real, not theatre. `src/plugin/mcp/server.test.ts` drives an actual MCP client against the actual server over an in-memory transport, with real `git init` repos and a real SQLite index, asserting on committed content and git log. Mocking is rare: zero `vi.mock`/`vi.fn` in three of the five test files sampled, and four occurrences across 2,506 lines in the largest.
- All five native dependencies are installed and built for this platform.

This is better engineering hygiene than most production repos. Anyone arguing for a rebuild has to answer it, so here is the answer.

**A green test suite tells you the code does what it was specified to do. It says nothing about whether the specification was right.** Every one of those 2,535 tests passes, and the system it protects filed five memories in its lifetime and stopped capturing after four days. The tests are not wrong; they are faithful to a design the owner no longer wants.

Two supporting points. First, the system's own evaluation harness scores its retrieval at **recall@5 of 0.458 and MRR of 0.389** over 28 fixtures (printed by `src/core/evalRig.test.ts:312`'s `runEval` block during the run). That is mediocre by its own measure, and it is the one quality number the suite actually produces about the product rather than about the code. Second, the subagent looked for a test that writes a record via `mem_log` and then retrieves it via `mem_search` in one body and **could not find one** — the write path and the read path are each covered thoroughly but the end-to-end loop that is the entire point of the product is not directly asserted anywhere.

And then the inversion. In a normal refactor, 46,391 lines of green tests are your safety net. Here they assert ladder promotion, gate floors, publish triage, ratify PR shapes, org membership rules, and SSH shred behaviour — all of which the rebuild deletes. They are not a net. They are 46,000 lines of tripwire defending the architecture you are trying to remove, and each failure hands an agent a judgement call between "fix the code" and "delete the test" that it has no context to make correctly.

So the health of this codebase is real, and it is an argument for **archiving it intact and mining it for parts**, not for building the next system inside it.

## One thing I could not verify

I did not open all 13 plan files that contain "execute this plan" imperative language to confirm whether each is now wrapped in a DO-NOT-EXECUTE banner. The count of 13 is verified; the mitigation is not.
