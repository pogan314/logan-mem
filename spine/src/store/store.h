/*
 * store.h — Opaque SQLite graph store for code knowledge graphs.
 *
 * All functions are prefixed lsm_store_*. The store handle is opaque —
 * callers never touch SQLite internals directly.
 *
 * Thread safety: a single store handle must not be used concurrently.
 * Use one store per thread or external synchronization.
 */
#ifndef LSM_STORE_H
#define LSM_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Opaque handle ──────────────────────────────────────────────── */

typedef struct lsm_store lsm_store_t;

/* ── Result codes ───────────────────────────────────────────────── */

#define LSM_STORE_OK 0
#define LSM_STORE_ERR (-1)
#define LSM_STORE_NOT_FOUND (-2)

/* ── Data structures ────────────────────────────────────────────── */

typedef struct {
    int64_t id;
    const char *project;
    const char *label;          /* Function, Class, Method, Module, File, ... */
    const char *name;           /* short name */
    const char *qualified_name; /* full dotted path */
    const char *file_path;      /* relative file path */
    int start_line;
    int end_line;
    const char *properties_json; /* JSON string, NULL → "{}" */
} lsm_node_t;

typedef struct {
    int64_t id;
    const char *project;
    int64_t source_id;
    int64_t target_id;
    const char *type;            /* CALLS, HTTP_CALLS, IMPORTS, ... */
    const char *properties_json; /* JSON string, NULL → "{}" */
} lsm_edge_t;

typedef struct {
    const char *name;
    const char *indexed_at; /* ISO 8601 */
    const char *root_path;
} lsm_project_t;

typedef struct {
    const char *project;
    const char *rel_path;
    const char *sha256;
    int64_t mtime_ns;
    int64_t size;
} lsm_file_hash_t;

/* One file's persisted LSP surface: the serialized cross-file definition set
 * (exactly what pass_lsp_cross registration consumes) plus the metadata the
 * closure-repair incremental route needs to decide and bound its work. The
 * store treats defs_json/ref_bloom as opaque; the codec lives with
 * pass_lsp_cross, which is the only writer and reader of their contents. */
typedef struct {
    const char *project;
    const char *rel_path;
    const char *surface_sha; /* sha256 hex of defs_json (the early-cutoff key) */
    const char *defs_json;   /* canonical JSON array of the file's LSP defs */
    const void *ref_bloom;   /* referenced-identifier bloom blob (may be NULL) */
    int ref_bloom_len;
    const char *config_ctx; /* governing-config context hash ("" = none) */
} lsm_lsp_surface_row_t;

/* Find nodes overlapping a line range in a file (excludes Module/Package). */
int lsm_store_find_nodes_by_file_overlap(lsm_store_t *s, const char *project, const char *file_path,
                                         int start_line, int end_line, lsm_node_t **out,
                                         int *count);

/* Find nodes whose qualified_name ends with the given suffix (dot-boundary). */
int lsm_store_find_nodes_by_qn_suffix(lsm_store_t *s, const char *project, const char *suffix,
                                      lsm_node_t **out, int *count);

/* Get CALLS degree of a node (inbound and outbound). */
void lsm_store_node_degree(lsm_store_t *s, int64_t node_id, int *in_deg, int *out_deg);

/* Get distinct file paths for a project. Caller must free each out[i] and out itself.
 * Returns LSM_STORE_OK or LSM_STORE_ERR. */
int lsm_store_list_files(lsm_store_t *s, const char *project, char ***out, int *count);

/* Get caller/callee names for a node (CALLS/HTTP_CALLS/ASYNC_CALLS edges).
 * Returns 0 on success. Caller must free each out_callers[i]/out_callees[i]
 * and the arrays themselves. */
int lsm_store_node_neighbor_names(lsm_store_t *s, int64_t node_id, int limit, char ***out_callers,
                                  int *caller_count, char ***out_callees, int *callee_count);

/* Batch count in/out degree for multiple nodes.
 * edge_type: filter by edge type (e.g. "CALLS"), or NULL/"" for all types.
 * out_in[i] and out_out[i] receive the in/out degree for node_ids[i].
 * Returns LSM_STORE_OK or LSM_STORE_ERR. */
int lsm_store_batch_count_degrees(lsm_store_t *s, const int64_t *node_ids, int id_count,
                                  const char *edge_type, int *out_in, int *out_out);

/* Upsert file hashes in batch. */
int lsm_store_upsert_file_hash_batch(lsm_store_t *s, const lsm_file_hash_t *hashes, int count);

/* ── LSP surface rows (closure-repair incremental) ───────────────
 * Upsert/get/delete are whole-row, keyed (project, rel_path). get returns
 * heap rows released with lsm_store_free_lsp_surfaces. A project with no
 * rows returns OK with *count == 0 — callers treat that as "no surface
 * data" and route to a full rebuild, which is also the upgrade path for
 * databases written before this table existed. */
