---
title: Raw — every promise the old spec made
type: ideation
status: stale
created: "2026-08-21 13:16 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [../pogan-toolkit/docs/superpowers/spec/00-pogan-mem-spec.md, 01-pogan-mem-v1.1-spec.md, read 2026-08-21]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions. Any relative path quoted in this file (`../spec/...`, `brainstorming/...`, `02-plugin-consolidation-spec.md`) is relative to `/home/ubuntu/projects/org/pogan-toolkit/docs/superpowers/` — the OLD repo — not to this one.

# pogan-mem spec promises — extracted verbatim

Repo: /home/ubuntu/projects/org/pogan-toolkit. Read-only extraction; no evaluation of implementation status performed here.

## File STATUS blocks (declared status, quoted)

**docs/superpowers/spec/00-pogan-mem-spec.md:3** — `> **STATUS (2026-08-21): PARTIALLY IMPLEMENTED.** Evidence checked directly against the codebase: ... This is a design document, not a description of current behavior — verify every claim against the codebase before relying on it. The toolkit is currently disabled (see README.md). All unexecuted work now lives in `../plans/2026-08-11-pogan-mem-v11-05-outstanding.md`.`

**docs/superpowers/spec/01-pogan-mem-v1.1-spec.md:1,3** — Title line: `# pogan-mem v1.1 — post-release wave 1 (spec, coordinator-authored — RATIFIED 2026-08-11)`. Status block: `> **BUILD STATUS (2026-08-21): PARTIALLY IMPLEMENTED.** Evidence checked directly against the codebase: ... This is a design/ratification document, not a description of current behavior — verify every claim against the codebase before relying on it. The toolkit is currently disabled (see README.md). All unexecuted work now lives in `../plans/2026-08-11-pogan-mem-v11-05-outstanding.md`.`

**docs/superpowers/plans/2026-08-02-pogan-mem-00-roadmap.md:3** — `> **STATUS: HISTORICAL RECORD — marked 2026-08-21.** This is an index over the four `2026-08-02-pogan-mem-0{1,2,3,4}-*.md` plan files, not itself a task plan ... **DO NOT EXECUTE.** This file and everything it indexes describes work already done. The toolkit is fully disabled on this machine (see `README.md`) and a redesign is pending, so nothing here should be run.`

**docs/superpowers/plans/2026-08-11-pogan-mem-v11-00-roadmap.md:3** — `> **STATUS: HISTORICAL RECORD — marked 2026-08-21.** This is an index over the three `2026-08-11-pogan-mem-v11-0{1,2,3}-*.md` plan files, not itself a task plan — it carries no checkboxes of its own. ... **DO NOT EXECUTE.** This file and everything it indexes describes work already done. The toolkit is fully disabled on this machine (see `README.md`) and a redesign is pending, so nothing here should be run.`

---

## 1. The v1.1-§0b acceptance clauses — quoted verbatim, numbered

Location: docs/superpowers/spec/01-pogan-mem-v1.1-spec.md, lines 29–38, under the heading "### The week gate (the wave's overall pass criterion)".

Lead-in text (line 29): "The chain is demonstrated live and measured from the store's own events — never narrated, and never satisfiable by activity alone (review C-A1: draft 3's count-only gate would have passed on the v1 store the owner judged useless). Preconditions, verified by `doctor` before the week starts (review B-26): the embedder is provisioned (§4c), §5b(c) default-on is live, and §4b's report command exists and its dated fixture dry-run artifact is present on disk (the dry run writes one — an observable fact, not a claim). The artifact is `pogan report --week`'s output, and it names the participating hosts (a single-host week is reported as a single-host result, review B-27). Per-session denominators are avoided throughout: under the D6-ruled weeks-long-session pattern the week's session count is on the order of one, so clauses denominate on active days and bounded windows instead (confirmation review F2). Clauses:"

1. (line 31) "**Unprompted capture:** `captured` events carrying `trigger: contract` (A6's new enum) on ≥5 distinct days `(gate criterion)`. Nudge-, CLI-, and instructed-origin captures are reported as separate lines and do not satisfy this clause (reviews B-20/21)."

