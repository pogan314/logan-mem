---
title: pogan-mem spec promises
type: ideation
status: stale
created: "2026-08-21 13:14 CDT"
updated: "2026-08-21 16:43 CDT"
version: "01"
sources: [docs/superpowers/01/ideation/findings/raw/spec-promises-full.md]
---

# pogan-mem spec promises

This is history — it shows how a spec can be precise and still describe the wrong thing. Every clause below was written down as the definition of success and, per the spec's own status line, was never once run (status: "PARTIALLY IMPLEMENTED"; the toolkit is disabled as of 2026-08-21).

## The seven §0b acceptance clauses (the "week gate"), verbatim

Source: `01-pogan-mem-v1.1-spec.md` lines 29-38.

1. **Unprompted capture**: "`captured` events carrying `trigger: contract`... on ≥5 distinct days (gate criterion). Nudge-, CLI-, and instructed-origin captures... do not satisfy this clause."
2. **Agent-initiated retrieval**: "every ACTIVE DAY of the week... contains ≥1 `retrieved` event whose `channel` is `pull`... push-channel deliveries and injections are reported separately and never summed into this clause."
3. **Use-through**: "of the records rendered by push surfaces during the week, ≥20% (gate criterion) are subsequently pulled by identifier... within 48 hours (gate criterion) of the render."
4. **Human usefulness sample**: "the owner rates a uniform random sample of 10 (gate criterion) of the week's new records useful/not-useful; a majority (gate criterion) must be useful."
5. **Real cleanup**: "at least one bulk act retiring ≥3 (gate criterion) records surfaced by the never-retrieved report's pull-only view, none of them created by a build or gate."
6. **Organic promotion**: "one promotion reaching ratified that traces to a `promotion-proposed` event from the recurrence detector with two distinct project ULIDs (gate criterion)."
7. **Deliverability floor**: "100% (gate criterion) of the week's new records carry ≥1 anchor or the explicit `everywhere` anchor... because D7's refusal binds at EVERY record-creating path."

## Numeric targets the spec committed to

| Target | Value | Source |
|---|---|---|
| Human triage budget | ≤45 min/week (knob) | 00 spec §14 line 576 |
| Answer-key size before any knob moves | ≥25 entries | 00 spec §14 line 572 |
| Distiller acceptance rate | ≥30% over one 25-transcript run (gate criterion) | 01 spec §1 line 56 |
| Per-session injection budget | 40,000 chars (knob), +6,000/agent | 00 spec §8.1 line 367 |
| Session-start injection cap | 8,500 chars (knob) | 00 spec §8.2 line 372 |
| UserPromptSubmit gate injection | ≤3,000 chars (knob) | 00 spec §6.3 line 324/367 |
| PostToolUse first-touch injection | ≤1,200 chars, max 5/agent/session (knob) | 00 spec §8.1 line 363 |
| SessionStart hook timeout | 10s (workload target ~250ms median) | 00 spec §8.1 line 360 |
| `mem_log` write cap | 100 records/session (knob) | 00 spec §8.3 line 399 |
| Regression floor on supermemory's benchmark suite | recall below half fixture-store recall reopens the retrieval design | 01 spec §9 line 141 |
| Week-gate anchor coverage | 100% (gate criterion) | 01 spec §0b clause 7 |
| Scale test targets | 10k and 50k records (gate criteria) | 01 spec §4 line 77 |

## Non-goals, as stated

- 00 spec §15: "no model-extracted prose graphs, no auto-applied AI links, no vector index over code, no process outliving a session, no service on the read path, no automatic decay..., no automated prompt optimisers, no automated distiller, no memory auto-promoting into anything the harness executes..., no push-on-change, no marker files in target repos, no writes into repos we don't own."
- 01 spec §12: "Per-agent memory partitions; GitHub plan changes; LongMemEval/LoCoMo; any hook gaining a model call...; any auto-deletion or auto-applied contradiction resolution."

## Spec file sizes
- `00-pogan-mem-spec.md`: 603 lines
- `01-pogan-mem-v1.1-spec.md`: 194 lines
- Total: 797 lines