int lsm_store_upsert_lsp_surface_batch(lsm_store_t *s, const lsm_lsp_surface_row_t *rows,
                                       int count);
int lsm_store_get_lsp_surfaces(lsm_store_t *s, const char *project, lsm_lsp_surface_row_t **out,
                               int *count);
int lsm_store_delete_lsp_surfaces(lsm_store_t *s, const char *project);
void lsm_store_free_lsp_surfaces(lsm_lsp_surface_row_t *rows, int count);

/* Reverse-dependency lookup for closure-repair routing: the DISTINCT
 * file_paths of nodes with at least one edge INTO a node of any file in
 * target_files, excluding the target files themselves. This is "who consumed
 * these files' definitions" as recorded by the previous generation — served
 * by idx_edges_target + idx_nodes_file, so cost tracks the result size, not
 * the graph size. out gets a malloc'd array of malloc'd strings; free with
 * lsm_store_free_dependent_files. */
int lsm_store_get_dependent_files(lsm_store_t *s, const char *project,
                                  const char *const *target_files, int target_count, char ***out,
                                  int *out_count);
void lsm_store_free_dependent_files(char **files, int count);

/* Find edges whose properties contain a url_path matching the keyword. */
int lsm_store_find_edges_by_url_path(lsm_store_t *s, const char *project, const char *keyword,
                                     lsm_edge_t **out, int *count);

/* Restore database from another store (backup API). */
int lsm_store_restore_from(lsm_store_t *dst, lsm_store_t *src);

/* Copy a transactionally-consistent snapshot, including committed WAL frames,
 * from an existing DB into a same-directory staging path. */
int lsm_store_backup_path(const char *source_path, const char *staging_path);

/* Seal a staging DB into one self-contained main file before atomic publish.
 * The store must have no concurrent users. */
int lsm_store_prepare_for_publish(lsm_store_t *s);

/* Checkpoint and detach sidecars from an existing destination immediately
 * before replacement. Fails closed while another process prevents sealing. */
int lsm_store_prepare_path_for_replace(const char *path);

/* ── Search ─────────────────────────────────────────────────────── */

typedef struct {
    const char *project;
    const char *label;        /* NULL = any label */
    const char *name_pattern; /* regex on name, NULL = any */
    const char *qn_pattern;   /* regex on qualified_name, NULL = any */
    const char *file_pattern; /* glob on file_path, NULL = any */
    const char *relationship; /* edge type filter, NULL = any */
    const char *direction;    /* "inbound" / "outbound" / "any", NULL = any */
    int min_degree;           /* -1 = no filter (default), 0+ = minimum */
    int max_degree;           /* -1 = no filter (default), 0+ = maximum */
    int limit;                /* 0 = default (10) */
    int offset;
    bool exclude_entry_points;
    bool include_connected;
    const char *sort_by; /* "relevance" / "name" / "degree", NULL = relevance */
    bool case_sensitive;
    const char **exclude_labels; /* NULL-terminated array, or NULL */
} lsm_search_params_t;

typedef struct {
    lsm_node_t node;
    int in_degree;
    int out_degree;
    /* connected_names: allocated array of strings, count in connected_count */
    const char **connected_names;
    int connected_count;
} lsm_search_result_t;

typedef struct {
    lsm_search_result_t *results;
    int count;
    int total; /* total before pagination */
} lsm_search_output_t;

/* ── Traversal ──────────────────────────────────────────────────── */

typedef struct {
    lsm_node_t node;
    int hop; /* BFS depth from root */
} lsm_node_hop_t;

typedef struct {
    const char *from_name;
    const char *to_name;
    const char *type;
    double confidence;
    int64_t source_id; /* edge endpoints — let callers match an edge to a hop node */
    int64_t target_id;
    const char *properties_json; /* raw edge properties (carries CALLS arg expressions) */
} lsm_edge_info_t;

typedef struct {
    lsm_node_t root;
    lsm_node_hop_t *visited;
    int visited_count;
    lsm_edge_info_t *edges;
    int edge_count;
} lsm_traverse_result_t;

/* ── Schema introspection ───────────────────────────────────────── */

typedef struct {
    const char *label;
    int count;
    char **properties; /* distinct property keys for this label (base + JSON) */
    int property_count;
} lsm_label_count_t;

typedef struct {
    const char *type;
    int count;
    char **properties; /* distinct property keys for this edge type (base + JSON) */
    int property_count;
} lsm_type_count_t;