2. (line 32) "**Agent-initiated retrieval:** every ACTIVE DAY of the week (a day with ≥1 event of any kind; the week must contain ≥5 active days `(gate criterion)`) contains ≥1 `retrieved` event whose `channel` is `pull` (A6's new emitter-stamped field — via-value inference cannot class the shipped enum, whose `fts-only` and `graph:*` forms are emitted from both pull and push sites); push-channel deliveries and injections are reported separately and never summed into this clause (review B-22)."

3. (line 33) "**Use-through:** of the records rendered by push surfaces during the week, ≥20% `(gate criterion)` are subsequently pulled by identifier (a `retrieved{via: get}` — `mem_get` gains this emission, A6/A11; today it emits nothing, and `direct` already means found-by-both-arms) within 48 hours `(gate criterion)` of the render — the rendered-vs-consumed distinction of §13-A11 (reviews C-A1/C-A14; window per F2, since the owner's session spans weeks)."

4. (line 34) "**Human usefulness sample:** the owner rates a uniform random sample of 10 `(gate criterion)` of the week's new records useful/not-useful; a majority `(gate criterion)` must be useful (review C-A1)."

5. (line 35) "**Real cleanup:** at least one bulk act retiring ≥3 `(gate criterion)` records surfaced by the never-retrieved report's pull-only view, none of them created by a build or gate (review B-25; §6's own two-junk-records criterion is a separate, smaller test)."

6. (line 36) "**Organic promotion:** one promotion reaching ratified that traces to a `promotion-proposed` event from the recurrence detector with two distinct project ULIDs `(gate criterion)`; a manually-scoped promotion is reported but does not satisfy the clause (review B-24)."

7. (line 37) "**Deliverability floor:** 100% `(gate criterion)` of the week's new records carry ≥1 anchor or the explicit `everywhere` anchor — mechanical because D7's refusal binds at EVERY record-creating path (see §11 item 7's scope) (review C-A15, confirmation F9)."

### The chain table (context for §0b, lines 9–25) — the 13 named links §0b's items are meant to close

Table header (line 9): "The owner's bar (2026-08-11): an elite, extremely high-performing memory system — measured not by features but by whether the chain below holds end to end. Every v1.1 item exists to close a named link. The closer column distinguishes three honest states (review C-A12): **code** (closed by shipped, gated code), **process** (closed by a documented, exercised procedure), and **OPEN** (not closed this wave, with the reason stated). Reviewers audit this table first."

Rows quoted exactly (Link | State after v1 | v1.1 closer), lines 13–25:
1. Plugin loads in every session | BROKEN — nothing loads it by default | code: §5b(c) default-on
2. Session start orients the agent | built, but delivered once per multi-week session | code: §5c re-denomination (D6, ruled)
3. Agents WRITE memories during work | behavioral, uninstructed; 4 of 5 live records anchorless | code: §5b(b) contract; §5b(d) nudge; D7 anchor enforcement (ruled)
4. Every turn SURFACES relevant memories | built but lexical-only, absolute-threshold, measured near-inert | code: §5b(a) index-derived gate (D8, ruled) + (a2) touched-anchor arm — push, bounded by design; the pull side (§5b(b) contract + `mem_search`) carries per-turn coverage per §5c's ruled position
5. Deep retrieval is semantic | built; embedder PROVISIONED live (§4c rider DONE 2026-08-12 — doctor `modelsFlag` ok, fused eval measured) | process: §4c ops rider (a week-gate precondition)
6. Usage improves ranking | built (counters, confidence ladder), but push deliveries pollute the pull signal | code: §5 pull/push counter split
7. Junk gets cleaned cheaply | report only, one-at-a-time; push-delivered records invisible to it | code: §6 bulk cleanup; §5 pinning; pull-only never-retrieved view
8. Past work gets mined | absent | code: §1 distiller; §2 documenter (evidence-gated)
9. Knowledge becomes team law | built, live-verified end to end | — (holds)
10. Knowledge becomes harness components | `pogan tally` surfaces candidates; no harvest path | code: §8b `pogan export` (D10, ruled adopted)
11. Quality is measured, knobs move on numbers | built; metric gaps; every number self-graded | code: §4 additions; process: §4's held-out answer key
12. Works where the owner works | one machine | OPEN — §9's Windows spike is an assessment, not an implementation; the second POSIX host's join is an owner item outside this spec
13. Stored claims stay true over time | NO MECHANISM — staleness checks paths, not truth; ratified records are the least revisable (review C-A5) | code: §5d contradiction candidates + re-confirm line (D9, ruled)

