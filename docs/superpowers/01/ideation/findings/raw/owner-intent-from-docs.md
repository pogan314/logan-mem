---
title: Raw — owner statements recorded in the old design docs
type: ideation
status: ideation
created: "2026-08-21 13:16 CDT"
updated: "2026-08-21 13:16 CDT"
version: "01"
sources: [../pogan-mem/docs/*.md and ../pogan-toolkit/docs/superpowers/**, read 2026-08-21]
---

> Raw compiled extract, preserved verbatim from the 2026-08-21 session's working files so the condensed docs that cite it stay traceable. Written by a subagent from the sources named above; read as evidence, not as conclusions. Any relative path quoted in this file (`../spec/...`, `brainstorming/...`, `02-plugin-consolidation-spec.md`) is relative to `/home/ubuntu/projects/org/pogan-toolkit/docs/superpowers/` — the OLD repo — not to this one.

# User-intent extraction — pogan-mem / pogan-toolkit design docs

Scope covered (read-only): pogan-mem/docs/{ai-agent-memory-open-questions.md, ai-agent-memory-system.md, "NM comments.md", "NM Plan evaluation.md", ai-memory-system-handoff.md, feedback/2026-07-16-cross-machine-sync-and-promotion-prompt.md}; pogan-toolkit/docs/superpowers/{brainstorming/discussion.Q+A.md, brainstorming/00-master-capability-list.md, brainstorming/knowledge-retrieval-methods.EDU.md, spec/open-discussion.F+Q.md, spec/00-pogan-mem-spec.md, spec/02-plugin-consolidation-spec.md, spec/03-outstanding-work-spec.md, spec/reviews/2026-08-11-v1.1-review-A.md, spec/reports/r3-rulings-regression.md, plans/2026-08-02-pogan-mem-04-ladder-and-clients.md, brainstorming/reports/r1/{05-ecc-claudemem.md,06-self-improving-loops.md,08-graph-storage.md}, brainstorming/reports/r3/{09-langchain-langgraph.md,10-github-memory-sweep.md}, brainstorming/reports/r8/25-decision-second-opinion.md}.

## Important scope note on the pogan-mem docs

Nearly everything in `docs/ai-agent-memory-system.md`, `docs/ai-agent-memory-open-questions.md`, and `docs/ai-memory-system-handoff.md` is **AGENT-AUTHORED** design/forensics prose with no attribution to Logan/Cody by name — Part III of `ai-agent-memory-system.md` and the OQ doc's "team input" sections attribute content to **"the team"** (a 2026-07-16 meeting with three participants: Nicholas + "the technical lead" + a third teammate — per `docs/feedback/2026-07-16-cross-machine-sync-and-promotion-prompt.md:1`), not exclusively to Logan. I have NOT presented team-attributed statements as Logan-specific unless a document names him directly or the content is otherwise clearly his (e.g., the machine path `/Users/logan/...` confirms he's one of the three, but does not tell you which "the team" sentences are his words specifically).

`docs/NM comments.md` and `docs/NM Plan evaluation.md` are both `authored: agent` (frontmatter) — synthesis/adjudication reports "pushed by Nick for Logan and Pasan," not Logan's own statements, except for two unattributed-by-name first-person quotes pulled from the raw meeting transcript (below) and one task assignment naming Logan.

The pogan-toolkit `docs/superpowers/` tree has much cleaner, direct attribution (owner rulings, direct quotes, 2nd-person "you" addressed to Logan) and is the stronger source.

---

## A. Verbatim user quotes (or near-verbatim, second-person-addressed) — pogan-toolkit

1. **HARD CONSTRAINT — single-plugin packaging, no scattered hand-written harness entries.**
   Quote: `"I HATE how you have any skills/agents/harness-components at all that are NOT just simply installed with the plugin."`
   File:line: `docs/superpowers/spec/02-plugin-consolidation-spec.md:5`
   Attribution: explicit — "Owner ruling" heading (spec title: "Spec 02 — Plugin consolidation (owner ruling 2026-08-11)"), quote given verbatim in the doc.
   Status: HARD CONSTRAINT, ruled/settled. Note: per the doc's status banner, the resulting build (spec 02) was halted mid-execution "by owner order pending spec review" (`02-plugin-consolidation-spec.md:3`) — so the constraint stands but the implementation is incomplete/parked, not abandoned.

2. **PREFERENCE — track AI model + effort level per record.**
   Quote: `"I want to track AI model and effort level"` (section heading, phrased as the user's own question/request)
   File:line: `docs/superpowers/spec/open-discussion.F+Q.md:117`
   Attribution: doc framing is direct address to the user ("Each section matches the spec section you asked about," line 5); heading text is presented as his own words.
   Agent's response: agreed, adopted ("Action: yes — add to the spec on the next edit pass," line 124). Confirmed later as ratified in `00-pogan-mem-spec.md`'s "F+Q rulings" list: "`born_in.model` + `born_in.effort` added under the verified-not-guessed rule."

3. **QUESTIONS/PUSHBACK the user raised, quoted as section headers (2nd-person-addressed doc, "your questions"):**
   - `"Aren't member and user effectively the same thing?"` — `open-discussion.F+Q.md:10` — resolved as-designed, no change.
   - `"Shouldn't the spine just be saved in the actual repo?"` — `open-discussion.F+Q.md:25` — user pushed for simplicity/less-moving-parts; agent explained why not (spine = disposable cache, never lives in a repo you don't own); no change made.
   - `"Why is ~/.pogan-keys not inside .pogan?"` — `open-discussion.F+Q.md:49` — pushback on structure; agent held firm (keys must never travel with the store); no change.
   - `"Are we sure about dates for events? What if we don't touch a repo for 120 days — is everything relevant gone?"` — `open-discussion.F+Q.md:60` — user worried about data loss/retention; answered "nothing is ever gone," no change needed.
   - `"The github_username in your example is my org, not my username"` — `open-discussion.F+Q.md:79` — user caught a real doc error; agent: "You caught a real error" — action taken (fix the example).
   - `"No tags? Shouldn't typescript/aws/postgres be tagged somehow?"` — `open-discussion.F+Q.md:94` — user pushed for a feature (freeform tags); agent recommended against adding it now (status quo A).
   - `"Who is main? That's confusing"` — `open-discussion.F+Q.md:111` — user flagged unclear naming; cosmetic rename floated, "your call," not confirmed decided.
   - `"What about tracking which spec/plan a memory came from?"` — `open-discussion.F+Q.md:126` — user request; adopted as optional `born_in.task` field.
   - `"Don't we want a preference kind? Where do bugs/issues/the BOARD fit?"` — `open-discussion.F+Q.md:134` — user proposed a new record type + integration with his BOARD tool; agent recommendation: no new kind, and explicit refusal to let memory become a task tracker ("Rebuilding a task tracker inside the memory system is exactly the kind of scope creep the refuse list exists to stop," line 147). Roll-up table (line 191) records this as declined.
   - `"How are new categories added? What about sub-categories?"` — `open-discussion.F+Q.md:152` — no action needed, existing design covers it.
   - `"Is compact even a Claude Code hook? Do we get post-compact injection?"` — `open-discussion.F+Q.md:168` — user's instinct ("don't we want post-compact injection?") already matched the design (line 177).
   Framing: HARD RULE requested by the task — these are quoted section headers in a document explicitly addressed to the user as "your questions" (line 1, line 5). They read as his actual review comments, not agent inventions, though the doc does not use his name.

4. **HARD CONSTRAINT (evidence standard for "done"):**
   Text: `"a box closes only on code, tests, commit DIFFS, or live system state — never on a commit message, log prose, or a plan's own claim"`
   File:line: `docs/superpowers/spec/03-outstanding-work-spec.md:9`
   Attribution: labeled "(owner ruling 2026-08-11)" but the sentence itself is agent-paraphrased, not inside quotation marks — treat the *rule* as owner-attributed but the *wording* as the doc's paraphrase of the ruling, not his literal sentence.
   Status: HARD CONSTRAINT, ruled/settled.

5. **Design constraint reflecting the user's own working pattern (paraphrased, not quoted):**
   Text: `"the owner's real pattern is one session left open for weeks, under which a deferred commit means weeks locally-invisible-to-other-machines and unbacked-up"`
   File:line: `docs/superpowers/spec/00-pogan-mem-spec.md:413` (also cross-referenced in `spec/reviews/2026-08-11-v1.1-review-A.md:100`)
   Attribution: "(owner ruling 2026-08-08, r-exec R5)" — paraphrase of his stated usage habit, not a direct quote. This is a fact-about-him used to justify a design change (commit-inline-always), not literally a "want," but it drove a hard rule.

6. **Capture-trigger instruction, widened by owner ruling (paraphrase; the resulting sentence itself is agent-authored tool text, not the user's words):**
   File:line: `docs/superpowers/spec/00-pogan-mem-spec.md:390` — "widened from the failure-only sentence by owner ruling, 2026-08-11." Not a quote of him; a ruling that changed scope of what the tool auto-prompts for.

## B. Short verbatim phrases attributed to "Cody's" brief/design/plan, embedded in agent research reports (pogan-toolkit `brainstorming/reports/`)

These are agent-authored reports, but each pulls a short phrase in quotation marks that the report explicitly attributes to Cody's own brief/plan — treat the *phrase* as reasonably reliable as his own wording (it's quoted, not paraphrased), while the surrounding sentence is agent commentary, not his.

- **HARD CONSTRAINT — no paid API keys / subscription-only.**
  `"no paid API keys" rule` — quoted twice: `brainstorming/reports/r1/05-ecc-claudemem.md:81` and `:181` ("the single most on-point historical cautionary tale for Cody's 'no paid API keys' rule").
- **PREFERENCE/REQUIREMENT — agent self-modification.**
  `"agent adjusts its own process"` capability — `brainstorming/reports/r1/05-ecc-claudemem.md:151` ("Cody's brief specifically wants the 'agent adjusts its own process' capability").
  `"adjust, create, or modify its own processes"` — `brainstorming/reports/r3/10-github-memory-sweep.md:17` ("Cody's plan to let an agent...").
  `"agent notices it's repeating a mistake and modifies its own process" requirement` — `brainstorming/reports/r3/09-langchain-langgraph.md:13`.
- **REQUIREMENT — cross-machine shared/personal memory split.**
  `"shared user level on top of project level"` requirement — `brainstorming/reports/r1/08-graph-storage.md:96`.
- **Cody's own idea, not agent-proposed (provenance note, not a "want" per se):**
  `"database spine" idea` — `brainstorming/reports/r1/05-ecc-claudemem.md:169` and `08-graph-storage.md:3` — the code-map/spine concept originated with him, per these reports; useful for distinguishing user-originated ideas from agent-proposed ones elsewhere in the corpus.

## C. Daily-experience / human-review statements (mixed attribution — flag carefully)

- `discussion.Q+A.md` (agent-authored Q&A responding to the user's own numbered questions, none of which are reproduced verbatim in that file) contains an agent's paraphrase of a user position: `"you confirmed you will not use the system until both halves are done"` (spine + memory) — `brainstorming/discussion.Q+A.md:231`. This is AGENT-PARAPHRASED, not a quote — flagging because it directly overturned the agent's own recommendation (agent argued memory-first, decision was spine-first) and is recorded as the reason.
- `00-master-capability-list.md:15` — "Two layers, in your own words:" followed by "The spine" / "The memory" descriptions. Framed as restating the user's own terminology, but not in quotation marks — treat as AGENT-PARAPHRASED-CLAIMING-TO-BE-HIS-WORDS, not a direct quote.
- CRUD-UI refusal: `discussion.Q+A.md:41` has the AGENT saying "I would refuse" a CRUD interface for editing memory (arguing markdown-in-git must stay the sole write path). A later, different report reframes this as the user's own decision: `brainstorming/reports/r8/25-decision-second-opinion.md:125` — "the thing you already refused when you refused a CRUD UI." This is an inconsistency: one source frames it as an agent recommendation, another frames it as something "you already refused." Given `00-master-capability-list.md`'s "The decisions" section (line 320: "Settled with the project owner, each one reviewed independently before being recorded here") lists "What I would refuse to build" as a recommendations-section item (not inside "The decisions" itself), I could not confirm from these files alone whether the CRUD-UI refusal was actually ratified as the user's decision or is still an agent recommendation later mischaracterized. **Flagging as OPEN / UNCONFIRMED which document is right**, rather than asserting either.

## D. Raw meeting-transcript quotes (pogan-mem `docs/NM comments.md`) — attribution to Logan specifically is NOT confirmed by name

These are the only genuine first-person verbatim quotes found in the pogan-mem doc set (pulled from the raw 2026-07-16 meeting transcript by an agent-authored review doc, `authored: agent`). None are attributed to "Logan" by name in the surrounding text — they're attributed to "the team" / "its own owner" generically. Context clues (the following action item assigns Logan to document the sync tool) make it plausible the sync-tool quote is his, but I am not asserting that as confirmed.

- `"I'm assuming the ecc1 works. That that is an underlying assumption."` — `docs/NM comments.md:86`. Speaker unnamed.
- `"I don't honestly have any fucking clue what it does. All I know is that it works."` — `docs/NM comments.md:88`, about the sync tool. Speaker unnamed, but the doc's action item 2 lines later (`docs/NM comments.md:132,150`) assigns "Logan: 30 minutes documenting the sync tool" — suggestive but not a confirmed same-speaker link.
- `"being disciplined in terms of us actually checking it and not just assuming it works... as a very impatient person, that's going to be honestly the hardest fucking thing."` — `docs/NM comments.md:52`. Speaker unnamed; "us"/"a very impatient person" is first-person self-description by one of the three meeting participants, not confirmed as Logan specifically.

## E. AGENT-AUTHORED design principles (pogan-mem) — explicitly NOT user-stated, no attribution given

The following read like "hard constraints" but the source documents give **no attribution to the user at all** — they are stated flatly as design principles by an unmarked-authorship document (`ai-agent-memory-system.md` §16, duplicated verbatim in the superseded `ai-memory-system-handoff.md` §2):
- "Subscription-only. No paid API keys, no paid hosted services." — `docs/ai-agent-memory-system.md:512`
- "Local-first & git-native." — `docs/ai-agent-memory-system.md:513`
- "Human-in-the-loop only at the one moment it matters" (capture/distillation fully automatic; the only human gate is memory→executing-tool conversion) — `docs/ai-agent-memory-system.md:514`
- "Compose, don't rebuild." / "Vibe-code, then harden." — `docs/ai-agent-memory-system.md:515,517`

These principles closely match the "no paid API keys" and "agent adjusts its own process" phrases independently confirmed as Cody's in section B above, and match the pogan-toolkit spec's ratified decisions — so they are very likely downstream of his actual stated wants, but **this specific document does not say so**, and per the task's hard rule they must be marked AGENT-AUTHORED here rather than attributed to him.

## F. "Team" (not solely Logan) design input — pogan-mem Part III / OQ doc / feedback file

All of Part III (`ai-agent-memory-system.md` §25–§27), the "Team input" annotations in `ai-agent-memory-open-questions.md`, and the raw prompt `docs/feedback/2026-07-16-cross-machine-sync-and-promotion-prompt.md` are attributed to **"the team"** (3 participants), confirmed human-originated ("Preserve provenance. This is team-authored design input (human-originated)," `feedback/...md:14`) but NOT individually attributed to Logan. Notable content (team-level, not confirmed Logan-specific):
- Runtime kill-switch for the retrieval/capture governor: "'this is pissing me off, relax it'" — `ai-agent-memory-system.md:755` / `feedback/...md:27-28` — a hypothetical UX phrase illustrating the desired tunability, not a literal user quote.
- "do not force users into a common file-path layout" — hard constraint, `feedback/...md:80`.
- "do not put multiple people on shared working storage" — hard constraint, `feedback/...md:69`.
- V1-first, don't over-granularize — explicit team agreement, `feedback/...md:39-40`.
- Containers/IP-protection explicitly deferred (parked, not now) — `feedback/...md:86-87`.