typedef struct {
    lsm_label_count_t *node_labels;
    int node_label_count;
    lsm_type_count_t *edge_types;
    int edge_type_count;
    /* relationship patterns like "(Function)-[CALLS]->(Function) [123x]" */
    const char **rel_patterns;
    int rel_pattern_count;
    const char **sample_func_names;
    int sample_func_count;
    const char **sample_class_names;
    int sample_class_count;
    const char **sample_qns;
    int sample_qn_count;
} lsm_schema_info_t;

/* ── Lifecycle ──────────────────────────────────────────────────── */

/* Open an in-memory database (for testing). */
lsm_store_t *lsm_store_open_memory(void);

/* Open a file-backed database at the given path. Creates if needed. */
lsm_store_t *lsm_store_open_path(const char *db_path);

/* Open an existing file-backed database read-write without CREATE. Intended
 * for coordinated mutations where a missing/typo path must never materialize
 * a ghost database. Returns NULL when the file does not exist. */
lsm_store_t *lsm_store_open_path_existing(const char *db_path);

/* Open an existing file-backed database for querying only. Opened READ-ONLY
 * (no SQLITE_OPEN_CREATE, no write pragmas) so queries never mutate the DB and
 * work on a read-only file / filesystem. Returns NULL if the file does not
 * exist — never creates a new .db file. */
lsm_store_t *lsm_store_open_path_query(const char *db_path);

/* Validate and seal an existing DB for atomic replacement without creating or
 * migrating its schema. Returns OK when sealed, NOT_FOUND when the bytes are
 * definitely corrupt/incompatible and should be quarantined, or ERR when the
 * file is busy/unavailable and must be left in place. */
int lsm_store_seal_existing_path_for_replace(const char *db_path);

/* On-disk path of a file-backed store, or NULL for an in-memory (:memory:)
 * store. The returned pointer is owned by the store. */
const char *lsm_store_db_path(const lsm_store_t *s);

/* Check database integrity. Returns true if the DB passes basic sanity checks
 * (projects table has correct types, no corruption indicators).
 * Returns false if corruption is detected — caller should delete and re-index. */
bool lsm_store_check_integrity(lsm_store_t *s);
/* Shallow check + PRAGMA quick_check — catches page-level corruption.
 * O(db size); use on rare paths (artifact import), not hot opens. */
bool lsm_store_check_integrity_deep(lsm_store_t *s);

/* Outcome of a quarantine-grade integrity check. Used to decide whether a DB
 * that failed the cheap open-time check should be quarantined (renamed to
 * .corrupt and rebuilt) or left alone. See lsm_store_check_integrity_verdict. */
typedef enum {
    LSM_INTEGRITY_OK = 0,        /* DB is healthy */
    LSM_INTEGRITY_CORRUPT = 1,   /* DB is structurally damaged — safe to quarantine */
    LSM_INTEGRITY_TRANSIENT = 2, /* SQL/busy/IO error — NOT corruption, do NOT quarantine */
} lsm_integrity_verdict_t;

/* Full integrity verdict for the quarantine decision path.
 *
 * The plain lsm_store_check_integrity() returns a single bool and cannot
 * distinguish "the projects table has 99 rows" (real corruption) from
 * "sqlite3_prepare_v2 returned SQLITE_BUSY because another instance held the
 * writer lock" (a transient lock contention, #1206). Quarantining on the latter
 * is what makes concurrent MCP instances destroy each other's healthy DBs.
 *
 * This function runs the shallow check, then PRAGMA quick_check, and classifies
 * the failure mode so the caller can quarantine ONLY on confirmed corruption.
 * O(db size); use only on the recovery/quarantine path, not hot opens. */
lsm_integrity_verdict_t lsm_store_check_integrity_verdict(lsm_store_t *s);

/* Open database for a named project in the default cache dir. */
lsm_store_t *lsm_store_open(const char *project);

/* Close the store and free all resources. NULL-safe. */
void lsm_store_close(lsm_store_t *s);

/* Get the underlying sqlite3 handle (for testing only). */
struct sqlite3 *lsm_store_get_db(lsm_store_t *s);

/* Get the last error message (static string, valid until next call). */
const char *lsm_store_error(lsm_store_t *s);

/* ── Transaction ────────────────────────────────────────────────── */

/* Begin a transaction. Returns LSM_STORE_OK on success. */
int lsm_store_begin(lsm_store_t *s);

/* Commit the current transaction. */
int lsm_store_commit(lsm_store_t *s);

/* Rollback the current transaction. */
int lsm_store_rollback(lsm_store_t *s);

/* ── Bulk write optimization ────────────────────────────────────── */

/* Tune pragmas for bulk write throughput (synchronous=OFF, large cache).
 * WAL journal mode is preserved throughout for crash safety. */
int lsm_store_begin_bulk(lsm_store_t *s);

/* Restore normal pragmas (synchronous=NORMAL, default cache) after bulk writes. */
int lsm_store_end_bulk(lsm_store_t *s);