---

## 2. Goals / non-goals

00-pogan-mem-spec.md has no section literally titled "Goals". Its framing/purpose bullets (lines 5–9) function as the closest thing to stated goals:

- (00 spec, line 5): "**What this is:** the concrete implementation contract for the system settled in `../brainstorming/00-master-capability-list.md`. Every value here (a field name, a timeout, a threshold) is a proposal you can veto line-by-line. Where this doc and the brainstorming doc disagree on *intent*, the brainstorming doc wins; where they disagree on a *concrete value*, this doc wins because the brainstorming doc deliberately left values open."
- (00 spec, line 6): "**One version.** Everything in this spec ships in the first release. Nothing here is staged for later; anything not worth shipping belongs on the refuse list, permanently."
- (00 spec, line 7): "**Numbers are knobs.** Thresholds and budgets marked `(knob)` are tunable after launch by measurement — the mechanism they parametrise is not."

Explicit non-goals section — **00 spec §15, line 583**, heading "## 15. Explicit non-goals" (line 581):
"The refuse list in `../brainstorming/00-master-capability-list.md` binds verbatim: no model-extracted prose graphs, no auto-applied AI links, no vector index over code, no process outliving a session, no service on the read path, no automatic decay (**proposals age out of a view; records never age; the never-retrieved report is a view, not decay**), no automated prompt optimisers, no automated distiller, no memory auto-promoting into anything the harness executes (run-scoped elevation only), no push-on-change, no marker files in target repos, no writes into repos we don't own."

v1.1 spec's own out-of-scope line — **01 spec §12, line 168-170**, heading "## v1.1-§12 Out of scope (owner-ruled)":
"Per-agent memory partitions; GitHub plan changes; LongMemEval/LoCoMo; any hook gaining a model call (the §5b(a2) anchor arm needs none); any auto-deletion or auto-applied contradiction resolution."

Roadmap "Goal" lines (these are the "goal/success sections" requested):

- **docs/superpowers/plans/2026-08-02-pogan-mem-00-roadmap.md:7** — "**Goal:** Build pogan-mem — the memory system specified in `../spec/00-pogan-mem-spec.md` — as one release, no staging, per the one-version mandate."
- **docs/superpowers/plans/2026-08-11-pogan-mem-v11-00-roadmap.md:7** — "**Goal:** Build the v1.1 wave specified in `../spec/01-pogan-mem-v1.1-spec.md` (RATIFIED 2026-08-11) on top of the released v1 (`main`, suite 81 files / 1930 tests green). The wave's overall acceptance is v1.1-§0b's week gate; the build's job is to make every gate clause mechanically measurable and every chain closer real."

Global constraints list (2026-08-02 roadmap, lines 79-96) function as a goals/non-negotiables list — key lines quoted:
- "**One version.** Everything ships in the first release; nothing is staged for later; refused things are refused permanently."
- "**The markdown is the system.** Three repos hold only markdown, JSON, JSONL, `.age`; every `.db` is derived and disposable."
- "**No process outlives a session.** No daemons, no caching agents; async work rides harness-owned async hooks."
- "**Hooks are FTS-only and model-free by contract.** The MCP server is the only model-warm process."
- "**Humans gate anything that executes or crosses a client boundary.** Run-scoped elevation only; provenance moves only through humans."
- "**Nothing is ever written into repos we don't own.** The spine's clean-tree test is `git status` byte-identical."
- "**Client-owned records are encrypted from first write.** Every file under an owned path is ciphertext except `project.json`."
- "**Never construct a git identity.**" (full text at line 88)

---

## 3. Every measurable/numeric target the spec commits to

