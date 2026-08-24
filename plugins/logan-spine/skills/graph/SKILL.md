---
name: graph
description: "Use the codebase knowledge graph for structural code queries. Triggers on: explore the codebase, understand the architecture, what functions exist, show me the structure, who calls this function, what does X call, trace the call chain, find callers of, show dependencies, impact analysis, dead code, unused functions, high fan-out, refactor candidates, code quality audit, graph query syntax, Cypher query examples, edge types, how to use search_graph."
---

# Logan Spine — Knowledge Graph Tools

Graph tools return precise structural results in ~500 tokens vs ~80K for grep.

**Every tool except `list_projects` requires a `project` argument.** The examples below omit it for brevity; a real call must carry it, or it fails with `missing required argument: project`. The name is the graph project key, not the folder name — the `SessionStart` context line states it (`graph project="..."`), and `list_projects` returns it alongside each `root_path`. So `search_graph(name_pattern="...")` below is really `search_graph(project="<key>", name_pattern="...")`.

## Quick Decision Matrix

| Question | Tool call |
|----------|----------|
| Who calls X? | `trace_path(direction="inbound")` |
| What does X call? | `trace_path(direction="outbound")` |
| Full call context | `trace_path(direction="both")` |
| Find by name pattern | `search_graph(name_pattern="...")` |
| Dead code | `search_graph(max_degree=0, exclude_entry_points=true)` |
| Cross-service edges | `query_graph` with Cypher |
| Impact of local changes | `detect_changes()` |
| Risk-classified trace | `trace_path(risk_labels=true)` |
| Text search | `search_code` or Grep |

## Exploration Workflow
1. `list_projects` — check if project is indexed
2. `get_graph_schema` — understand node/edge types
3. `search_graph(label="Function", name_pattern=".*Pattern.*")` — find code
4. `get_code_snippet(qualified_name="project.path.FuncName")` — read source

## Tracing Workflow
1. `search_graph(name_pattern=".*FuncName.*")` — discover exact name
2. `trace_path(function_name="FuncName", direction="both", depth=3)` — trace
3. `detect_changes()` — map git diff to affected symbols

## Evidence Tiers
- **Scout (Tier 1):** fast positive lookup with few graph calls and targeted source checks. Treat results as provisional; never make absence, exhaustive, dead-code, or complete-impact claims.
- **Verify (Tier 2, default):** task-directed searches, relevant trace directions, exact snippets for material claims, and all relevant result pages.
- **Auditor (Tier 3):** bounded-scope full verification with a current graph generation, complete relevant pagination, both call directions and broader relationships when material, plus explicit unresolved limitations.
- **Every tier:** after candidate paths are known, call `check_index_coverage` once with every evidence path. For negative or exhaustive claims also include the relevant scopes. A clean result means no recorded gap, not proof of completeness. For partial, skipped, excluded, stale, pending, or unknown coverage, read/grep the reported ranges or scope before relying on the graph.

## Sessions and Subagents
- At session start or after compaction, call `list_projects`/`index_status` before structural exploration, then choose Scout, Verify, or Auditor for the task.
- Before delegating, query the graph and coverage in the parent. Pass the tier, exact project, generation/freshness, bounded scope, queries and pagination state, qualified symbols, paths, call-chain findings, coverage ranges/reasons, source fallback already performed, and unresolved questions to the child.
- Runtimes such as Hermes isolate child context: put those graph findings in the `context` argument to `delegate_task`; do not assume the child inherits MCP access or the parent's conversation.
- A child without MCP tools must not call or claim MCP access. It should work from the supplied evidence and use read/grep on exact source, especially every reported missed-coverage range.

## Quality Analysis
- Dead code: `search_graph(max_degree=0, exclude_entry_points=true)`
- High fan-out/fan-in: `search_graph(min_degree=10, relationship="CALLS")` — `search_graph` has no `direction` parameter; `min_degree`/`max_degree` filter the combined in+out degree, so read the per-row `in`/`out` columns to tell fan-out from fan-in

## 15 MCP Tools
`index_repository`, `index_status`, `list_projects`, `delete_project`,
`search_graph`, `search_code`, `trace_path`, `detect_changes`,
`query_graph`, `get_graph_schema`, `get_code_snippet`, `get_architecture`,
`check_index_coverage`, `manage_adr`, `ingest_traces`

## Edge Types
`get_graph_schema(project=...)` returns the edge types actually present in *that* project with live counts — treat it as the authority and this list as orientation. A type below is absent from a graph whenever the codebase has nothing to emit it.

Structure: `CONTAINS_FOLDER`, `CONTAINS_FILE`, `DEFINES`, `DEFINES_METHOD`, `IMPORTS`, `HAS_BRANCH`
Calls and references: `CALLS`, `CALL_REFERENCE`, `USAGE`, `DATA_FLOWS`
Types: `INHERITS`, `IMPLEMENTS`, `OVERRIDE`, `DECORATES`
Data and effects: `READS`, `WRITES`, `RAISES`, `THROWS`
Service boundaries: `HTTP_CALLS`, `ASYNC_CALLS`, `GRPC_CALLS`, `GRAPHQL_CALLS`, `TRPC_CALLS`, `HANDLES`, `CONFIGURES`, `DEPENDS_ON`, `INFRA_MAPS`
Cross-project (only after multi-repo linking): `CROSS_HTTP_CALLS`, `CROSS_ASYNC_CALLS`, `CROSS_CHANNEL`, `CROSS_GRPC_CALLS`, `CROSS_GRAPHQL_CALLS`, `CROSS_TRPC_CALLS`
Tests, docs, similarity: `TESTS`, `TESTS_FILE`, `DOCUMENTS`, `SIMILAR_TO`, `SEMANTICALLY_RELATED`, `FILE_CHANGES_WITH`

`RAISES` vs `THROWS` and `READS` vs `WRITES` are each one pass choosing between two names, so query both sides of a pair. There is no `CONTAINS_PACKAGE` edge — upstream's help text lists one, but nothing in the engine ever creates it.

## Cypher Examples (for query_graph)
```
MATCH (a)-[r:HTTP_CALLS]->(b) RETURN a.name, b.name, r.url_path, r.confidence LIMIT 20
MATCH (f:Function) WHERE f.name =~ '.*Handler.*' RETURN f.name, f.file_path
MATCH (a)-[r:CALLS]->(b) WHERE a.name = 'main' RETURN b.name
```

## Gotchas
1. `search_graph(relationship="HTTP_CALLS")` filters nodes by degree — use `query_graph` with Cypher to see actual edges.
2. `query_graph` has a 100k row ceiling — add a Cypher `LIMIT` for broad queries or use `search_graph` pagination.
3. `trace_path` needs exact names — use `search_graph(name_pattern=...)` first.
4. `trace_path(direction="outbound")` misses cross-service callers — use `direction="both"`. Only `trace_path` takes `direction`; `search_graph` has no such parameter.
5. `search_graph` results default to 50 per page — check `has_more` and use `offset`.