/* Drop user indexes for faster bulk inserts. */
int lsm_store_drop_indexes(lsm_store_t *s);

/* Recreate user indexes after bulk inserts. */
int lsm_store_create_indexes(lsm_store_t *s);

/* ── WAL / Checkpoint ───────────────────────────────────────────── */

/* Force WAL checkpoint + PRAGMA optimize. */
int lsm_store_checkpoint(lsm_store_t *s);

/* #1083: the WAL size limit (journal_size_limit) applied to this write
 * connection, in bytes; -1 = unlimited (SQLite default / pre-fix). */
int64_t lsm_store_journal_size_limit(lsm_store_t *s);

/* Opaque store generation for pagination-cursor staleness detection:
 * "u<db_uid>g<mutation_gen>" — db_uid is minted per DB file, mutation_gen
 * bumps on every index run. "legacy" for DBs predating store_meta. */
int lsm_store_generation(lsm_store_t *s, char *buf, size_t bufsz);

/* Seal a fully-written staging database before atomic publication.
 * Raises synchronous to FULL, requires an exclusive TRUNCATE checkpoint to
 * complete, then leaves the database in verified DELETE journal mode so the
 * main file is self-contained (no required -wal/-shm sidecars). This is a
 * fail-closed operation: SQLITE_BUSY and an unconfirmed mode transition are
 * errors. The caller must own the staging database exclusively. */
int lsm_store_seal_for_atomic_publish(lsm_store_t *s);

/* Resolve the mmap_size pragma value applied to on-disk stores from the
 * LSM_SQLITE_MMAP_SIZE environment variable. Defaults to 67108864 (64 MB)
 * when the variable is unset, malformed, or partially numeric. Negative
 * values clamp to 0 (which disables mmap and reverts to read()/pread()
 * I/O — recoverable SQLITE_IOERR instead of SIGBUS when concurrent
 * processes truncate the DB file under live mappings). Exposed for
 * testability. */
int64_t lsm_store_resolve_mmap_size(void);

/* ── Dump / Restore ─────────────────────────────────────────────── */

/* Dump in-memory database to a file. */
int lsm_store_dump_to_file(lsm_store_t *s, const char *dest_path);

/* ── Project CRUD ───────────────────────────────────────────────── */

int lsm_store_upsert_project(lsm_store_t *s, const char *name, const char *root_path);
int lsm_store_get_project(lsm_store_t *s, const char *name, lsm_project_t *out);
int lsm_store_list_projects(lsm_store_t *s, lsm_project_t **out, int *count);
int lsm_store_delete_project(lsm_store_t *s, const char *name);

/* ── Node CRUD ──────────────────────────────────────────────────── */

/* Upsert a single node. Returns node ID (>0) or LSM_STORE_ERR. */
int64_t lsm_store_upsert_node(lsm_store_t *s, const lsm_node_t *n);

/* Upsert nodes in batch. out_ids must have room for count entries. */
int lsm_store_upsert_node_batch(lsm_store_t *s, const lsm_node_t *nodes, int count,
                                int64_t *out_ids);

/* Find node by primary key. Returns LSM_STORE_OK or LSM_STORE_NOT_FOUND. */
int lsm_store_find_node_by_id(lsm_store_t *s, int64_t id, lsm_node_t *out);

/* Find node by project + qualified_name. */
int lsm_store_find_node_by_qn(lsm_store_t *s, const char *project, const char *qn, lsm_node_t *out);

/* Find node by qualified_name only (no project filter — QNs are globally unique). */
int lsm_store_find_node_by_qn_any(lsm_store_t *s, const char *qn, lsm_node_t *out);

/* Find all nodes in a project. Returns allocated array, caller frees. */
int lsm_store_find_nodes(lsm_store_t *s, const char *project, lsm_node_t **out, int *count);

/* Find nodes by name (exact match). Returns allocated array, caller frees. */
int lsm_store_find_nodes_by_name(lsm_store_t *s, const char *project, const char *name,
                                 lsm_node_t **out, int *count);

/* Find nodes by name across all projects. Returns allocated array, caller frees. */
int lsm_store_find_nodes_by_name_any(lsm_store_t *s, const char *name, lsm_node_t **out,
                                     int *count);

/* Find nodes by label. */
int lsm_store_find_nodes_by_label(lsm_store_t *s, const char *project, const char *label,
                                  lsm_node_t **out, int *count);

/* Find nodes by file path. */
int lsm_store_find_nodes_by_file(lsm_store_t *s, const char *project, const char *file_path,
                                 lsm_node_t **out, int *count);

/* Batch lookup: map qualified names → node IDs.
 * qns[i] is resolved; out_ids[i] receives the ID or 0 if not found.
 * Returns number of QNs actually found, or LSM_STORE_ERR. */