### Latency / timing budgets
- 00 spec §1.4 line 88: "**Anchor resolution:** exists / moved / gone in <10ms from the per-repo db."
- 00 spec §1.4 line 89: "**Architecture card:** ≤60,000 chars (≈15k tokens — chars are the rule, tokens the rationale), regenerated on spine rebuild."
- 00 spec §4.2 line 277: "### 4.2 Matching (SessionStart, <150ms)"
- 00 spec §5 line 289: "`pogan spine build` parses the tree into `spine/<registry-ulid>.db`; <30s on a 2,000-file repo `(knob)`; rebuild is the only repair."
- 00 spec §8.1 hook table line 360: SessionStart (startup|resume|fork|clear) "10s (workload target ~250ms median)"
- 00 spec §8.1 line 361: SessionStart (compact) "2s"
- 00 spec §8.1 line 362: UserPromptSubmit "10s"
- 00 spec §8.1 line 363: PostToolUse (Read|Edit|Write) "5s"
- 00 spec §8.1 line 364: SessionEnd "30s"
- 00 spec §3.2 line 262: "`pogan in` appends one `<ulid>.md` in <50ms plaintext / <250ms `(knob)` encrypted"
- 00 spec §8.3 line 388: embedder cap "more than **4** `(knob)` are already resident on the box" triggers fallback; lease "stale after 5 minutes"
- 00 spec §8.3 line 395: `mem_search` pending_embed drain "bounded per call (≤50 records or ≤2,000ms per drain `(knob)`)"
- 00 spec §6.2 step 4, line 318: rerank fires when remaining `search_budget_ms` (4,000ms `(knob)`, config default) after fusion is ≥1,500ms
- 00 spec §12, line 518: SQLite `busy_timeout = 5000`
- 00 spec §8.1 line 367: orphan marker staleness "30 minutes `(knob)`"
- 00 spec §11.4 line 502: shred transcript-pass refusal window: newest transcript mtime "inside the last 15 minutes `(knob)`"
- 01 spec §1 line 44: distiller extract output budget "60,000 chars `(knob)`" with "per-transcript share of 4,000 chars `(knob)`"
- 01 spec §5c line 111: "internal assembly deadline of 1200ms `(knob)`" for compact-handler block re-assembly (2-second figure is the hook PROCESS timeout, stated same line)

### Token / char caps (injection budgets)
- 00 spec §8.1 line 367: per-session injection budget "40,000 chars `(knob)`"
- 00 spec §8.1 line 363 (PostToolUse first-touch): "≤1,200 chars, **max 5 injections per agent per session `(knob)`**"
- 00 spec §8.2 line 372: session-start injection cap "8,500 chars `(knob)`"
- 00 spec §8.2 table (lines 374-381): status slot ≤200; architecture digest ≤4,000; failed-approaches headlines ≤2,000; category decisions ≤1,500; queue pressure ≤300; reserve = remainder
- 00 spec §8.2 line 383: "up to 240 chars `(knob)` of the body's first paragraph"
- 00 spec §8.1 line 367 (r7 ruling h): "40,000 + 6,000 × the number of agents" total session injection scaling
- 00 spec §6.3 line 324/367: UserPromptSubmit gate injection "≤3,000 chars `(knob)`"
- 01 spec §5b(b) line 102: usage-contract header cap "600-char cap `(knob)`"; digest donates "4,000 → 3,300 `(knob)`"
- 01 spec §5 line 94: profile injection slot: "N=3 `(knob)`", "90-day window `(knob)`", "300-char cap `(knob)`"
- 01 spec §3 line 70: spine html node record listings "cap at 20 `(knob)`"

