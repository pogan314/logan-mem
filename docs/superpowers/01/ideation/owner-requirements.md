---
title: Owner requirements — what Logan said they want
type: ideation
status: ideation
created: 2026-08-21
updated: 2026-08-21
version: "01"
sources: [owner messages in the 2026-08-21 session; findings/owner-quotes-timeline.md for the July-August history]
---

# What the owner wants

- This is the owner's intent in their own words, with a plain-English reading under each quote. The quotes are verbatim. The readings are interpretation and can be wrong.
- The long history of earlier statements (July 16 – August 21) is in `findings/owner-quotes-timeline.md`. This file is the **current** position as of 2026-08-21, which overrides anything older.

## The one-sentence goal

> "My initial goal was I provided several highly popular memory-related repos ... and asked the agent to help me take all the best parts and pieces of each to build my own memory system."

## The verdict on the old build

> "After weeks of work, I found out that the system that was built basically doesn't create any usable memories at all. The entire system is effectively useless, and it is wildly over-engineered."

> "This time we are going to simplify the fuck out of it as well."

## Requirements, current as of 2026-08-21

| # | Owner's words (verbatim) | Plain reading |
|---|---|---|
| 1 | "The system needs to be automated to learn on its own or its fucking useless" | No human step anywhere in the path from "something happened" to "a memory exists and agents can see it" |
| 2 | "every memory is automatic. The correct 'approval' thing was just a YAML key/value that clarifies for the agent whether or not a memory was reviewed by a human" | Human review is a **flag on a memory**, never a gate in front of it. Unreviewed memories are shown to agents exactly like reviewed ones; the flag only adds weight |
| 3 | "if comparing two memories by two different users or something be like 'okay lets maybe add significance to the verified memory'" | A `human_verified`-style field is a tiebreaker the agent may use, not a filter |
| 4 | "I originally had said I wanted a way to basically promote memories into actual harness components (which is a different system, not the actual memory)" | Turning memories into skills / hooks / rules / agents is a **separate subsystem** from memory. It is the place a human review makes sense |
| 5 | "a coordinator coordinating a long running autonomous execution and executors keep making the same mistake so the coordinator changes their strategy instead of repeating that mistake indefinitely" | "Automatic learning" primarily means: within one long run, the coordinator notices repeated failure and adapts. This is the defining example |
| 6 | "There is also the potential to include in this the ability to create harness components for a given repo" | Per-repo harness components (a repo-specific rule or skill) are a possible output of learning |
| 7 | "I don't see anything about per-repo memories" | Memories that belong to one repo must exist as a first-class thing, and be shared with whoever works on that repo |
| 8 | "I do want the spine to be possibly in the actual repo branch so we can have notes and descriptions and shit. It should probably be its own separate system within the monorepo" | The code map lives in the repo, is committed, carries human notes, and is its own subsystem, not a cache |
| 9 | "The memory types is the biggest failure ... I hated that it was the only type of memory that got written" | Build-failure memories are fine. The failure was that **nothing else** ever got written. Many kinds must get captured |
| 10 | "I'm interested in the obra/episodic-memory concept (either as a separate plugin or our own version built in with everything else)" | Searchable conversation history is wanted; packaging is open |
| 11 | "The two party approval is not necessary" | Drop the two-person pull-request gate on shared rules |
| 12 | "Aren't shared and org essentially the same thing? ... why do we need per-person folders if we are using GitHub, can't the commit id ... be used" | Collapse the old three-store split. Attribution can come from git authorship or a field, not folder structure |
| 13 | "are the encryption things really necessary? ... everything is for internal purposes" | Encryption is probably unnecessary; the owner does not hate it but sees no value and a real cost (incomplete results) |
| 14 | "the other major issue was not making it a plugin" | One plugin. One install. One uninstall. Nothing scattered across the machine |
| 15 | "we are going to build in versions but the first version is still going to be a substantial build" | Versions are allowed (this revokes the old "one full version" rule). Version 01 is still real, not a toy |
| 16 | "everytime I try to refactor anything claude gets wildly confused and starts skipping and hallucinating and drifting wildly" | The build process itself must be designed so agents cannot drift: small scope, no ambient stale docs, hard budgets |

## Standing constraints carried over from the old project (still true unless the owner says otherwise)

- No paid API keys. Everything runs on the Claude subscription or locally.
- Local-first. Plain files a human can open. GitHub is a backup/sync photocopy, never the brain.
- Memories are never hard-deleted except by the owner; they get deactivated or superseded.
- Docs are bullets and tables, not paragraphs. Every claim verified against code or live state. No invented causes.