int lsm_store_find_node_ids_by_qns(lsm_store_t *s, const char *project, const char **qns,
                                   int qn_count, int64_t *out_ids);

/* Count nodes in project. Returns count or LSM_STORE_ERR. */
int lsm_store_count_nodes(lsm_store_t *s, const char *project);

int lsm_store_count_nodes_scoped(lsm_store_t *s, const char *project, const char *path);

int lsm_store_count_edges_scoped(lsm_store_t *s, const char *project, const char *path);

/* True when path is a non-empty scope after normalization (issue #604). */
bool lsm_store_arch_path_scoped(const char *path);

/* When scoped, writes normalized directory prefix into norm_out. Returns false if unscoped. */
bool lsm_store_normalize_arch_path(const char *path, char *norm_out, size_t norm_sz);

/* True when architecture aspect `name` belongs to the "overview" subset:
 * every aspect EXCEPT the large per-file listing (file_tree). Shared by both
 * aspect gates — want_aspect (store.c) and aspect_wanted (mcp.c) — so the
 * two sites cannot drift. */
bool lsm_store_arch_aspect_in_overview(const char *name);

/* Delete all nodes for a project (cascade deletes edges). */
int lsm_store_delete_nodes_by_project(lsm_store_t *s, const char *project);

/* Delete nodes by file path. */
int lsm_store_delete_nodes_by_file(lsm_store_t *s, const char *project, const char *file_path);

/* Delete nodes by label. */
int lsm_store_delete_nodes_by_label(lsm_store_t *s, const char *project, const char *label);

/* ── Edge CRUD ──────────────────────────────────────────────────── */

/* Insert or update edge. Returns edge ID (>0) or LSM_STORE_ERR. */
int64_t lsm_store_insert_edge(lsm_store_t *s, const lsm_edge_t *e);

/* Insert edges in batch. */
int lsm_store_insert_edge_batch(lsm_store_t *s, const lsm_edge_t *edges, int count);

/* Fetch all CALLS edges among Function/Method nodes for a project as parallel
 * (source_id, target_id) arrays (caller frees both). For SCC / cycle analysis.
 * Stops at max_edges and sets *truncated — never a silent cap. Returns
 * LSM_STORE_OK (or _ERR); *count is the number returned. */
int lsm_store_fetch_call_edges(lsm_store_t *s, const char *project, int max_edges,
                               int64_t **out_src, int64_t **out_tgt, int *count, bool *truncated);

/* Find edges by source node. */
int lsm_store_find_edges_by_source(lsm_store_t *s, int64_t source_id, lsm_edge_t **out, int *count);

/* Find edges by target node. */
int lsm_store_find_edges_by_target(lsm_store_t *s, int64_t target_id, lsm_edge_t **out, int *count);

/* Find edges by source + type. */
int lsm_store_find_edges_by_source_type(lsm_store_t *s, int64_t source_id, const char *type,
                                        lsm_edge_t **out, int *count);

/* Find edges by target + type. */
int lsm_store_find_edges_by_target_type(lsm_store_t *s, int64_t target_id, const char *type,
                                        lsm_edge_t **out, int *count);

/* Find all edges of a type in project. */
int lsm_store_find_edges_by_type(lsm_store_t *s, const char *project, const char *type,
                                 lsm_edge_t **out, int *count);

/* Count all edges in project. */
int lsm_store_count_edges(lsm_store_t *s, const char *project);

/* Count edges of given type. */
int lsm_store_count_edges_by_type(lsm_store_t *s, const char *project, const char *type);

/* Delete all edges for a project. */
int lsm_store_delete_edges_by_project(lsm_store_t *s, const char *project);

/* Delete edges by type. */
int lsm_store_delete_edges_by_type(lsm_store_t *s, const char *project, const char *type);

/* ── File hash CRUD ─────────────────────────────────────────────── */

int lsm_store_upsert_file_hash(lsm_store_t *s, const char *project, const char *rel_path,
                               const char *sha256, int64_t mtime_ns, int64_t size);

int lsm_store_get_file_hashes(lsm_store_t *s, const char *project, lsm_file_hash_t **out,
                              int *count);

/* Fetch one exact file-hash record. The returned strings are heap-owned and
 * must be released with lsm_store_clear_file_hash(). */
int lsm_store_get_file_hash(lsm_store_t *s, const char *project, const char *rel_path,
                            lsm_file_hash_t *out);

/* Free heap-owned fields in one exact file-hash record and zero it. */
void lsm_store_clear_file_hash(lsm_file_hash_t *hash);

int lsm_store_delete_file_hash(lsm_store_t *s, const char *project, const char *rel_path);

int lsm_store_delete_file_hashes(lsm_store_t *s, const char *project);

/* ── Index coverage (#963) ──────────────────────────────────────── */

