---
title: Supermemory (supermemoryai/supermemory)
type: wiki
status: research-fact
created: "2026-08-21 13:08 CDT"
updated: "2026-08-21 13:08 CDT"
sources: [docs/wiki/research-extracts/repos-toplevel-docs.md, live `gh api repos/supermemoryai/supermemory` call on 2026-08-21]
---

- URL: github.com/supermemoryai/supermemory
- License: MIT
- Stars: 28,987 — last push: 2026-08-21 (verified live 2026-08-21)

## What it does

- A hosted memory platform with a broad integration surface: REST API, SDKs, a hosted OAuth MCP server, a browser extension, and roughly 25 framework integrations.
- Core pitch: treat memory like files an agent can already navigate (list, read, grep) rather than a bespoke API.
- Ships a local mode (`npx supermemory local`) that runs single-process with a local, on-device default embedding model.
- Memory entries carry versioning (parent/root memory IDs, an `isLatest` flag) and typed relations between memories (`updates`, `extends`, `derives`).
- Supports per-user profile-style retrieval and a background "dreaming" extraction pass that builds graph links between memories.

## How its memory works

- Capture: ingests via API/SDK/connectors; a conversations client does smart diffing on re-ingest of the same conversation.
- Store: the MIT-licensed open repo has zero database schema, migration files, or extraction/search code in it — no `CREATE TABLE`, `pgTable`, or `sqliteTable` anywhere in the clone. The actual storage/extraction engine is closed.
- Retrieve: `POST /v4/search` with a configurable relevance `threshold`; also supports precomputed per-user profile retrieval.
- Lifecycle: memories carry `isInference`/`isForgotten`/`isStatic`/`forgetAfter`/`forgetReason` flags, and a `forget-matching` bulk-deletion endpoint exists.
- No human review gate: a memory written into a shared space is live the instant it lands — there's no pending/approval state.

## Files that implement it

| path | what it does | verified-exists? |
|---|---|---|
| packages/validation/schemas.ts | `MemoryEntrySchema`, `DocumentSchema`, etc. — the versioning/relation/provenance field definitions | yes |
| apps/mcp/wrangler.jsonc | Cloudflare Workers + Durable Objects config for the hosted MCP server | yes |
| apps/docs/concepts/how-it-works.mdx | docs describing it as a "Temporal Vector-graph engine" | yes |
| apps/docs/self-hosting/configuration.mdx | self-host configuration docs | yes |
| apps/docs/connectors/ | connector docs directory | yes (directory) |
| apps/docs/concepts/graph-memory.mdx | describes the background "dreaming" extraction pass | yes |
| packages/tools/src/conversations-client.ts | smart-diffing re-ingest client | yes |
| packages/validation/api.ts | search endpoint parameter definitions, including `threshold` | yes |
| apps/docs/concepts/user-profiles.mdx | precomputed profile retrieval docs | yes |
| apps/docs/self-hosting/local-vs-enterprise.mdx | local-vs-paid-tier comparison | yes |
| apps/docs/overview/security.mdx | SOC 2 / GDPR / HIPAA claims | yes |
| apps/mcp/src/server/analytics.ts | per-tool-call PostHog telemetry inside the MCP server | yes |
| apps/mcp/src/server/tools/ | roughly 15 MCP tool implementations | yes (directory) |
| packages/tools/src/openai/middleware.ts | `withSupermemory` OpenAI client wrapper | yes |
| apps/browser-extension/entrypoints/content/ | browser extension content scripts | yes (directory) |
| skills/supermemory/SKILL.md | Claude Code skill for supermemory | yes |
| .github/workflows/ci.yml | CI config — runs typecheck/lint only, not the test suites | yes |

## Good

- The broadest integration surface of any product surveyed: 32 REST paths, a hosted MCP server, a browser extension, roughly 25 framework integrations.
- Local mode genuinely runs single-process with a local default embedding model (`Xenova/bge-base-en-v1.5`), and the docs support running fully offline against Ollama, LM Studio, or vLLM.
- The memory schema has real versioning and typed relations (`updates`/`extends`/`derives`), more structured than most competitors' flat fact stores.

## Bad

- The differentiating engine is closed — the open MIT repo has no database schema or extraction code at all; what's open is the client/integration layer, not the memory logic itself.
- No human review step — a memory written to a shared space is live immediately, with automatic contradiction resolution (`isLatest`) deciding which fact is current.
- Telemetry is real and on by default in the open code: PostHog in the web app, per-tool-call PostHog in the MCP server, plus Sentry sourcemaps.
- CI only runs typecheck and lint — it does not run the test suites.
- "Self-hosted" is a pricing-page trap: the free path is single-user OSS mode still on a 0.0.x version; anything beyond that is paid Enterprise, at a $399/mo Scale tier.

## Worth stealing

- The `extends`/`derives`/`updates` typed-relation vocabulary for linking memories, as a smaller idea than a full graph.
- A configurable search `threshold` parameter, and `isStatic`-style pinning to exempt a memory from auto-forgetting.
- Diff-on-re-ingest idempotency for repeated conversation captures.
- Precomputed per-user profile retrieval as a cheap alternative to search on every turn.

## Old project's verdict

History that binds nothing — study only; the steal-worthy ideas were all called retrieval-ergonomics accents, not architecture, and its LLM-driven extraction/"dreaming", automatic decay, and always-on LLM proxy were all explicitly flagged as not worth stealing.