### Recall / precision / rate targets
- 00 spec §14 line 572: answer key "≥25 entries before any knob moves"
- 00 spec §16 step 1 line 594: "10/10 exact for anchors + category edges, ≥8/10 human-judged for one-liners"
- 00 spec §5b(a) (01 spec line 100): "AND recall 0.458 / empty-search 88.9% vs OR recall 0.917 / empty-search 5.6%, noise 0.322→0.718, must-not 0→2, both at FTS-only top-5" (measured T1b comparison, not a target but a measured evidentiary figure cited in spec text)
- 01 spec §9 line 141 (supermemory's benchmark suite): "a recall below half the fixture-store recall REOPENS the retrieval-design question as a docket item"
- 01 spec §1 line 56 (distiller pass criteria): "**acceptance rate** — over one real 25-transcript `(gate criterion)` run, the share of drafts the owner accepts at triage clears 30% `(gate criterion)`"
- 01 spec §2 line 65: "≥1 pair record and ≥3 module facts `(gate criteria)` survive triage"
- 01 spec §0b clause 1: "≥5 distinct days `(gate criterion)`"
- 01 spec §0b clause 2: "≥5 active days `(gate criterion)`"
- 01 spec §0b clause 3: "≥20% `(gate criterion)`" use-through, "within 48 hours `(gate criterion)`"
- 01 spec §0b clause 4: "10 `(gate criterion)`" sample, "a majority `(gate criterion)`"
- 01 spec §0b clause 5: "≥3 `(gate criterion)`" records retired
- 01 spec §0b clause 6: "two distinct project ULIDs `(gate criterion)`"
- 01 spec §0b clause 7: "100% `(gate criterion)`" anchor coverage

### Triage / human time budgets
- 00 spec §14 line 576: "**The human budget is a stated target: ≤45 minutes/week `(knob)`** for the owner across publish triage, ratification, staleness lines, inbox distillation, and the never-retrieved report."
- 01 spec §1 line 54: "§1's pass criteria include a measured triage-minutes-per-25-transcripts figure checked against 00 §14's ≤45-min/week human budget."

### Other numeric knobs / thresholds
- 00 spec §7.1 line 333: 20 hand-written categories
- 00 spec §7.2 line 338: "Realistically **~100–200 entries `(knob)`** at ship" for the glossary seed
- 00 spec §7.3 line 346: link "surfaced for review at confidence ≤ **−3** `(knob)`"
- 00 spec §7.3 line 345: proposals "age out of the view after 28 days `(knob)`"
- 00 spec §9 line 450 (doctor): "~18 checks"
- 00 spec §9 line 437 (`pogan tally`): "dormant below ~4 members"
- 00 spec §10.1 line 458: fact-recurrence match "cosine ≥ 0.83 `(knob — very likely too loose for bge-small...)`"
- 00 spec §5 line 294: module cluster "a directory at depth ≤2 with ≥3 parsed source files `(knob)`"
- 00 spec §6.1 line 311: vector brute-force scan "fine to ~50k records (~50–100ms)"
- 00 spec §6.2 step 1, line 315: each ranked list "cut at the top **20** `(knob)`"; `mem_search` default `k` = "**10** `(knob)`"
- 00 spec §6.2 step 3, line 317: graph expand "≤2 hops", fused top **10**, category fan-out cut "at the top **20** `(knob)`"
- 00 spec §6.2 step 5, line 319: layer weights "project 1.00 / project-member 0.97 / shared 0.94 / members-me 0.91 / user 0.88 `(knob)`"
- 00 spec §6.3 line 324: bm25 gate "`-bm25() ≥ 8.0` `(knob)`"
- 00 spec §8.3 line 399: `mem_log` "per-session write cap of 100 records `(knob)`"
- 00 spec §13 line 143 (event volume estimate, not a target): "~200 bytes/event × ~100 events/session × ~25 session-boundaries/day ≈ 0.5 MB/day/member"
- 00 spec §2.1 line 143: "index build compacts months older than 90 days `(knob)`"
- 00 spec §9 line 434 ("never-retrieved report"): "N records not retrieved in 90 days `(knob)`"
- 00 spec §16 build size (masthead line 9): "**~65–85 agent-days**, concentrated in steps 5 and 8 — a 13–17 week project with one reviewer."
- 01 spec §4 line 77 (scale harness): "10k and 50k records `(gate criteria)`"
- 01 spec §4 line 100 (T12 scale measurement): "50k index build 2704.9s → 67.7s"; floor derivation sample "up to 512 `(knob)` active records"
- 01 spec §5b(d) line 104: nudge trigger "≥3 `(knob)` consecutive failures"; "at most 3 `(knob)` per session"
- 01 spec §5d line 121: "Ratified standards older than 180 days `(knob)`"
- 01 spec §6 line 123: bulk deactivate — batch confirmation, no separate numeric cap stated beyond the existing TTY gate
- 01 spec §10 item 6, line 151: tmp-leak gate "`/tmp` entry-count delta ≤ 3 `(knob)`"
- 01 spec §2 line 63: module-fact clusters "above 30 files `(knob)`" walked per-subdirectory

---

## 4. Spec file sizes (line counts)

- docs/superpowers/spec/00-pogan-mem-spec.md — **603 lines**
- docs/superpowers/spec/01-pogan-mem-v1.1-spec.md — **194 lines**
- **Total (both spec files): 797 lines**

(Roadmap files, read for goal sections only, not part of the "spec" line-count ask: 2026-08-02-pogan-mem-00-roadmap.md = 111 lines; 2026-08-11-pogan-mem-v11-00-roadmap.md = 29 lines.)

---

## 5. Explicit "must NOT be built" / "stay simple" / anti-complexity warnings — quoted

- 00 spec §15 (line 583, full non-goals list quoted above): "no model-extracted prose graphs, no auto-applied AI links, no vector index over code, no process outliving a session, no service on the read path, no automatic decay ... no automated prompt optimisers, no automated distiller, no memory auto-promoting into anything the harness executes (run-scoped elevation only), no push-on-change, no marker files in target repos, no writes into repos we don't own."
- 00 spec §1.4 line 93: "Anything else the spine could do is **out of scope permanently**."
- 00 spec §5 line 294: "**No community detection, ever.**"
- 00 spec §5 line 292: "Other languages are out of scope for this release, permanently per the one-version rule unless re-ruled."
- 00 spec §7.1 line 333: "Still zero AI: a hand-maintained relation list is a table lookup."
- 00 spec §7.2 line 338: on the glossary seed — "Public glossaries (the awesome-list ecosystem) serve as a *lookup aid while mapping*, never as a bulk import source: a 500-entry import 'hand-curated' from lists gets drafted by an AI and skimmed by a human, which is quiet drift toward the freeform-vocab trap this file exists to prevent."
- 00 spec §7.2 line 341: "Still a **closed** vocabulary with zero AI at runtime: a curated lookup table is not freeform tagging — nothing outside the file ever becomes a tag on its own."
- 00 spec §6.3 line 323: "**Hooks are FTS-only, by contract** (a hook is a fresh process; the 130MB model loads in seconds and cannot reach the MCP server's warm copy)."
- 00 spec §10.2 line 470: "distill | human, from `pogan tally` | same PR gate; **no automated distiller, ever**"
- 00 spec §1, line 6: "**One version.** Everything in this spec ships in the first release. Nothing here is staged for later; anything not worth shipping belongs on the refuse list, permanently."
- 01 spec §12 (line 170, full quote above): "Per-agent memory partitions; GitHub plan changes; LongMemEval/LoCoMo; any hook gaining a model call (the §5b(a2) anchor arm needs none); any auto-deletion or auto-applied contradiction resolution."
- 01 spec §1 line 41: "It never writes any live layer, never runs from a hook, and never schedules itself — the §15 no-unattended-AI line is preserved because a human invokes it and a human triages its output."
- 01 spec §5d line 119: "Detection, never resolution — inside the refuse list exactly as the never-retrieved report is."
- 01 spec §9 (Windows spike) line 142: "No implementation — which is why chain link 12 reads OPEN, not closed (review C-A12)."
- 01 spec §10 header, line 144: "## v1.1-§10 Hardening batch (all shipped-code corrections; no behavior redesigns)"
- Roadmap 2026-08-02-...-00-roadmap.md line 83: "**No process outlives a session.** No daemons, no caching agents; async work rides harness-owned async hooks."
- Roadmap 2026-08-02-...-00-roadmap.md line 84: "**Hooks are FTS-only and model-free by contract.** The MCP server is the only model-warm process."
- Roadmap 2026-08-02-...-00-roadmap.md line 86: "**Nothing is ever written into repos we don't own.** The spine's clean-tree test is `git status` byte-identical."