/* One best-effort coverage row: a file the indexer could not fully cover.
 * kind "parse_partial" = indexed but the parse tree had ERROR/MISSING regions
 * (detail = 1-based line ranges "12-40,88-90"); skip kinds "read"/"extract"/
 * "oversized" = not indexed at all (detail = reason). Stored in the separate
 * index_coverage table — coverage is metadata ABOUT the graph, never mixed
 * into the graph itself. */
typedef struct {
    const char *rel_path;
    const char *kind;
    const char *detail;
} lsm_coverage_row_t;

/* Metadata describing how completely one index run recorded the best-effort
 * coverage signal. `recording_status` is "complete", "truncated", or
 * "unavailable"; it is deliberately separate from hash_records_complete.
 * Strings returned by lsm_store_coverage_meta_get are heap-owned. */
typedef struct {
    const char *project;
    const char *generation;
    const char *index_mode;
    const char *recorded_at;
    const char *recording_status;
    int ignored_files_stored;
    int ignored_files_total;
    int coverage_version;
    bool hash_records_complete;
} lsm_coverage_meta_t;

/* Replace the project's coverage rows in one transaction, then prune rows for
 * files absent from file_hashes (deleted from the repo). Call AFTER hashes
 * were persisted for the run. */
int lsm_store_coverage_replace(lsm_store_t *s, const char *project, const lsm_coverage_row_t *rows,
                               int count);

/* Replace coverage rows and their run metadata atomically. Passing NULL meta
 * clears any older metadata so it cannot be mistaken for the new row set. */
int lsm_store_coverage_replace_ex(lsm_store_t *s, const char *project,
                                  const lsm_coverage_row_t *rows, int count,
                                  const lsm_coverage_meta_t *meta);

/* Fetch all coverage rows (ordered by rel_path). Caller frees via
 * lsm_store_free_coverage. */
int lsm_store_coverage_get(lsm_store_t *s, const char *project, lsm_coverage_row_t **out,
                           int *count);

/* Fetch coverage rows for one path. Exact rows are returned together with any
 * not_indexed_dir ancestor that covers the path. */
int lsm_store_coverage_get_path(lsm_store_t *s, const char *project, const char *rel_path,
                                lsm_coverage_row_t **out, int *count);

/* Fetch coverage rows at/below a directory scope, plus a not_indexed_dir
 * ancestor that covers the scope. Prefix matching is segment-boundary safe. */
int lsm_store_coverage_get_scope(lsm_store_t *s, const char *project, const char *scope,
                                 lsm_coverage_row_t **out, int *count);

/* Fetch/free the metadata paired with the current coverage row set. */
int lsm_store_coverage_meta_get(lsm_store_t *s, const char *project, lsm_coverage_meta_t *out);
void lsm_store_coverage_meta_clear(lsm_coverage_meta_t *meta);

/* Name of the derived miss-graph shadow project ("<project>::missed").
 * lsm_store_coverage_replace materializes the coverage rows as a file-
 * structure graph (Project → Folder → File{kind, detail}) under this project
 * name — queryable via the normal cypher path without touching the real
 * project's graph. */
void lsm_store_coverage_shadow_project(char *dst, size_t dstsz, const char *project);

void lsm_store_free_coverage(lsm_coverage_row_t *rows, int count);

/* ── Search ─────────────────────────────────────────────────────── */

int lsm_store_search(lsm_store_t *s, const lsm_search_params_t *params, lsm_search_output_t *out);

/* Free a search output's allocated memory. */
void lsm_store_search_free(lsm_search_output_t *out);

/* ── Traversal ──────────────────────────────────────────────────── */

int lsm_store_bfs(lsm_store_t *s, int64_t start_id, const char *direction, const char **edge_types,
                  int edge_type_count, int max_depth, int max_results, lsm_traverse_result_t *out);

/* Multi-source BFS from ALL seed ids at once (one CTE, temp-table anchored).
 * Seeds are EXCLUDED from the result (impact semantics); MIN(hop) across the
 * seed set; canonical (hop,id) order; *truncated set when the max_results
 * memory-safety ceiling was hit (counting is otherwise uncapped). */
int lsm_store_bfs_multi(lsm_store_t *s, const int64_t *seed_ids, int seed_count,
                        const char *direction, const char **edge_types, int edge_type_count,
                        int max_depth, int max_results, lsm_traverse_result_t *out,
                        bool *truncated);

/* Free a traverse result's allocated memory. */
void lsm_store_traverse_free(lsm_traverse_result_t *out);

/* ── Impact analysis ────────────────────────────────────────────── */

typedef enum {
    LSM_RISK_CRITICAL = 0,
    LSM_RISK_HIGH = 1,
    LSM_RISK_MEDIUM = 2,
    LSM_RISK_LOW = 3,
} lsm_risk_level_t;

/* Map BFS hop depth to risk level. */
lsm_risk_level_t lsm_hop_to_risk(int hop);

/* String representation of risk level. */
const char *lsm_risk_label(lsm_risk_level_t level);

typedef struct {
    int critical;
    int high;
    int medium;
    int low;
    int total;
    bool has_cross_service;
} lsm_impact_summary_t;

/* Build impact summary from visited hops and edges. */
lsm_impact_summary_t lsm_build_impact_summary(const lsm_node_hop_t *hops, int hop_count,
                                              const lsm_edge_info_t *edges, int edge_count);

/* Deduplicate BFS hops, keeping minimum hop per node ID.
 * Returns allocated array and count via out params. Caller frees result. */
int lsm_deduplicate_hops(const lsm_node_hop_t *hops, int hop_count, lsm_node_hop_t **out,
                         int *out_count);

/* ── Schema ─────────────────────────────────────────────────────── */

int lsm_store_get_schema(lsm_store_t *s, const char *project, lsm_schema_info_t *out);

/* Like lsm_store_get_schema but skips per-label/per-type JSON property-key
 * discovery (json_each scans over every row) — for callers that only need
 * label/type counts, e.g. get_architecture. */
int lsm_store_get_schema_counts(lsm_store_t *s, const char *project, lsm_schema_info_t *out);

int lsm_store_get_schema_counts_scoped(lsm_store_t *s, const char *project, const char *path,
                                       lsm_schema_info_t *out);

/* Free a schema info's allocated memory. */
void lsm_store_schema_free(lsm_schema_info_t *out);

/* ── Architecture ───────────────────────────────────────────────── */

typedef struct {
    const char *language;
    int file_count;
} lsm_language_count_t;

typedef struct {
    const char *name;
    int node_count;
    int fan_in;
    int fan_out;
} lsm_package_summary_t;

typedef struct {
    const char *name;
    const char *qualified_name;
    const char *file;
} lsm_entry_point_t;

typedef struct {
    const char *method;
    const char *path;
    const char *handler;
} lsm_route_info_t;

typedef struct {
    const char *name;
    const char *qualified_name;
    int fan_in;
} lsm_hotspot_t;

typedef struct {
    const char *from;
    const char *to;
    int call_count;
} lsm_cross_pkg_boundary_t;

typedef struct {
    const char *from;
    const char *to;
    const char *type;
    int count;
} lsm_service_link_t;

typedef struct {
    const char *name;
    const char *layer;
    const char *reason;
} lsm_package_layer_t;

typedef struct {
    int id;
    const char *label;
    int members;
    double cohesion;
    const char **top_nodes;
    int top_node_count;
    const char **packages;
    int package_count;
    const char **edge_types;
    int edge_type_count;
} lsm_cluster_info_t;

typedef struct {
    const char *path;
    const char *type; /* "dir" or "file" */
    int children;
} lsm_file_tree_entry_t;

typedef struct {
    /* Pointers first to minimize padding */
    lsm_language_count_t *languages;
    lsm_package_summary_t *packages;
    lsm_entry_point_t *entry_points;
    lsm_route_info_t *routes;
    lsm_hotspot_t *hotspots;
    lsm_cross_pkg_boundary_t *boundaries;
    lsm_service_link_t *services;
    lsm_package_layer_t *layers;
    lsm_cluster_info_t *clusters;
    lsm_file_tree_entry_t *file_tree;
    /* Counts after pointers */
    int language_count;
    int package_count;
    int entry_point_count;
    int route_count;
    int hotspot_count;
    int boundary_count;
    int service_count;
    int layer_count;
    int cluster_count;
    int file_tree_count;
} lsm_architecture_info_t;

int lsm_store_get_architecture(lsm_store_t *s, const char *project, const char *path,
                               const char **aspects, int aspect_count,
                               lsm_architecture_info_t *out);
void lsm_store_architecture_free(lsm_architecture_info_t *out);

/* ── ADR (Architecture Decision Record) ────────────────────────── */

#define LSM_ADR_MAX_LENGTH 8000

typedef struct {
    const char *project;
    const char *content;
    const char *created_at;
    const char *updated_at;
} lsm_adr_t;

int lsm_store_adr_store(lsm_store_t *s, const char *project, const char *content);
int lsm_store_adr_get(lsm_store_t *s, const char *project, lsm_adr_t *out);
int lsm_store_adr_delete(lsm_store_t *s, const char *project);
int lsm_store_adr_update_sections(lsm_store_t *s, const char *project, const char **keys,
                                  const char **values, int count, lsm_adr_t *out);
void lsm_store_adr_free(lsm_adr_t *adr);

/* ADR section parsing/rendering (pure functions, no store needed) */

enum { PROPS_MAX = 16 };

typedef struct {
    char *keys[PROPS_MAX];
    char *values[PROPS_MAX];
    int count;
} lsm_adr_sections_t;

lsm_adr_sections_t lsm_adr_parse_sections(const char *content);
char *lsm_adr_render(const lsm_adr_sections_t *sections);
int lsm_adr_validate_content(const char *content, char *errbuf, int errbuf_size);
int lsm_adr_validate_section_keys(const char **keys, int count, char *errbuf, int errbuf_size);
void lsm_adr_sections_free(lsm_adr_sections_t *s);

/* ── Search helpers (exposed for testing) ───────────────────────── */

/* Convert a glob pattern to SQL LIKE pattern. Caller must free result. */
char *lsm_glob_to_like(const char *pattern);

/* Extract literal substrings (>= 3 chars) from a regex pattern for LIKE pre-filtering.
 * Bails on alternation (|). Returns count of hints written to out[].
 * Each out[i] is malloc'd — caller must free each string. */
int lsm_extract_like_hints(const char *pattern, char **out, int max_out);

/* Prepend (?i) to a regex pattern if not already present.
 * Returns a static buffer — do NOT free. */
const char *lsm_ensure_case_insensitive(const char *pattern);

/* Strip leading (?i) from a regex pattern.
 * Returns a static buffer — do NOT free. */
const char *lsm_strip_case_flag(const char *pattern);

/* ── Architecture helpers (exposed for testing) ────────────────── */

const char *lsm_qn_to_package(const char *qn);
const char *lsm_qn_to_top_package(const char *qn);
bool lsm_is_test_file_path(const char *fp);
int lsm_store_find_architecture_docs(lsm_store_t *s, const char *project, char ***out, int *count);

/* ── Community detection (Leiden) ──────────────────────────────── */

typedef struct {
    int64_t src;
    int64_t dst;
} lsm_louvain_edge_t;

typedef struct {
    int64_t node_id;
    int community;
} lsm_louvain_result_t;

/* Multi-level Leiden community detection (Traag, Waltman & van Eck 2019,
 * arXiv:1810.08473): local moving + refinement + aggregation, repeated until
 * the partition can no longer be coarsened. Refinement guarantees every
 * reported community is internally connected. The resolution parameter
 * controls granularity (higher -> more, smaller communities); 1.0 is standard.
 * Allocates *out (length *out_count == node_count); the caller frees it. */
int lsm_leiden(const int64_t *nodes, int node_count, const lsm_louvain_edge_t *edges,
               int edge_count, double resolution, lsm_louvain_result_t **out, int *out_count);

/* Convenience wrapper: lsm_leiden with resolution 1.0. */
int lsm_louvain(const int64_t *nodes, int node_count, const lsm_louvain_edge_t *edges,
                int edge_count, lsm_louvain_result_t **out, int *out_count);

/* ── Memory management helpers ──────────────────────────────────── */

/* Free heap-allocated strings in a stack-allocated node (does NOT free the node itself). */
void lsm_node_free_fields(lsm_node_t *n);

/* Free heap-allocated strings in a stack-allocated project (does NOT free the project itself). */
void lsm_project_free_fields(lsm_project_t *p);

/* Free an array of nodes returned by find_nodes_by_* functions. */
void lsm_store_free_nodes(lsm_node_t *nodes, int count);

/* Free an array of edges returned by find_edges_by_* functions. */
void lsm_store_free_edges(lsm_edge_t *edges, int count);

/* Free an array of projects. */
void lsm_store_free_projects(lsm_project_t *projects, int count);

/* Free an array of file hashes. */
void lsm_store_free_file_hashes(lsm_file_hash_t *hashes, int count);

/* ── Vector search ───────────────────────────────────────────────── */

/* Result from vector similarity search. */
typedef struct {
    int64_t node_id;
    char *name;
    char *qualified_name;
    char *file_path;
    char *label;
    double score;
} lsm_vector_result_t;

/* Search for nodes similar to the given query keywords using stored RI vectors.
 * Builds a merged query vector from the keywords, then does cosine scan via
 * the lsm_cosine_i8 SQL function joined with the nodes table.
 * Returns results sorted by score DESC. Caller must free with lsm_store_free_vector_results. */
int lsm_store_vector_search(lsm_store_t *s, const char *project, const char **keywords,
                            int keyword_count, int limit, lsm_vector_result_t **out,
                            int *out_count);

/* Free vector search results. */
void lsm_store_free_vector_results(lsm_vector_result_t *results, int count);

/* Count vectors for a project. */
int lsm_store_count_vectors(lsm_store_t *s, const char *project);

/* Execute an arbitrary SQL statement (pragmas, FTS5 maintenance, etc).
 * Returns LSM_STORE_OK on success. */
int lsm_store_exec(lsm_store_t *s, const char *sql);

#endif /* LSM_STORE_H */
