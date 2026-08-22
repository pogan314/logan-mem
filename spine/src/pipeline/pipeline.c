/*
 * pipeline.c — Indexing pipeline orchestrator.
 *
 * Coordinates multi-pass indexing:
 *   1. Discover files
 *   2. Build structure (Project/Folder/Package/File nodes)
 *   3. Bulk load sources (read + LZ4 HC compress)
 *   4. Extract definitions (fused: extract + write nodes + build registry)
 *   5. Resolve imports, calls, usages, semantic edges
 *   6. Post-passes: tests, communities, HTTP links, git history
 *   7. Dump graph buffer to SQLite
 */
#include "foundation/constants.h"

enum { LSM_DIR_PERMS = 0755, PL_RING = 4, PL_RING_MASK = 3, PL_SEQ_PASSES = 6 };
#define PL_NSEC_PER_SEC 1000000000LL
#include "pipeline/pipeline.h"
#include "pipeline/artifact.h"
#include "pipeline/pipeline_internal.h"
#include "pipeline/lsp_surface.h"
#include "pipeline/pass_lsp_cross.h"
#include "pipeline/worker_pool.h"
#include "graph_buffer/graph_buffer.h"
#include "git/git_context.h"
#include "store/store.h"
#include "macro_table.h"
#include "arena.h"
#include "discover/discover.h"
#include "discover/userconfig.h"
#include "foundation/platform.h"
#include "foundation/compat_fs.h"
#include "foundation/log.h"
#include "foundation/str_util.h"
#include "foundation/hash_table.h"
#include "foundation/compat.h"
#include "foundation/compat_thread.h"
#include "foundation/profile.h"
#include "foundation/mem.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#define lsm_pipeline_getpid _getpid
#else
#include <unistd.h>
#define lsm_pipeline_getpid getpid
#endif

static inline void *intptr_to_ptr(intptr_t v) {
    void *p;
    memcpy(&p, &v, sizeof(p));
    return p;
}

/* ── Global index lock ─────────────────────────────────────────── */
/* Prevents concurrent pipeline runs on the same DB file.
 * Atomic spinlock: 0 = free, 1 = locked. */
static atomic_int g_pipeline_busy = 0;

#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
static atomic_bool g_persist_test_fail_after_stage_dump = false;
static atomic_bool g_persist_test_cancel_after_predump = false;
static atomic_bool g_persist_test_cancel_after_destination_prepare = false;
static atomic_bool g_persist_test_fail_adr_capture = false;
static lsm_pipeline_test_hook_fn g_persist_test_before_final_manifest = NULL;
static void *g_persist_test_before_final_manifest_userdata = NULL;

void lsm_pipeline_incremental_test_fail_after_stage_dump_once(void) {
    atomic_store(&g_persist_test_fail_after_stage_dump, true);
}

void lsm_pipeline_incremental_test_cancel_after_predump_once(void) {
    atomic_store(&g_persist_test_cancel_after_predump, true);
}

void lsm_pipeline_incremental_test_cancel_after_destination_prepare_once(void) {
    atomic_store(&g_persist_test_cancel_after_destination_prepare, true);
}

void lsm_pipeline_incremental_test_fail_adr_capture_once(void) {
    atomic_store(&g_persist_test_fail_adr_capture, true);
}

void lsm_pipeline_incremental_test_before_final_manifest_once(lsm_pipeline_test_hook_fn hook,
                                                              void *userdata) {
    g_persist_test_before_final_manifest = hook;
    g_persist_test_before_final_manifest_userdata = userdata;
}

void lsm_pipeline_persist_test_run_before_final_manifest(void) {
    lsm_pipeline_test_hook_fn hook = g_persist_test_before_final_manifest;
    void *userdata = g_persist_test_before_final_manifest_userdata;
    g_persist_test_before_final_manifest = NULL;
    g_persist_test_before_final_manifest_userdata = NULL;
    if (hook) {
        hook(userdata);
    }
}

bool lsm_pipeline_persist_test_take_failure_after_stage_dump(void) {
    return atomic_exchange(&g_persist_test_fail_after_stage_dump, false);
}

bool lsm_pipeline_persist_test_take_cancel_after_predump(void) {
    return atomic_exchange(&g_persist_test_cancel_after_predump, false);
}

bool lsm_pipeline_persist_test_take_cancel_after_destination_prepare(void) {
    return atomic_exchange(&g_persist_test_cancel_after_destination_prepare, false);
}

void lsm_pipeline_persist_test_reset_faults(void) {
    atomic_store(&g_persist_test_fail_after_stage_dump, false);
    atomic_store(&g_persist_test_cancel_after_predump, false);
    atomic_store(&g_persist_test_cancel_after_destination_prepare, false);
    atomic_store(&g_persist_test_fail_adr_capture, false);
    g_persist_test_before_final_manifest = NULL;
    g_persist_test_before_final_manifest_userdata = NULL;
}
#endif

bool lsm_pipeline_try_lock(void) {
    return atomic_exchange(&g_pipeline_busy, 1) == 0;
}

#define LOCK_SPIN_NS 100000000 /* 100ms between lock retries */

void lsm_pipeline_lock(void) {
    while (atomic_exchange(&g_pipeline_busy, 1) != 0) {
        struct timespec ts = {0, LOCK_SPIN_NS};
        lsm_nanosleep(&ts, NULL);
    }
}

void lsm_pipeline_unlock(void) {
    atomic_store(&g_pipeline_busy, 0);
}

/* ── Internal state ──────────────────────────────────────────────── */

struct lsm_pipeline {
    char *repo_path;
    char *db_path;
    char *project_name;
    lsm_git_context_t git_ctx;
    char *branch_qn;
    lsm_index_mode_t requested_mode;
    lsm_index_mode_t mode;
    atomic_int cancelled_storage;
    atomic_int *cancelled;
    bool persistence; /* write .logan-spine/graph.db.zst after indexing */

    /* Indexing state (set during run) */
    lsm_gbuf_t *gbuf;
    lsm_registry_t *registry;

    /* Directory subtrees skipped during discovery (rel paths). Captured from
     * lsm_discover_ex so the MCP layer can report excluded subtrees (#411).
     * Owned by the pipeline; freed in lsm_pipeline_free. */
    char **excluded_dirs;
    int excluded_count;

    /* Individual files dropped by ignore rules during discovery (#963
     * "purposely not indexed" — by design, not failures). Stored entries are
     * capped in discovery; ignored_total keeps the uncapped count so
     * truncation stays explicit. Owned by the pipeline. */
    lsm_ignored_file_t *ignored_files;
    int ignored_count;
    int ignored_total;

    /* Per-file indexing failures (skipped files) surfaced via MCP/CLI/logfile
     * (Stage 2 / Track B). A skip is the expected handled outcome of a bad or
     * oversized file — the run still succeeds ("indexed"). Owned by the
     * pipeline; freed in lsm_pipeline_free. */
    lsm_file_error_t *file_errors;
    int file_errors_count;
    int file_errors_cap;

    /* User-defined extension overrides (loaded once per run) */
    lsm_userconfig_t *userconfig;

    /* Committed graph size at dump time (-1 = dump did not run). #334 gate axis. */
    int committed_nodes;
    int committed_edges;

    /* ADR (project_summaries) captured before a full-reindex DB delete, so it
     * can be restored after the rebuild. NULL when no ADR existed. Issue #516. */
    char *saved_adr;

    /* Per-file LSP surfaces serialized at the collect_all_defs seam (the only
     * moment the result cache is alive), persisted by dump_and_persist_hashes
     * so the closure-repair incremental route can early-cutoff on surface
     * hashes and rehydrate cross registries without re-parsing. Heap rows,
     * released with lsm_store_free_lsp_surfaces in lsm_pipeline_free. NULL
     * when cross-LSP was disabled for the run — the incremental route then
     * finds no rows and correctly falls back to a full rebuild. */
    lsm_lsp_surface_row_t *surface_rows;
    int surface_row_count;

    /* Deterministic test-only seam at the final publication boundary. Kept
     * per pipeline so concurrent test/process activity cannot cross-trigger. */
    void (*before_publish_hook)(lsm_pipeline_t *, const char *, void *);
    void *before_publish_hook_ctx;
    int (*rename_hook)(const char *, const char *, void *);
    void *rename_hook_ctx;
};

/* ── Global pkgmap (one active pipeline at a time) ─────────────── */

static LSMHashTable *g_pkgmap = NULL;

LSMHashTable *lsm_pipeline_get_pkgmap(void) {
    return g_pkgmap;
}

void lsm_pipeline_set_pkgmap(LSMHashTable *map) {
    g_pkgmap = map;
}

/* ── Timing helper ──────────────────────────────────────────────── */

static double elapsed_ms(struct timespec start) {
    struct timespec now;
    lsm_clock_gettime(CLOCK_MONOTONIC, &now);
    return ((double)(now.tv_sec - start.tv_sec) * LSM_MS_PER_SEC) +
           ((double)(now.tv_nsec - start.tv_nsec) / LSM_US_PER_SEC_F);
}

/* Format int to string for logging. Thread-safe via TLS rotating buffers. */
static const char *itoa_buf(int val) {
    static LSM_TLS char bufs[PL_RING][LSM_SZ_32];
    static LSM_TLS int idx = 0;
    int i = idx;
    idx = (idx + SKIP_ONE) & PL_RING_MASK;
    snprintf(bufs[i], sizeof(bufs[i]), "%d", val);
    return bufs[i];
}

/* Log current + peak RSS at a pipeline phase boundary (memory profiling). */
static void log_phase_mem(const char *phase) {
    enum { PL_BYTES_PER_MB = 1024 * 1024 };
    lsm_log_info("mem.phase", "phase", phase, "rss_mb",
                 itoa_buf((int)(lsm_mem_rss() / PL_BYTES_PER_MB)), "peak_mb",
                 itoa_buf((int)(lsm_mem_peak_rss() / PL_BYTES_PER_MB)));
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

lsm_pipeline_t *lsm_pipeline_new(const char *repo_path, const char *db_path,
                                 lsm_index_mode_t mode) {
    if (!repo_path) {
        return NULL;
    }

    lsm_pipeline_t *p = calloc(LSM_ALLOC_ONE, sizeof(lsm_pipeline_t));
    if (!p) {
        return NULL;
    }

    p->repo_path = strdup(repo_path);
    p->db_path = db_path ? strdup(db_path) : NULL;
    p->project_name = lsm_project_name_from_path(repo_path);
    (void)lsm_git_context_resolve(repo_path, &p->git_ctx);
    p->branch_qn = lsm_git_context_branch_qn(p->project_name, &p->git_ctx);
    p->requested_mode = mode;
    p->mode = mode;
    p->persistence = false;
    p->committed_nodes = -1;
    p->committed_edges = -1;
    atomic_init(&p->cancelled_storage, 0);
    p->cancelled = &p->cancelled_storage;

    return p;
}

static int pipeline_refresh_git_context(lsm_pipeline_t *p) {
    lsm_git_context_t fresh = {0};
    if (!p || lsm_git_context_resolve(p->repo_path, &fresh) != 0) {
        lsm_git_context_free(&fresh);
        return LSM_NOT_FOUND;
    }
    char *fresh_branch_qn = lsm_git_context_branch_qn(p->project_name, &fresh);
    if (!fresh_branch_qn) {
        lsm_git_context_free(&fresh);
        return LSM_NOT_FOUND;
    }
    lsm_git_context_free(&p->git_ctx);
    free(p->branch_qn);
    p->git_ctx = fresh;
    p->branch_qn = fresh_branch_qn;
    return 0;
}

void lsm_pipeline_set_persistence(lsm_pipeline_t *p, bool enabled) {
    if (p) {
        p->persistence = enabled;
    }
}

bool lsm_pipeline_set_project_name(lsm_pipeline_t *p, const char *name) {
    if (!p || !name || !name[0]) {
        return false;
    }

    char *normalized = lsm_project_name_from_path(name);
    if (!normalized) {
        return false;
    }
    if (!lsm_validate_project_name(normalized)) {
        free(normalized);
        return false;
    }

    free(p->project_name);
    p->project_name = normalized;
    free(p->branch_qn);
    p->branch_qn = lsm_git_context_branch_qn(p->project_name, &p->git_ctx);
    return true;
}

void lsm_pipeline_set_lsp_surfaces(lsm_pipeline_t *p, lsm_lsp_surface_row_t *rows, int count) {
    if (!p) {
        lsm_store_free_lsp_surfaces(rows, count);
        return;
    }
    lsm_store_free_lsp_surfaces(p->surface_rows, p->surface_row_count);
    p->surface_rows = rows;
    p->surface_row_count = count;
}

void lsm_pipeline_free(lsm_pipeline_t *p) {
    if (!p) {
        return;
    }
    free(p->repo_path);
    free(p->db_path);
    free(p->project_name);
    lsm_discover_free_excluded(p->excluded_dirs, p->excluded_count);
    p->excluded_dirs = NULL;
    p->excluded_count = 0;
    lsm_discover_free_ignored(p->ignored_files, p->ignored_count);
    p->ignored_files = NULL;
    p->ignored_count = 0;
    p->ignored_total = 0;
    for (int i = 0; i < p->file_errors_count; i++) {
        free(p->file_errors[i].path);
        free(p->file_errors[i].reason);
        free(p->file_errors[i].phase);
    }
    free(p->file_errors);
    p->file_errors = NULL;
    p->file_errors_count = 0;
    p->file_errors_cap = 0;
    free(p->branch_qn);
    free(p->saved_adr); /* freed here too: error paths can exit before the
                         * restore in dump_and_persist_hashes runs. Issue #516. */
    p->saved_adr = NULL;
    lsm_store_free_lsp_surfaces(p->surface_rows, p->surface_row_count);
    p->surface_rows = NULL;
    p->surface_row_count = 0;
    lsm_git_context_free(&p->git_ctx);
    /* gbuf, store, registry freed during/after run */
    /* Defensively free userconfig in case run() was never called or panicked */
    if (p->userconfig) {
        lsm_set_user_lang_config(NULL);
        lsm_userconfig_free(p->userconfig);
        p->userconfig = NULL;
    }
    free(p);
}

void lsm_pipeline_cancel(lsm_pipeline_t *p) {
    if (p && p->cancelled) {
        atomic_store(p->cancelled, 1);
    }
}

void lsm_pipeline_bind_cancel_flag(lsm_pipeline_t *p, atomic_int *cancelled) {
    if (p && cancelled) {
        p->cancelled = cancelled;
    }
}

void lsm_pipeline_set_before_publish_hook_for_tests(
    lsm_pipeline_t *p, void (*hook)(lsm_pipeline_t *, const char *, void *), void *ctx) {
    if (p) {
        p->before_publish_hook = hook;
        p->before_publish_hook_ctx = ctx;
    }
}

void lsm_pipeline_set_rename_hook_for_tests(lsm_pipeline_t *p,
                                            int (*hook)(const char *, const char *, void *),
                                            void *ctx) {
    if (p) {
        p->rename_hook = hook;
        p->rename_hook_ctx = ctx;
    }
}

const char *lsm_pipeline_project_name(const lsm_pipeline_t *p) {
    return p ? p->project_name : NULL;
}

const char *lsm_pipeline_repo_path(const lsm_pipeline_t *p) {
    return p ? p->repo_path : NULL;
}

atomic_int *lsm_pipeline_cancelled_ptr(lsm_pipeline_t *p) {
    return p ? p->cancelled : NULL;
}

int lsm_pipeline_get_mode(const lsm_pipeline_t *p) {
    return p ? (int)p->mode : 0;
}

void lsm_pipeline_get_excluded(const lsm_pipeline_t *p, char ***out, int *count) {
    if (out) {
        *out = p ? p->excluded_dirs : NULL;
    }
    if (count) {
        *count = p ? p->excluded_count : 0;
    }
}

/* NULL-safe heap strdup (avoids a strdup dependency + guards NULL inputs). */
static char *fe_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) {
        memcpy(d, s, n);
    }
    return d;
}

void lsm_pipeline_add_file_error(lsm_pipeline_t *p, const char *path, const char *reason,
                                 const char *phase) {
    if (!p) {
        return;
    }
    if (p->file_errors_count >= p->file_errors_cap) {
        int ncap = p->file_errors_cap ? p->file_errors_cap * 2 : 16;
        lsm_file_error_t *grown =
            (lsm_file_error_t *)realloc(p->file_errors, (size_t)ncap * sizeof(*grown));
        if (!grown) {
            /* Never abort indexing just to record a skip — drop this record. */
            return;
        }
        p->file_errors = grown;
        p->file_errors_cap = ncap;
    }
    lsm_file_error_t *e = &p->file_errors[p->file_errors_count];
    e->path = fe_strdup(path);
    e->reason = fe_strdup(reason);
    e->phase = fe_strdup(phase);
    p->file_errors_count++;
}

void lsm_pipeline_get_file_errors(const lsm_pipeline_t *p, lsm_file_error_t **out, int *count) {
    if (out) {
        *out = p ? p->file_errors : NULL;
    }
    if (count) {
        *count = p ? p->file_errors_count : 0;
    }
}

void lsm_pipeline_get_ignored(const lsm_pipeline_t *p, lsm_ignored_file_t **out, int *count,
                              int *total) {
    if (out) {
        *out = p ? p->ignored_files : NULL;
    }
    if (count) {
        *count = p ? p->ignored_count : 0;
    }
    if (total) {
        *total = p ? p->ignored_total : 0;
    }
}

void lsm_pipeline_get_committed_counts(const lsm_pipeline_t *p, int *nodes, int *edges) {
    if (nodes) {
        *nodes = p ? p->committed_nodes : -1;
    }
    if (edges) {
        *edges = p ? p->committed_edges : -1;
    }
}

void lsm_pipeline_set_committed_counts(lsm_pipeline_t *p, int nodes, int edges) {
    if (p) {
        p->committed_nodes = nodes;
        p->committed_edges = edges;
    }
}

/* Effective worker count. The crash supervisor re-runs its worker single-
 * threaded (LSM_INDEX_SINGLE_THREAD=1) so a per-file marker can pin the EXACT
 * crasher; a parallel re-run would race the marker. Honour that override
 * everywhere the worker count drives the parallel/sequential decision, so the
 * whole extraction phase collapses to the deterministic sequential path. */
static int effective_worker_count(bool initial) {
    const char *st = getenv("LSM_INDEX_SINGLE_THREAD");
    if (st && st[0] == '1') {
        return 1;
    }
    return lsm_default_worker_count(initial);
}

/* Resolve the DB path for this pipeline. Caller must free(). */
static char *resolve_db_path(const lsm_pipeline_t *p) {
    if (!p) {
        return NULL;
    }
    if (p->db_path) {
        return strdup(p->db_path);
    }

    const char *cache_dir = lsm_resolve_cache_dir();
    cache_dir = cache_dir ? cache_dir : lsm_tmpdir();
    if (!cache_dir || !p->project_name) {
        return NULL;
    }
    size_t cache_len = strlen(cache_dir);
    size_t project_len = strlen(p->project_name);
    if (project_len > SIZE_MAX - cache_len) {
        return NULL;
    }
    size_t stem_len = cache_len + project_len;
    if (stem_len > SIZE_MAX - sizeof("/.db")) {
        return NULL;
    }
    size_t path_size = stem_len + sizeof("/.db");
    char *path = malloc(path_size);
    if (!path) {
        return NULL;
    }
    int n = snprintf(path, path_size, "%s/%s.db", cache_dir, p->project_name);
    if (n < 0 || (size_t)n >= path_size) {
        free(path);
        return NULL;
    }
    return path;
}

static int check_cancel(const lsm_pipeline_t *p) {
    return atomic_load(p->cancelled) ? LSM_NOT_FOUND : 0;
}

/* ── Hash table cleanup callback ─────────────────────────────────── */

static void free_seen_dir_key(const char *key, void *val, void *ud) {
    (void)val;
    (void)ud;
    free((void *)key);
}

/* ── Pass 1: Structure ──────────────────────────────────────────── */

/* Create Project, Folder/Package, and File nodes in the graph buffer. */
/* Walk directory chain upward, creating Folder nodes and CONTAINS_FOLDER edges. */
static void create_folder_chain(lsm_pipeline_t *p, const char *dir, LSMHashTable *seen_dirs) {
    char *walk = strdup(dir);
    while (walk[0] != '\0' && !lsm_ht_get(seen_dirs, walk)) {
        lsm_ht_set(seen_dirs, strdup(walk), intptr_to_ptr(SKIP_ONE));
        char *folder_qn = lsm_pipeline_fqn_folder(p->project_name, walk);
        const char *dir_base = strrchr(walk, '/');
        dir_base = dir_base ? dir_base + SKIP_ONE : walk;
        lsm_gbuf_upsert_node(p->gbuf, "Folder", dir_base, folder_qn, walk, 0, 0, "{}");

        char *pdir = strdup(walk);
        char *ps = strrchr(pdir, '/');
        if (ps) {
            *ps = '\0';
        } else {
            free(pdir);
            pdir = strdup("");
        }
        const char *pqn;
        char *pqn_heap = NULL;
        if (pdir[0] == '\0') {
            pqn = p->branch_qn ? p->branch_qn : p->project_name;
        } else {
            pqn_heap = lsm_pipeline_fqn_folder(p->project_name, pdir);
            pqn = pqn_heap;
        }
        const lsm_gbuf_node_t *fn = lsm_gbuf_find_by_qn(p->gbuf, folder_qn);
        const lsm_gbuf_node_t *pn = lsm_gbuf_find_by_qn(p->gbuf, pqn);
        if (fn && pn) {
            lsm_gbuf_insert_edge(p->gbuf, pn->id, fn->id, "CONTAINS_FOLDER", "{}");
        }
        free(folder_qn);
        free(pqn_heap);
        char *up = strrchr(walk, '/');
        if (up) {
            *up = '\0';
        } else {
            walk[0] = '\0';
        }
        free(pdir);
    }
    free(walk);
}

static int pass_structure(lsm_pipeline_t *p, const lsm_file_info_t *files, int file_count) {
    lsm_log_info("pass.start", "pass", "structure", "files", itoa_buf(file_count));

    /* Project node */
    lsm_gbuf_upsert_node(p->gbuf, "Project", p->project_name, p->project_name, NULL, 0, 0, "{}");
    const char *branch_qn = p->branch_qn ? p->branch_qn : p->project_name;
    const char *branch_name = p->git_ctx.branch ? p->git_ctx.branch : "working-tree";
    char branch_props[LSM_SZ_2K];
    const char *branch_props_json = "{}";
    if (lsm_git_context_props_json(&p->git_ctx, branch_props, sizeof(branch_props)) > 0) {
        branch_props_json = branch_props;
    }
    if (p->branch_qn) {
        int64_t branch_id = lsm_gbuf_upsert_node(p->gbuf, "Branch", branch_name, branch_qn, NULL, 0,
                                                 0, branch_props_json);
        const lsm_gbuf_node_t *project_node = lsm_gbuf_find_by_qn(p->gbuf, p->project_name);
        if (project_node && branch_id > 0) {
            lsm_gbuf_insert_edge(p->gbuf, project_node->id, branch_id, "HAS_BRANCH",
                                 branch_props_json);
        }
    }

    /* Collect unique directories and create Folder/Package nodes */
    LSMHashTable *seen_dirs = lsm_ht_create(LSM_SZ_256);

    for (int i = 0; i < file_count; i++) {
        const char *rel = files[i].rel_path;
        if (!rel) {
            continue;
        }

        /* Create File node */
        char *file_qn = lsm_pipeline_fqn_compute(p->project_name, rel, "__file__");
        /* Extract basename */
        const char *slash = strrchr(rel, '/');
        const char *basename = slash ? slash + SKIP_ONE : rel;

        char props[LSM_SZ_256];
        const char *ext = strrchr(basename, '.');
        snprintf(props, sizeof(props), "{\"extension\":\"%s\"}", ext ? ext : "");

        const char *qualified_name = file_qn;
        const char *file_path = rel;
        lsm_gbuf_upsert_node(p->gbuf, "File", basename, qualified_name, file_path, 0, 0, props);

        /* CONTAINS_FILE edge: parent dir -> file */
        char *dir = strdup(rel);
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            {
                *last_slash = '\0';
            }
        } else {
            free(dir);
            dir = strdup("");
        }

        const char *parent_qn;
        char *parent_qn_heap = NULL;
        if (dir[0] == '\0') {
            parent_qn = branch_qn;
        } else {
            parent_qn_heap = lsm_pipeline_fqn_folder(p->project_name, dir);
            parent_qn = parent_qn_heap;
        }

        /* Walk up directory chain, creating Folder nodes */
        create_folder_chain(p, dir, seen_dirs);

        /* Now create the CONTAINS_FILE edge */
        const lsm_gbuf_node_t *fnode = lsm_gbuf_find_by_qn(p->gbuf, file_qn);
        const lsm_gbuf_node_t *pnode = lsm_gbuf_find_by_qn(p->gbuf, parent_qn);
        if (fnode && pnode) {
            lsm_gbuf_insert_edge(p->gbuf, pnode->id, fnode->id, "CONTAINS_FILE", "{}");
        }

        free(file_qn);
        free(dir);
        free(parent_qn_heap);
    }

    /* Free seen_dirs keys */
    lsm_ht_foreach(seen_dirs, free_seen_dir_key, NULL);
    lsm_ht_free(seen_dirs);

    lsm_log_info("pass.done", "pass", "structure", "nodes", itoa_buf(lsm_gbuf_node_count(p->gbuf)),
                 "edges", itoa_buf(lsm_gbuf_edge_count(p->gbuf)));
    return 0;
}

/* ── Pass 2: Definitions ─────────────────────────────────────────── */

/* Implemented in pass_definitions.c via lsm_pipeline_pass_definitions() */

/* ── Githistory compute thread (for fused post-pass parallelism) ─── */

typedef struct {
    const char *repo_path;
    lsm_githistory_result_t *result;
} gh_compute_arg_t;

static void *gh_compute_thread_fn(void *arg) {
    gh_compute_arg_t *a = arg;
    lsm_pipeline_githistory_compute(a->repo_path, a->result);
    return NULL;
}

/* Extract Route nodes from URL strings found in config files (YAML, HCL, TOML).
 * These are infrastructure-defined endpoints (Cloud Scheduler, Terraform). */
/* Process infra bindings: topic→URL pairs from IaC configs.
 * Creates Route nodes for endpoints and HANDLES edges linking
 * topic Routes to endpoint Routes (bridging the gap). */
/* Process one infra binding: create Route node + INFRA_MAPS edge. */
static int process_one_infra_binding(lsm_gbuf_t *gbuf, const LSMInfraBinding *ib,
                                     const char *rel_path) {
    char url_route_qn[LSM_ROUTE_QN_SIZE];
    snprintf(url_route_qn, sizeof(url_route_qn), "__route__infra__%s", ib->target_url);
    int64_t url_route_id = lsm_gbuf_upsert_node(gbuf, "Route", ib->target_url, url_route_qn,
                                                rel_path, 0, 0, "{\"source\":\"infra\"}");
    char topic_route_qn[LSM_ROUTE_QN_SIZE];
    snprintf(topic_route_qn, sizeof(topic_route_qn), "__route__%s__%s",
             ib->broker ? ib->broker : "async", ib->source_name);
    const lsm_gbuf_node_t *topic_route = lsm_gbuf_find_by_qn(gbuf, topic_route_qn);
    int64_t topic_route_id;
    if (topic_route) {
        topic_route_id = topic_route->id;
    } else {
        /* The config file IS the declaration that the topic/queue/schedule exists;
         * upsert its Route node so the binding maps even when no code-side dispatch
         * call created the node first (e.g. a standalone scheduler/subscription
         * manifest). */
        topic_route_id = lsm_gbuf_upsert_node(gbuf, "Route", ib->source_name, topic_route_qn,
                                              rel_path, 0, 0, ib->broker ? ib->broker : "async");
        if (topic_route_id <= 0) {
            return 0;
        }
    }
    char props[LSM_SZ_512];
    snprintf(props, sizeof(props), "{\"broker\":\"%s\",\"topic\":\"%s\",\"endpoint\":\"%s\"}",
             ib->broker ? ib->broker : "async", ib->source_name, ib->target_url);
    lsm_gbuf_insert_edge(gbuf, topic_route_id, url_route_id, "INFRA_MAPS", props);
    return SKIP_ONE;
}

static void lsm_pipeline_process_infra_bindings(lsm_gbuf_t *gbuf, const lsm_file_info_t *files,
                                                LSMFileResult **result_cache, int file_count) {
    int bindings = 0;
    for (int i = 0; i < file_count; i++) {
        if (!result_cache[i]) {
            continue;
        }
        for (int bi = 0; bi < result_cache[i]->infra_bindings.count; bi++) {
            const LSMInfraBinding *ib = &result_cache[i]->infra_bindings.items[bi];
            if (ib->source_name && ib->target_url) {
                bindings += process_one_infra_binding(gbuf, ib, files[i].rel_path);
            }
        }
    }
    if (bindings > 0) {
        char buf[LSM_SZ_16];
        snprintf(buf, sizeof(buf), "%d", bindings);
        lsm_log_info("pass.infra_bindings", "linked", buf);
    }
}

static bool is_infra_file(const char *fp) {
    return fp != NULL &&
           (strstr(fp, ".yaml") != NULL || strstr(fp, ".yml") != NULL ||
            strstr(fp, ".tf") != NULL || strstr(fp, ".hcl") != NULL || strstr(fp, ".toml") != NULL);
}

/* CI/tooling configs describe the development TOOLCHAIN — their URLs are
 * repository/action/registry references, never endpoints this service
 * exposes. Minting infra Route nodes from them lets the route matcher's
 * root-service heuristic attach every handler of an ambiguous "/" route to
 * each tooling URL (junk HANDLES churn on plain pallets/flask, #999).
 * Deny by file identity, not URL shape: deployment configs (Cloud
 * Scheduler, compose) keep minting their genuine endpoints. */
static bool is_ci_tooling_config(const char *fp) {
    if (!fp) {
        return false;
    }
    if (strstr(fp, ".github/") != NULL || strstr(fp, ".gitlab/") != NULL ||
        strstr(fp, ".circleci/") != NULL) {
        return true;
    }
    const char *slash = strrchr(fp, '/');
    const char *base = slash ? slash + 1 : fp;
    static const char *const tooling[] = {".pre-commit-config.yaml",
                                          ".pre-commit-hooks.yaml",
                                          ".gitlab-ci.yml",
                                          ".travis.yml",
                                          "azure-pipelines.yml",
                                          "appveyor.yml",
                                          "bitbucket-pipelines.yml",
                                          ".readthedocs.yaml",
                                          ".readthedocs.yml",
                                          "codecov.yml",
                                          ".codecov.yml",
                                          ".goreleaser.yaml",
                                          ".goreleaser.yml",
                                          ".golangci.yml",
                                          ".golangci.yaml",
                                          NULL};
    for (int i = 0; tooling[i]; i++) {
        if (strcmp(base, tooling[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* True when a YAML key path denotes an UPSTREAM dependency, CONFIG value, or
 * HEALTHCHECK target rather than an endpoint this service exposes. Such URLs
 * (auth JWKS, downstream service base URLs, package-registry URLs, healthcheck
 * curl targets) are NOT routes the service serves and must not mint Route nodes
 * (#521). Exposed-endpoint keys (push_endpoint, post_url, callback, webhook)
 * are intentionally absent here so they still produce infra Route nodes. */
static bool is_upstream_config_key(const char *key_path) {
    if (!key_path) {
        /* No key context (e.g. flat string) — keep prior behaviour and mint. */
        return false;
    }
    static const char *const deny[] = {"jwks",     "registry",     "registries", "healthcheck",
                                       "upstream", "_service_url", "auth",       NULL};
    for (int i = 0; deny[i]; i++) {
        if (strstr(key_path, deny[i]) != NULL) {
            return true;
        }
    }
    return false;
}

/* Try to create an infra Route node from one string_ref. */
static void try_upsert_infra_route(lsm_gbuf_t *gbuf, const LSMStringRef *sr, const char *fp) {
    if (sr->kind != LSM_STRREF_URL || !sr->value || !strstr(sr->value, "://")) {
        return;
    }
    /* Skip upstream/config/healthcheck URLs — they are not exposed routes (#521). */
    if (is_upstream_config_key(sr->key_path)) {
        return;
    }
    char route_qn[LSM_ROUTE_QN_SIZE];
    snprintf(route_qn, sizeof(route_qn), "__route__infra__%s", sr->value);
    char route_props[LSM_SZ_512];
    if (sr->key_path) {
        snprintf(route_props, sizeof(route_props), "{\"source\":\"infra\",\"key_path\":\"%s\"}",
                 sr->key_path);
    } else {
        snprintf(route_props, sizeof(route_props), "{\"source\":\"infra\"}");
    }
    lsm_gbuf_upsert_node(gbuf, "Route", sr->value, route_qn, fp, 0, 0, route_props);
}

/* A URL string_ref that does NOT denote a route the service serves: a value
 * containing whitespace is a command/sentence with an embedded URL (e.g. a
 * Docker healthcheck `curl --fail http://... || exit 1`); a NULL key_path is a
 * context-less/duplicate ref; an upstream/config/healthcheck key is an external
 * dependency, not an exposed route. (#521) */
static bool route_sr_denied(const LSMStringRef *sr) {
    if (!sr->value || strchr(sr->value, ' ')) {
        return true;
    }
    if (!sr->key_path) {
        return true;
    }
    return is_upstream_config_key(sr->key_path);
}

static void lsm_pipeline_extract_infra_routes(lsm_gbuf_t *gbuf, const lsm_file_info_t *files,
                                              LSMFileResult **result_cache, int file_count) {
    /* DENY-WINS-BY-VALUE: the same URL is often extracted as several string_refs
     * at different key_path granularities (full path, leaf key, flat). The Route
     * node is keyed by VALUE, so it would be minted if ANY granularity passed the
     * per-ref guard — e.g. a denied full path `registries.terraform-registry.url`
     * is defeated by a sibling leaf `url`. So pass 1 collects every URL value
     * denied under ANY of its refs; pass 2 mints only values never denied. (#521) */
    LSMHashTable *denied = lsm_ht_create(16);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < file_count; i++) {
            if (!result_cache[i] || !is_infra_file(files[i].rel_path) ||
                is_ci_tooling_config(files[i].rel_path)) {
                continue;
            }
            for (int si = 0; si < result_cache[i]->string_refs.count; si++) {
                const LSMStringRef *sr = &result_cache[i]->string_refs.items[si];
                if (sr->kind != LSM_STRREF_URL || !sr->value || !strstr(sr->value, "://")) {
                    continue;
                }
                if (pass == 0) {
                    if (denied && route_sr_denied(sr)) {
                        lsm_ht_set(denied, sr->value, (void *)1);
                    }
                } else if (!denied || !lsm_ht_has(denied, sr->value)) {
                    try_upsert_infra_route(gbuf, sr, files[i].rel_path);
                }
            }
        }
    }
    lsm_ht_free(denied);
}

/* Run decorator_tags, configlink, and route matching passes. */
typedef void (*predump_pass_fn)(lsm_pipeline_ctx_t *);
static void predump_deco(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_decorator_tags(ctx->gbuf, ctx->project_name);
}
static void predump_route(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_create_route_nodes(ctx->gbuf);
}
static void predump_sim(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_similarity(ctx);
}
static void predump_sem(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_semantic_edges(ctx);
}
static void predump_cfg(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_configlink(ctx);
}
static void predump_complexity(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_complexity(ctx);
}
static void predump_documents(lsm_pipeline_ctx_t *ctx) {
    lsm_pipeline_pass_documents(ctx);
}
static void run_predump_passes(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx) {
    static const struct {
        predump_pass_fn fn;
        const char *name;
        bool moderate_only; /* true = skip in fast mode */
    } passes[] = {
        {predump_deco, "decorator_tags", false}, {predump_cfg, "configlink", false},
        {predump_route, "route_match", false},   {predump_sim, "similarity", true},
        {predump_documents, "documents", false}, {predump_sem, "semantic_edges", true},
        {predump_complexity, "complexity", false},
    };
    enum { PREDUMP_PASS_COUNT = 7 };
    struct timespec t;
    for (int i = 0; i < PREDUMP_PASS_COUNT && !check_cancel(p); i++) {
        /* "moderate_only" passes (similarity/semantic edges) run in FULL,
         * MODERATE and ADVANCED — they are skipped only in FAST. Compare
         * explicitly against FAST rather than `> MODERATE` so ADVANCED
         * (numerically 3) is not mistaken for a lighter mode than FULL. */
        if (passes[i].moderate_only && p->mode == LSM_MODE_FAST) {
            continue;
        }
        lsm_clock_gettime(CLOCK_MONOTONIC, &t);
        passes[i].fn(ctx);
        lsm_log_info("pass.timing", "pass", passes[i].name, "elapsed_ms",
                     itoa_buf((int)elapsed_ms(t)));
    }
}

/* Adapter that lets lsm_pipeline_pass_lsp_cross slot into the seq_passes
 * dispatch table. The cross-file LSP needs the per-file LSMFileResult cache
 * to read defs/imports without re-extracting; in the sequential path that
 * cache is ctx->result_cache (set up by run_sequential_pipeline before
 * launching the dispatch loop). When the cache is unavailable (e.g. if the
 * pipeline opted out of caching), the pass becomes a no-op since there are
 * no extracted results to feed cross-file resolution. */
static int seq_pass_lsp_cross_dispatch(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                                       int file_count) {
    if (!ctx || !ctx->result_cache)
        return 0;
    /* Cross-file LSP runs in every mode. */
    return lsm_pipeline_pass_lsp_cross(ctx, files, file_count, ctx->result_cache);
}

/* Run the sequential pipeline path: definitions, k8s, lsp_cross, calls, usages, semantic. */
/* Build the ObjectScript $$$macro table from .inc include files in the repo.
 * Returns NULL (and does no work) when no ObjectScript include files exist.
 * Caller owns the returned heap table (free via lsm_macro_table_free). */
LSMMacroTable *lsm_build_macro_table_from_files(const lsm_file_info_t *files, int count,
                                                const char *repo_path) {
    (void)repo_path;
    bool has_inc = false;
    for (int i = 0; i < count; i++) {
        if (files[i].language == LSM_LANG_OBJECTSCRIPT_ROUTINE && files[i].path &&
            (strrchr(files[i].path, '.') != NULL &&
             strcmp(strrchr(files[i].path, '.'), ".inc") == 0)) {
            has_inc = true;
            break;
        }
    }
    if (!has_inc) {
        return NULL;
    }

    LSMMacroTable *mt = (LSMMacroTable *)calloc(1, sizeof(LSMMacroTable));
    if (!mt) {
        return NULL;
    }

    lsm_arena_init(&mt->arena);
    lsm_macro_table_init_system(mt);

    for (int i = 0; i < count; i++) {
        if (files[i].language != LSM_LANG_OBJECTSCRIPT_ROUTINE) {
            continue;
        }
        if (!files[i].path || !(strrchr(files[i].path, '.') != NULL &&
                                strcmp(strrchr(files[i].path, '.'), ".inc") == 0)) {
            continue;
        }
        FILE *f = lsm_fopen(files[i].path, "rb");
        if (!f) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        rewind(f);
        if (fsize > 0) {
            char *src = (char *)malloc((size_t)fsize + 1);
            if (src) {
                size_t nread = fread(src, 1, (size_t)fsize, f);
                src[nread] = '\0';
                lsm_parse_inc_file(mt, &mt->arena, src);
                free(src);
            }
        }
        (void)fclose(f);
    }
    return mt;
}

static int run_sequential_pipeline(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx,
                                   const lsm_file_info_t *files, int file_count,
                                   struct timespec *t) {
    lsm_log_info("pipeline.mode", "mode", "sequential", "files", itoa_buf(file_count));

    /* Build package map from manifest files (sequential: read manifests directly).
     * Use the repo-walking variant so manifests filtered out by the main
     * discoverer (package.json, composer.json) still feed pkgmap and let
     * workspace imports like `@my/pkg` resolve to their target Module. */
    lsm_pipeline_set_pkgmap(lsm_pkgmap_build_from_repo(ctx->repo_path, files, file_count,
                                                       ctx->project_name, ctx->excluded_dirs,
                                                       ctx->excluded_count));

    LSMFileResult **seq_cache = (LSMFileResult **)calloc(file_count, sizeof(LSMFileResult *));
    if (seq_cache) {
        ctx->result_cache = seq_cache;
    }

    /* ObjectScript: build the $$$macro table from .inc include files so that
     * pass_calls can resolve macro-mediated dispatch. NULL when not present. */
    LSMMacroTable *mt = lsm_build_macro_table_from_files(files, file_count, ctx->repo_path);
    if (mt) {
        ctx->macro_table = mt;
    }
    typedef int (*seq_pass_fn)(lsm_pipeline_ctx_t *, const lsm_file_info_t *, int);
    static const struct {
        seq_pass_fn fn;
        const char *name;
        bool ignore_err;
    } seq_passes[] = {
        {lsm_pipeline_pass_definitions, "definitions", false},
        {lsm_pipeline_pass_k8s, "k8s", true},
        {seq_pass_lsp_cross_dispatch, "lsp_cross", true},
        {lsm_pipeline_pass_calls, "calls", false},
        {lsm_pipeline_pass_usages, "usages", false},
        {lsm_pipeline_pass_semantic, "semantic", false},
    };
    int rc = 0;
    for (int si = 0; si < PL_SEQ_PASSES && rc == 0; si++) {
        lsm_clock_gettime(CLOCK_MONOTONIC, t);
        int pr = seq_passes[si].fn(ctx, files, file_count);
        if (pr != 0 && !seq_passes[si].ignore_err) {
            rc = pr;
        }
        lsm_log_info("pass.timing", "pass", seq_passes[si].name, "elapsed_ms",
                     itoa_buf((int)elapsed_ms(*t)));
        if (check_cancel(p)) {
            rc = LSM_NOT_FOUND;
        }
    }
    /* Consume infra bindings (YAML/HCL topic/queue/scheduler → endpoint) so
     * INFRA_MAPS edges also form on the sequential path, not just the parallel
     * one. process_one_infra_binding self-creates the topic Route node when no
     * code-side dispatch created it (e.g. a standalone scheduler manifest). */
    if (seq_cache && rc == 0) {
        lsm_pipeline_extract_infra_routes(p->gbuf, files, seq_cache, file_count);
        lsm_pipeline_process_infra_bindings(p->gbuf, files, seq_cache, file_count);
    }
    if (seq_cache) {
        for (int i = 0; i < file_count; i++) {
            if (seq_cache[i]) {
                lsm_free_result(seq_cache[i]);
            }
        }
        free(seq_cache);
        ctx->result_cache = NULL;
    }
    /* Release the lsp_cross pass's shared registries only now: resolved_calls
     * borrowed registry-owned strings that the calls pass read above. The
     * module-QN strings the registries borrow (parked on the ctx by the pass
     * for exactly this lifetime) go with them. */
    if (ctx->seq_cross_arena_live) {
        lsm_arena_destroy(&ctx->seq_cross_arena);
        ctx->seq_cross_arena_live = false;
    }
    if (ctx->seq_cross_def_modules) {
        for (int i = 0; i < ctx->seq_cross_def_module_count; i++) {
            free(ctx->seq_cross_def_modules[i]);
        }
        free(ctx->seq_cross_def_modules);
        ctx->seq_cross_def_modules = NULL;
        ctx->seq_cross_def_module_count = 0;
    }
    /* Destroy this thread's TLS parser: the sequential path parses on the
     * CALLING thread (usually main), and a parser left alive here was
     * allocated in the current tree-sitter allocator epoch. A later
     * parallel run switches the global ts allocator to the slab
     * (lsm_slab_install); destroying the stale parser then frees
     * mimalloc-epoch memory through slab_free -> plain free() and libmalloc
     * aborts — the #773 second-index SIGABRT. */
    lsm_destroy_thread_parser();
    /* ObjectScript: free the macro / return-type tables built for this run. */
    if (ctx->macro_table) {
        lsm_macro_table_free((LSMMacroTable *)ctx->macro_table);
        ctx->macro_table = NULL;
    }
    if (ctx->return_type_table) {
        for (int i = 0; i < ctx->return_type_table->count; i++) {
            free((void *)ctx->return_type_table->entries[i].return_type);
        }
        free((void *)ctx->return_type_table);
        ctx->return_type_table = NULL;
    }
    return rc;
}

/* Run the parallel pipeline path: extract, registry, resolve, infra, k8s. */
static int run_parallel_pipeline(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx,
                                 const lsm_file_info_t *files, int file_count, int worker_count,
                                 struct timespec *t) {
    lsm_log_info("pipeline.mode", "mode", "parallel", "workers", itoa_buf(worker_count), "files",
                 itoa_buf(file_count));
    _Atomic int64_t shared_ids;
    atomic_init(&shared_ids, lsm_gbuf_next_id(p->gbuf));
    LSMFileResult **cache = (LSMFileResult **)calloc(file_count, sizeof(LSMFileResult *));
    if (!cache) {
        lsm_log_error("pipeline.err", "phase", "cache_alloc");
        return LSM_NOT_FOUND;
    }
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    int rc = lsm_parallel_extract(ctx, files, file_count, cache, &shared_ids, worker_count);
    lsm_log_info("pass.timing", "pass", "parallel_extract", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(*t)));
    if (rc != 0 || check_cancel(p)) {
        for (int i = 0; i < file_count; i++) {
            lsm_free_result(cache[i]);
        }
        free(cache);
        return rc != 0 ? rc : LSM_NOT_FOUND;
    }
    lsm_gbuf_set_next_id(p->gbuf, atomic_load(&shared_ids));
    /* extract -> registry handoff: return the extract phase's freed-but-retained
     * allocator pages to the OS before registry_build allocates. On a 2x Linux
     * index the extract peak holds ~13 GB of reclaimable pages (peak_mb 20.7 vs
     * live rss_mb 7); not returning them pushed the process over the system
     * memory-pressure threshold and got it SIGKILLed at registry entry. */
    lsm_mem_collect();
    lsm_log_info("mem.collect", "phase", "post_extract", "rss_mb",
                 itoa_buf((int)(lsm_mem_rss() / (1024 * 1024))));
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    rc = lsm_build_registry_from_cache(ctx, files, file_count, cache);
    lsm_log_info("pass.timing", "pass", "registry_build", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(*t)));
    log_phase_mem("registry_build");
    if (rc != 0 || check_cancel(p)) {
        for (int i = 0; i < file_count; i++) {
            if (cache[i]) {
                lsm_free_result(cache[i]);
            }
        }
        free(cache);
        return rc != 0 ? rc : LSM_NOT_FOUND;
    }
    /* Registry consumers may materialize serial nodes (Channel, EnvVar, and
     * future carrier-derived resources) after parallel extraction established
     * the shared allocator watermark. Advance the atomic allocator before
     * resolve workers resume; otherwise their IDs and the later next-id reset
     * can collide with those nodes and orphan freshly inserted edges. */
    int64_t registry_next_id = lsm_gbuf_next_id(p->gbuf);
    if (registry_next_id > atomic_load(&shared_ids)) {
        atomic_store(&shared_ids, registry_next_id);
    }
    /* Cross-file LSP precondition: build a project-wide LSMLSPDef[]
     * once. The fused resolve_worker invokes lsm_pxc_run_one(_ts) per
     * file using these defs + the file's IMPORTS map, so cross-file
     * type-resolved CALLS land in result->resolved_calls before the
     * CALLS-edge emission. This replaces the old sequential
     * lsm_pipeline_pass_lsp_cross pass which re-read every source from
     * disk and re-parsed every tree on a single thread (~520s on
     * kubernetes). Soft-failure: NULL all_defs / NULL def_modules just
     * mean cross-file LSP no-ops; per-file LSP already ran during
     * extract. */
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    /* Cross-file LSP (type-aware call/usage resolution across files) — the
     * most expensive phase. LSM_DISABLE_LSP_CROSS=1 opts out (it can SIGSEGV
     * on large TS projects — see #340/#344); with cross-LSP off, all_defs
     * stays NULL and the fused resolver simply no-ops cross-file resolution
     * (per-file LSP already ran during extract). */
    char lsm_lsp_cross_env[LSM_SZ_16];
    const bool run_cross_lsp = lsm_safe_getenv("LSM_DISABLE_LSP_CROSS", lsm_lsp_cross_env,
                                               sizeof(lsm_lsp_cross_env), NULL) == NULL;
    if (!run_cross_lsp) {
        lsm_log_info("lsp_cross.skipped", "reason", "LSM_DISABLE_LSP_CROSS env set");
    }
    char **def_modules = NULL;
    int def_count = 0;
    LSMLSPDef *all_defs = NULL;
    int *def_starts = NULL;
    if (run_cross_lsp) {
        def_modules = (char **)calloc((size_t)file_count, sizeof(char *));
        def_starts = (int *)calloc((size_t)file_count + 1, sizeof(int));
        all_defs = def_modules
                       ? lsm_pxc_collect_all_defs(cache, files, file_count, ctx->project_name,
                                                  def_modules, &def_count, def_starts)
                       : NULL;
    }
    /* Serialize per-file LSP surfaces NOW — the result cache dies with this
     * pass, and the rows are what lets an incremental run detect body-only
     * edits and rehydrate cross registries without re-parsing the world.
     * Failure only degrades: no rows → the incremental route full-rebuilds. */
    if (ctx->pipeline && all_defs && def_starts) {
        lsm_lsp_surface_row_t *surface_rows = NULL;
        int surface_count = 0;
        if (lsm_lsp_surface_build_rows(ctx->project_name, cache, files, file_count, all_defs,
                                       def_starts, &surface_rows, &surface_count) == 0) {
            lsm_pipeline_set_lsp_surfaces(ctx->pipeline, surface_rows, surface_count);
        } else {
            lsm_log_warn("lsp_surface.serialize_failed", "files", itoa_buf(file_count));
        }
    }
    free(def_starts);
    /* Build inverted index: module_qn → defs. The fused resolve_worker
     * uses this to filter the global all_defs[] down to just the defs
     * each file actually needs (own_module + imported modules) — the
     * gopls "package summary" pattern. Drops per-file registry build
     * cost from O(all_defs) to O(relevant_defs), typically 50-100×
     * smaller per file. */
    LSMModuleDefIndex *module_def_index =
        all_defs ? lsm_pxc_build_module_def_index(all_defs, def_count) : NULL;
    /* Tier 2 full: pre-build per-language cross-LSP registries.
     * Built ONCE here; shared READ-ONLY across all files of that language
     * during resolve. Per-file work is then: parse + AST walk + O(1) lookups
     * — no registry build, no Phase 1b mutations. Languages added so far:
     * Go, Python, C/C++, C#, TS/JS, Java. Others (Kotlin, PHP) fall back to per-file. */
    LSMArena cross_lsp_arena;
    lsm_arena_init(&cross_lsp_arena);
    LSMCrossLspRegistries cross_registries = {0};
    if (all_defs) {
        /* Per-builder split of lsp_cross_prepare — attributes a slow prepare to
         * ONE language instead of re-diagnosing the whole pass (the cs builder
         * hid ~140 s behind the pass total, #1669 follow-up). */
        struct timespec t_b;
        long b_ms[6];
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.go = lsm_go_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[0] = (long)elapsed_ms(t_b);
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.python =
            lsm_py_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[1] = (long)elapsed_ms(t_b);
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.c = lsm_c_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[2] = (long)elapsed_ms(t_b);
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.cs = lsm_cs_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[3] = (long)elapsed_ms(t_b);
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.ts = lsm_ts_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[4] = (long)elapsed_ms(t_b);
        lsm_clock_gettime(CLOCK_MONOTONIC, &t_b);
        cross_registries.java =
            lsm_java_build_cross_registry(&cross_lsp_arena, all_defs, def_count);
        b_ms[5] = (long)elapsed_ms(t_b);
        char b_buf[6][LSM_SZ_16];
        const char *b_name[6] = {"go", "python", "c", "cs", "ts", "java"};
        for (int bi = 0; bi < 6; bi++) {
            snprintf(b_buf[bi], sizeof(b_buf[bi]), "%ld", b_ms[bi]);
        }
        lsm_log_info("lsp_cross_prepare.builders", b_name[0], b_buf[0], b_name[1], b_buf[1],
                     b_name[2], b_buf[2], b_name[3], b_buf[3], b_name[4], b_buf[4], b_name[5],
                     b_buf[5]);
        /* Rust: NOT built here. The shared all_defs registry is built LAZILY on the
         * first NULL-filter rust file (the amplifier files) inside lsm_parallel_resolve
         * — repos whose rust files all filter to subsets never pay the build/RSS. */
    }
    lsm_log_info("pass.timing", "pass", "lsp_cross_prepare", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(*t)));
    log_phase_mem("lsp_cross_prepare");
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    rc = lsm_parallel_resolve(ctx, files, file_count, cache, &shared_ids, worker_count, all_defs,
                              def_count, def_modules, module_def_index, &cross_registries);
    lsm_log_info("pass.timing", "pass", "parallel_resolve", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(*t)));
    log_phase_mem("parallel_resolve");
    lsm_pxc_free_module_def_index(module_def_index);
    lsm_arena_destroy(&cross_lsp_arena); /* releases all per-lang registries */
    free(all_defs);
    if (def_modules) {
        for (int i = 0; i < file_count; i++) {
            free(def_modules[i]);
        }
        free(def_modules);
    }
    lsm_gbuf_set_next_id(p->gbuf, atomic_load(&shared_ids));
    lsm_pipeline_extract_infra_routes(p->gbuf, files, cache, file_count);
    lsm_pipeline_process_infra_bindings(p->gbuf, files, cache, file_count);
    for (int i = 0; i < file_count; i++) {
        if (cache[i]) {
            lsm_free_result(cache[i]);
        }
    }
    free(cache);
    if (rc != 0) {
        return rc;
    }
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    lsm_pipeline_pass_k8s(ctx, files, file_count);
    lsm_log_info("pass.timing", "pass", "k8s", "elapsed_ms", itoa_buf((int)elapsed_ms(*t)));
    return check_cancel(p) ? LSM_NOT_FOUND : 0;
}

static int capture_existing_adr(lsm_pipeline_t *p, const char *db_path) {
#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
    if (atomic_exchange(&g_persist_test_fail_adr_capture, false)) {
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
#endif
    lsm_store_t *adr_store = lsm_store_open_path_query(db_path);
    if (!adr_store) {
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    lsm_adr_t existing = {0};
    int adr_rc = lsm_store_adr_get(adr_store, p->project_name, &existing);
    if (adr_rc == LSM_STORE_NOT_FOUND) {
        lsm_store_close(adr_store);
        free(p->saved_adr);
        p->saved_adr = NULL;
        return 0;
    }
    if (adr_rc != LSM_STORE_OK || !existing.content) {
        lsm_store_adr_free(&existing);
        lsm_store_close(adr_store);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    char *saved = strdup(existing.content);
    lsm_store_adr_free(&existing);
    lsm_store_close(adr_store);
    if (!saved) {
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    free(p->saved_adr);
    p->saved_adr = saved;
    return 0;
}

/* Route an existing generation. Full rebuilds never delete the live DB here:
 * publication owns the eventual atomic replacement after every pass and
 * metadata write has succeeded. */
static int try_incremental_or_delete_db(lsm_pipeline_t *p, lsm_file_info_t *files, int file_count,
                                        const lsm_file_hash_t *baseline_manifest,
                                        int baseline_count, bool force_full_on_mismatch) {
    char *db_path = resolve_db_path(p);
    if (!db_path) {
        return LSM_PIPELINE_FORCE_FULL_REINDEX;
    }
    struct stat db_st;
    if (stat(db_path, &db_st) != 0) {
        free(db_path);
        return LSM_PIPELINE_FORCE_FULL_REINDEX;
    }
    lsm_store_t *check_store = lsm_store_open_path_query(db_path);
    bool valid = check_store && lsm_store_check_integrity(check_store);
    if (check_store) {
        lsm_store_close(check_store);
    }
    if (!valid) {
        lsm_log_warn("pipeline.route", "path", "full", "reason", "invalid_existing_db");
        free(db_path);
        return LSM_PIPELINE_FORCE_FULL_REINDEX;
    }
    lsm_log_info("pipeline.route", "path", "incremental_manifest");
    int rc = lsm_pipeline_run_incremental(p, db_path, files, file_count, baseline_manifest,
                                          baseline_count, force_full_on_mismatch);
    /* Delete the existing generation ONLY when we are about to rebuild it.
     * On main this was guarded by an early `return rc` for the incremental
     * path; this function has no such early return, so the delete must be
     * conditional. Unconditionally removing it destroys the database on the
     * no-op and successful-incremental routes -- the pipeline reports success
     * while every later reader finds no store. */
    if (rc == LSM_PIPELINE_FORCE_FULL_REINDEX) {
        int adr_rc = capture_existing_adr(p, db_path);
        if (adr_rc != 0) {
            rc = adr_rc;
        }
        (void)lsm_unlink(db_path);
        (void)lsm_remove_db_sidecars(db_path);
    }
    free(db_path);
    return rc;
}

static const char *pipeline_mode_name(lsm_index_mode_t mode) {
    switch (mode) {
    case LSM_MODE_FULL:
        return "full";
    case LSM_MODE_MODERATE:
        return "moderate";
    case LSM_MODE_FAST:
        return "fast";
    default:
        return "unknown";
    }
}

static int pipeline_mode_coverage_rank(lsm_index_mode_t mode) {
    switch (mode) {
    case LSM_MODE_FULL:
        return 3;
    case LSM_MODE_MODERATE:
        return 2;
    case LSM_MODE_FAST:
        return 1;
    default:
        return 0;
    }
}

/* Index modes are additive: a cheaper run may refresh a fuller graph, but it
 * must never erase files that the cheaper discovery intentionally skips. The
 * exact-manifest pipeline therefore keeps the most comprehensive successfully
 * published mode and performs any changed rebuild at that coverage level. */
static bool promote_mode_to_existing_coverage(lsm_pipeline_t *p) {
    if (!p || !p->project_name) {
        return false;
    }
    char *db_path = resolve_db_path(p);
    if (!db_path) {
        return false;
    }
    lsm_store_t *store = lsm_store_open_path_query(db_path);
    free(db_path);
    if (!store) {
        return false;
    }
    bool promoted = false;
    lsm_coverage_meta_t meta = {0};
    if (lsm_store_coverage_meta_get(store, p->project_name, &meta) == LSM_STORE_OK &&
        meta.index_mode) {
        lsm_index_mode_t stored_mode = p->mode;
        if (strcmp(meta.index_mode, "full") == 0) {
            stored_mode = LSM_MODE_FULL;
        } else if (strcmp(meta.index_mode, "moderate") == 0) {
            stored_mode = LSM_MODE_MODERATE;
        } else if (strcmp(meta.index_mode, "fast") == 0) {
            stored_mode = LSM_MODE_FAST;
        }
        if (pipeline_mode_coverage_rank(stored_mode) > pipeline_mode_coverage_rank(p->mode)) {
            lsm_log_info("pipeline.mode", "requested", pipeline_mode_name(p->mode), "effective",
                         pipeline_mode_name(stored_mode), "reason", "preserve_existing_coverage");
            p->mode = stored_mode;
            promoted = true;
        }
    }
    lsm_store_coverage_meta_clear(&meta);
    lsm_store_close(store);
    return promoted;
}

/* Defined below, next to the other publication helpers. */
static char *create_staging_path(const char *final_path);

static void discard_generation_stage(const char *stage_path) {
    if (!stage_path) {
        return;
    }
    lsm_unlink(stage_path);
    lsm_remove_db_sidecars(stage_path);
}

static int generation_rebuild_fts(lsm_store_t *store) {
    if (lsm_store_exec(store, "INSERT INTO nodes_fts(nodes_fts) VALUES('delete-all');") !=
        LSM_STORE_OK) {
        return LSM_STORE_ERR;
    }
    if (lsm_store_exec(store,
                       "INSERT INTO nodes_fts(rowid, name, qualified_name, label, file_path) "
                       "SELECT id, lsm_camel_split(name), qualified_name, label, file_path "
                       "FROM nodes;") == LSM_STORE_OK) {
        return LSM_STORE_OK;
    }
    return lsm_store_exec(store,
                          "INSERT INTO nodes_fts(rowid, name, qualified_name, label, file_path) "
                          "SELECT id, name, qualified_name, label, file_path FROM nodes;");
}

typedef struct {
    bool quarantined;
    char backup_path[LSM_SZ_4K];
} lsm_replacement_prepare_t;

static int replacement_sidecar_path(char *out, size_t out_size, const char *base,
                                    const char *suffix) {
    int n = snprintf(out, out_size, "%s%s", base, suffix);
    return n > 0 && (size_t)n < out_size ? 0 : LSM_PIPELINE_PERSIST_FAILED;
}

static bool replacement_path_exists(const char *path) {
    lsm_path_info_t info;
    return lsm_path_info_utf8(path, &info) == 0;
}

static int rollback_quarantined_generation(const char *db_path,
                                           lsm_replacement_prepare_t *prepared) {
    if (!prepared || !prepared->quarantined) {
        return 0;
    }
    static const char *const suffixes[] = {"-wal", "-shm"};
    if (lsm_rename_noreplace(prepared->backup_path, db_path) != 0) {
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        char source[LSM_SZ_4K];
        char destination[LSM_SZ_4K];
        if (replacement_sidecar_path(source, sizeof(source), prepared->backup_path, suffixes[i]) !=
                0 ||
            replacement_sidecar_path(destination, sizeof(destination), db_path, suffixes[i]) != 0) {
            return LSM_PIPELINE_PERSIST_FAILED;
        }
        if (replacement_path_exists(source) && lsm_rename_noreplace(source, destination) != 0) {
            return LSM_PIPELINE_PERSIST_FAILED;
        }
    }
    prepared->quarantined = false;
    prepared->backup_path[0] = '\0';
    return 0;
}

static int quarantine_existing_generation(const char *db_path,
                                          lsm_replacement_prepare_t *prepared) {
    if (!db_path || !prepared) {
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    static const char *const suffixes[] = {"-wal", "-shm"};
    char candidate[LSM_SZ_4K];
    for (int attempt = 0; attempt < 10000; attempt++) {
        int n = attempt == 0
                    ? snprintf(candidate, sizeof(candidate), "%s.corrupt", db_path)
                    : snprintf(candidate, sizeof(candidate), "%s.corrupt.%d", db_path, attempt);
        if (n <= 0 || (size_t)n >= sizeof(candidate)) {
            return LSM_PIPELINE_PERSIST_FAILED;
        }
        bool available = !replacement_path_exists(candidate);
        for (size_t i = 0; available && i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
            char candidate_sidecar[LSM_SZ_4K];
            if (replacement_sidecar_path(candidate_sidecar, sizeof(candidate_sidecar), candidate,
                                         suffixes[i]) != 0) {
                return LSM_PIPELINE_PERSIST_FAILED;
            }
            available = !replacement_path_exists(candidate_sidecar);
        }
        if (!available) {
            continue;
        }
        if (lsm_rename_noreplace(db_path, candidate) != 0) {
            if (replacement_path_exists(candidate)) {
                continue;
            }
            return LSM_PIPELINE_PERSIST_FAILED;
        }

        snprintf(prepared->backup_path, sizeof(prepared->backup_path), "%s", candidate);
        prepared->quarantined = true;
        for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
            char source[LSM_SZ_4K];
            char destination[LSM_SZ_4K];
            if (replacement_sidecar_path(source, sizeof(source), db_path, suffixes[i]) != 0 ||
                replacement_sidecar_path(destination, sizeof(destination), candidate,
                                         suffixes[i]) != 0) {
                (void)rollback_quarantined_generation(db_path, prepared);
                return LSM_PIPELINE_PERSIST_FAILED;
            }
            if (replacement_path_exists(source) && lsm_rename_noreplace(source, destination) != 0) {
                (void)rollback_quarantined_generation(db_path, prepared);
                return LSM_PIPELINE_PERSIST_FAILED;
            }
        }
        return 0;
    }
    return LSM_PIPELINE_PERSIST_FAILED;
}

/* `quarantine_invalid` separates the two callers, which own very different
 * destinations. The publishing wrapper passes true: its destination is the
 * user's live database, and bytes that are not a readable database are the only
 * evidence of what went wrong, so they are moved aside rather than overwritten.
 * publish_generation passes false: its destination is a private staging file
 * this process created moments ago, so an unreadable one is our own debris --
 * parking that under a .corrupt name leaves a file in the database directory
 * that nothing ever collects and that no one can interpret. */
static int prepare_existing_generation_for_replace(const char *db_path,
                                                   lsm_replacement_prepare_t *prepared,
                                                   bool quarantine_invalid) {
    if (!prepared) {
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    memset(prepared, 0, sizeof(*prepared));
    /* Every failure edge below logs before returning: a silent PERSIST_FAILED
     * surfaces to the user as "Pipeline failed. Check repo_path ..." -- blaming
     * a repo that indexed perfectly for a destination-side replacement fault. */
    lsm_path_info_t info;
    if (lsm_path_info_utf8(db_path, &info) == 0) {
        if (!info.is_regular || info.is_symlink) {
            lsm_log_error("finalize.prepare_failed", "reason", "destination_not_regular", "path",
                          db_path);
            return LSM_PIPELINE_PERSIST_FAILED;
        }
        int seal_rc = lsm_store_seal_existing_path_for_replace(db_path);
        if (seal_rc == LSM_STORE_NOT_FOUND) {
            if (!quarantine_invalid) {
                (void)lsm_unlink(db_path);
                if (lsm_remove_db_sidecars(db_path) != 0) {
                    lsm_log_error("finalize.prepare_failed", "reason",
                                  "invalid_destination_sidecar_cleanup", "path", db_path);
                    return LSM_PIPELINE_PERSIST_FAILED;
                }
                return 0;
            }
            return quarantine_existing_generation(db_path, prepared);
        }
        if (seal_rc != LSM_STORE_OK) {
            char seal_text[16];
            (void)snprintf(seal_text, sizeof(seal_text), "%d", seal_rc);
            lsm_log_error("finalize.prepare_failed", "reason", "seal_existing", "rc", seal_text,
                          "path", db_path);
            return LSM_PIPELINE_PERSIST_FAILED;
        }
    }
    if (lsm_remove_db_sidecars(db_path) != 0) {
        char errno_text[16];
        (void)snprintf(errno_text, sizeof(errno_text), "%d", errno);
        lsm_log_error("finalize.prepare_failed", "reason", "sidecar_cleanup", "errno", errno_text,
                      "path", db_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    return 0;
}

int lsm_pipeline_publish_generation(const lsm_pipeline_generation_t *generation) {
    if (!generation || !generation->gbuf || !generation->final_db_path || !generation->project ||
        generation->manifest_count < 0 ||
        (generation->manifest_count > 0 && !generation->manifest) ||
        generation->coverage_count < 0 ||
        (generation->coverage_count > 0 && !generation->coverage)) {
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    if (generation->cancelled && atomic_load(generation->cancelled)) {
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }

    /* The staging name must be unpredictable and created exclusively. It used
     * to be "<db>.stage.<pid>.<counter>", which any other process can compute
     * in advance; this path is then unlinked and written, so in a
     * world-writable database directory an attacker could land a symlink in
     * the window between the two and have us clobber the target. Sharing the
     * mkstemp-based helper the other staging site already uses closes that:
     * O_EXCL creation means we only ever write a file we made ourselves.
     *
     * The old unlink-first step goes with it. It existed to clear a leftover
     * file at a name we might reuse; a freshly minted name cannot collide,
     * and its sidecars cannot pre-exist either. */
    char *stage_path = create_staging_path(generation->final_db_path);
    if (!stage_path) {
        return LSM_PIPELINE_PERSIST_FAILED;
    }

    int dump_rc = lsm_gbuf_dump_to_sqlite(generation->gbuf, stage_path);
    if (dump_rc != 0) {
        discard_generation_stage(stage_path);
        free(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
    if (lsm_pipeline_persist_test_take_failure_after_stage_dump()) {
        discard_generation_stage(stage_path);
        free(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
#endif
    if (generation->cancelled && atomic_load(generation->cancelled)) {
        discard_generation_stage(stage_path);
        free(stage_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }

    return lsm_pipeline_publish_staged(stage_path, generation, true, false);
}

/* Complete and publish an already-materialized staging database: metadata
 * writes, FTS policy, integrity, seal, then the shared finalize leg. Takes
 * ownership of stage_path (frees it on every path). fts_wholesale selects
 * the dump path's delete-all-and-rebuild; the delta path passes false
 * because its patch step already wrote row-level FTS inserts for exactly
 * the nodes it created. */
int lsm_pipeline_publish_staged(char *stage_path, const lsm_pipeline_generation_t *generation,
                                bool fts_wholesale, bool destination_known_healthy) {
    struct timespec t_pub;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);
    lsm_store_t *store = lsm_store_open_path(stage_path);
    if (!store) {
        discard_generation_stage(stage_path);
        free(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    bool ok = lsm_store_exec(store, "PRAGMA synchronous=FULL;") == LSM_STORE_OK;
    ok = ok && lsm_store_delete_file_hashes(store, generation->project) == LSM_STORE_OK &&
         lsm_store_upsert_file_hash_batch(store, generation->manifest,
                                          generation->manifest_count) == LSM_STORE_OK;
    /* LSP surfaces belong to the generation: written inside the same staging
     * store, before the atomic rename, so graph and surface data can never
     * publish separately. The delete guards the incremental path, whose
     * staging DB starts as a copy of the previous generation. */
    /* surfaces_in_place: the delta patch already upserted exactly the
     * repaired files' rows and deleted the purged ones inside its own
     * transaction; rewriting every row here would be the single largest
     * block of a delta publish at scale. */
    if (ok && !generation->surfaces_in_place) {
        ok = lsm_store_delete_lsp_surfaces(store, generation->project) == LSM_STORE_OK &&
             lsm_store_upsert_lsp_surface_batch(store, generation->surface_rows,
                                                generation->surface_row_count) == LSM_STORE_OK;
    }
    if (ok && generation->adr_content) {
        ok = lsm_store_adr_store(store, generation->project, generation->adr_content) ==
             LSM_STORE_OK;
    }
    lsm_log_info("publish.timing", "block", "writes", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_pub)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);

    lsm_project_t project_info = {0};
    bool have_project_info =
        lsm_store_get_project(store, generation->project, &project_info) == LSM_STORE_OK;
    lsm_log_info("publish.timing", "block", "get_project", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_pub)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);
    lsm_coverage_meta_t meta = generation->coverage_meta;
    meta.generation = have_project_info ? project_info.indexed_at : NULL;
    meta.coverage_version = LSM_SEMANTIC_INDEX_VERSION;
    meta.hash_records_complete = true;
    if (!have_project_info ||
        lsm_store_coverage_replace_ex(store, generation->project, generation->coverage,
                                      generation->coverage_count, &meta) != LSM_STORE_OK) {
        ok = false;
    }
    if (have_project_info) {
        lsm_project_free_fields(&project_info);
    }
    lsm_log_info("publish.timing", "block", "coverage_replace", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_pub)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);
    if (fts_wholesale && generation_rebuild_fts(store) != LSM_STORE_OK) {
        ok = false;
    }
    lsm_log_info("publish.timing", "block", "fts", "elapsed_ms", itoa_buf((int)elapsed_ms(t_pub)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);
    if (ok && !lsm_store_check_integrity(store)) {
        ok = false;
    }
    lsm_log_info("publish.timing", "block", "integrity", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_pub)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_pub);
    if (ok && lsm_store_seal_for_atomic_publish(store) != LSM_STORE_OK) {
        ok = false;
    }
    lsm_log_info("publish.timing", "block", "seal", "elapsed_ms", itoa_buf((int)elapsed_ms(t_pub)));
    lsm_store_close(store);
    if (!ok) {
        discard_generation_stage(stage_path);
        free(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    int fin_rc = lsm_pipeline_finalize_staged_generation(
        stage_path, generation->final_db_path, generation->cancelled, destination_known_healthy);
    free(stage_path);
    return fin_rc;
}

char *lsm_pipeline_create_staging_path(const char *final_path) {
    return create_staging_path(final_path);
}

void lsm_pipeline_discard_stage(const char *stage_path) {
    discard_generation_stage(stage_path);
}

/* Shared final leg of every publication, dump-built or delta-patched: the
 * staging file is complete and sealed; remove its sidecars, quarantine the
 * previous generation, and atomically rename. Owns discarding the stage on
 * every failure path. The store handle must already be CLOSED — sidecar
 * removal and rename act on the bare file. */
int lsm_pipeline_finalize_staged_generation(char *stage_path, const char *final_db_path,
                                            atomic_int *cancelled, bool destination_known_healthy) {
    struct timespec t_fin;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_fin);
    if (lsm_remove_db_sidecars(stage_path) != 0) {
        /* This returned PERSIST_FAILED with no log at all, which is how #1620
         * presented: every pass succeeded, the worker exited 0, no error-level
         * line was emitted anywhere, and the user was told "Pipeline failed.
         * Check repo_path exists and contains source files" — pointed at their
         * repository for a filesystem permission problem. A publish that fails
         * must say so. */
        char errno_text[16];
        (void)snprintf(errno_text, sizeof(errno_text), "%d", errno);
        lsm_log_error("finalize.sidecar_removal_failed", "errno", errno_text, "stage", stage_path);
        discard_generation_stage(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    if (cancelled && atomic_load(cancelled)) {
        discard_generation_stage(stage_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    lsm_log_info("finalize.timing", "block", "stage_sidecars", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_fin)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_fin);
    lsm_replacement_prepare_t prepared = {0};
    /* destination_known_healthy: the delta route CLONED this same file and
     * ran complete transactions against the clone minutes ago -- reaching
     * this point is structural-health evidence, and the quick_check the
     * prepare would run is a full-database page scan (measured 35.5s on a
     * kernel-scale generation). A corrupt live DB can never take the delta
     * route: every earlier step fails it into the dump path, whose
     * finalize keeps the check and the quarantine semantics. Sidecars are
     * still removed either way: a replaced DB must never inherit the old
     * generation's WAL. */
    if (destination_known_healthy) {
        if (lsm_remove_db_sidecars(final_db_path) != 0) {
            char errno_text[16];
            (void)snprintf(errno_text, sizeof(errno_text), "%d", errno);
            lsm_log_error("finalize.prepare_failed", "reason", "healthy_sidecar_cleanup", "errno",
                          errno_text, "path", final_db_path);
            lsm_pipeline_discard_stage(stage_path);
            return LSM_PIPELINE_PERSIST_FAILED;
        }
    } else if (prepare_existing_generation_for_replace(final_db_path, &prepared, false) != 0) {
        discard_generation_stage(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
    if (lsm_pipeline_persist_test_take_cancel_after_destination_prepare() && cancelled) {
        atomic_store(cancelled, true);
    }
#endif
    if (cancelled && atomic_load(cancelled)) {
        int rollback_rc = rollback_quarantined_generation(final_db_path, &prepared);
        discard_generation_stage(stage_path);
        return rollback_rc == 0 ? LSM_PIPELINE_ABORT_PRESERVE_DB : LSM_PIPELINE_PERSIST_FAILED;
    }
    lsm_log_info("finalize.timing", "block", "prepare_live", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_fin)));
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_fin);
    if (lsm_rename_replace(stage_path, final_db_path) != 0) {
        char errno_text[16];
        (void)snprintf(errno_text, sizeof(errno_text), "%d", errno);
        lsm_log_error("finalize.rename_failed", "errno", errno_text, "stage", stage_path, "dest",
                      final_db_path);
        (void)rollback_quarantined_generation(final_db_path, &prepared);
        discard_generation_stage(stage_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }
    lsm_log_info("finalize.timing", "block", "rename", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t_fin)));
    return 0;
}

/* Dump graph to SQLite and persist file hashes for incremental indexing. */
static int dump_and_persist_hashes(lsm_pipeline_t *p, const lsm_file_hash_t *baseline_manifest,
                                   int baseline_count, struct timespec *t) {
    lsm_clock_gettime(CLOCK_MONOTONIC, t);
    char *db_path = resolve_db_path(p);
    if (!db_path) {
        return LSM_NOT_FOUND;
    }
    char *db_dir = strdup(db_path);
    if (!db_dir) {
        free(db_path);
        return LSM_NOT_FOUND;
    }
    char *last_slash = strrchr(db_dir, '/');
#ifdef _WIN32
    char *last_backslash = strrchr(db_dir, '\\');
    if (last_backslash && (!last_slash || last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
#endif
    if (last_slash) {
        *last_slash = '\0';
        lsm_mkdir_p(db_dir, LSM_DIR_PERMS);
    }

    lsm_file_hash_t *manifest = NULL;
    int manifest_count = 0;
#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
    lsm_pipeline_persist_test_run_before_final_manifest();
#endif
    if (lsm_pipeline_build_fresh_semantic_manifest(p->project_name, p->repo_path, p->mode,
                                                   &manifest, &manifest_count) != 0) {
        lsm_log_error("pipeline.err", "phase", "semantic_manifest");
        /* db_path and db_dir are this function's strdups; the success tail and
         * the publish-failure return release them, and these two aborts must
         * too -- LSan caught exactly these paths leaking both strings. */
        free(db_dir);
        free(db_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    if (!lsm_pipeline_semantic_manifests_equal(baseline_manifest, baseline_count, manifest,
                                               manifest_count)) {
        lsm_log_warn("pipeline.abort", "reason", "semantic_inputs_changed");
        lsm_pipeline_free_semantic_manifest(manifest, manifest_count);
        free(db_dir);
        free(db_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }

    int cov_total = p->file_errors_count + p->excluded_count + p->ignored_count;
    lsm_coverage_row_t *cov = NULL;
    int cov_count = 0;
    bool coverage_rows_available = cov_total == 0;
    if (cov_total > 0) {
        cov = malloc((size_t)cov_total * sizeof(*cov));
        if (cov) {
            coverage_rows_available = true;
            for (int i = 0; i < p->file_errors_count; i++) {
                cov[cov_count++] = (lsm_coverage_row_t){.rel_path = p->file_errors[i].path,
                                                        .kind = p->file_errors[i].phase,
                                                        .detail = p->file_errors[i].reason};
            }
            for (int i = 0; i < p->excluded_count; i++) {
                cov[cov_count++] = (lsm_coverage_row_t){.rel_path = p->excluded_dirs[i],
                                                        .kind = "not_indexed_dir",
                                                        .detail = "excluded subtree"};
            }
            for (int i = 0; i < p->ignored_count; i++) {
                cov[cov_count++] = (lsm_coverage_row_t){.rel_path = p->ignored_files[i].rel_path,
                                                        .kind = "not_indexed_file",
                                                        .detail = p->ignored_files[i].reason};
            }
        }
    }
    lsm_pipeline_generation_t generation = {
        .gbuf = p->gbuf,
        .final_db_path = db_path,
        .project = p->project_name,
        .cancelled = p->cancelled,
        .manifest = manifest,
        .manifest_count = manifest_count,
        .adr_content = p->saved_adr,
        .coverage = cov,
        .coverage_count = cov_count,
        .coverage_meta =
            {
                .index_mode = pipeline_mode_name(p->mode),
                .recording_status =
                    !coverage_rows_available
                        ? "unavailable"
                        : (p->ignored_total > p->ignored_count ? "truncated" : "complete"),
                .ignored_files_stored = p->ignored_count,
                .ignored_files_total = p->ignored_total,
                .coverage_version = LSM_SEMANTIC_INDEX_VERSION,
                .hash_records_complete = true,
            },
        .surface_rows = p->surface_rows,
        .surface_row_count = p->surface_row_count,
    };

    free(db_dir);
    /* Capture committed counts BEFORE the dump. lsm_gbuf_dump_to_sqlite calls
     * release_gbuf_indexes(), which frees node_by_qn (graph_buffer.c), after
     * which lsm_gbuf_node_count() returns 0. Reading these post-dump left
     * committed_nodes at 0, so the #334 plausibility gate never fired. */
    p->committed_nodes = lsm_gbuf_node_count(p->gbuf);
    p->committed_edges = lsm_gbuf_edge_count(p->gbuf);
    int rc = lsm_pipeline_publish_generation(&generation);
    free(cov);
    lsm_pipeline_free_semantic_manifest(manifest, manifest_count);
    if (rc != 0) {
        /* db_path is this function's strdup (resolve_db_path); every return
         * must release it. LSan on the Linux leg caught exactly this pair of
         * exits leaking. */
        free(db_path);
        return rc;
    }
    lsm_log_info("pass.timing", "pass", "dump_and_persist", "elapsed_ms",
                 itoa_buf((int)elapsed_ms(*t)), "files", itoa_buf(manifest_count));
    if (p->ignored_total > p->ignored_count) {
        lsm_log_warn("index.ignored_capped", "stored", itoa_buf(p->ignored_count), "total",
                     itoa_buf(p->ignored_total));
    }
    free(p->saved_adr);
    p->saved_adr = NULL;

    free(db_path);
    return 0;
}

/* Run githistory pass. */
static int run_githistory(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx) {
    struct timespec t_gh;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t_gh);

    lsm_githistory_result_t gh_result = {0};
    lsm_thread_t gh_thread;
    bool gh_threaded = false;
    gh_compute_arg_t gh_arg = {.repo_path = ctx->repo_path, .result = &gh_result};

    if (p->mode != LSM_MODE_FAST) {
        if (effective_worker_count(true) > SKIP_ONE) {
            if (lsm_thread_create(&gh_thread, 0, gh_compute_thread_fn, &gh_arg) == 0) {
                gh_threaded = true;
            }
        }
        if (!gh_threaded) {
            lsm_pipeline_githistory_compute(ctx->repo_path, &gh_result);
            lsm_log_info("pass.timing", "pass", "githistory_compute", "elapsed_ms",
                         itoa_buf((int)elapsed_ms(t_gh)));
        }
    } else {
        lsm_log_info("pass.skip", "pass", "githistory", "reason", "fast_mode");
    }

    if (gh_threaded) {
        lsm_thread_join(&gh_thread);
        lsm_log_info("pass.timing", "pass", "githistory_compute", "elapsed_ms",
                     itoa_buf((int)elapsed_ms(t_gh)));
    }

    int gh_edges = 0;
    if (gh_result.count > 0 || gh_result.file_temporal_count > 0) {
        gh_edges = lsm_pipeline_githistory_apply(ctx, &gh_result);
    }
    lsm_log_info("pass.done", "pass", "githistory", "commits", itoa_buf(gh_result.commit_count),
                 "edges", itoa_buf(gh_edges));
    free(gh_result.couplings);
    free(gh_result.file_temporal);
    return 0;
}

/* ── Pipeline run ────────────────────────────────────────────────── */

/* Run tests + git history. Returns 0 on success. */
static int run_tests_and_history(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx,
                                 const lsm_file_info_t *files, int file_count) {
    struct timespec t;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t);
    LSM_PROF_START(t_tests);
    int rc = lsm_pipeline_pass_tests(ctx, files, file_count);
    LSM_PROF_END_N("pipeline", "pass_tests", t_tests, file_count);
    lsm_log_info("pass.timing", "pass", "tests", "elapsed_ms", itoa_buf((int)elapsed_ms(t)));
    if (rc == 0 && !check_cancel(p)) {
        LSM_PROF_START(t_gh);
        rc = run_githistory(p, ctx);
        LSM_PROF_END("pipeline", "pass_githistory", t_gh);
    }
    if (check_cancel(p)) {
        return LSM_NOT_FOUND;
    }
    return rc;
}

/* Run tests, git history, predump passes, and dump+persist. */
static int run_post_extraction(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx,
                               const lsm_file_info_t *files, int file_count,
                               const lsm_file_hash_t *baseline_manifest, int baseline_count) {
    int rc = run_tests_and_history(p, ctx, files, file_count);
    if (rc != 0) {
        return rc;
    }

    LSM_PROF_START(t_predump);
    run_predump_passes(p, ctx);
    LSM_PROF_END("pipeline", "3_predump_passes_total", t_predump);

#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
    if (lsm_pipeline_persist_test_take_cancel_after_predump()) {
        atomic_store(p->cancelled, 1);
    }
#endif

    if (check_cancel(p)) {
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }

    struct timespec t;
    LSM_PROF_START(t_dump);
    rc = dump_and_persist_hashes(p, baseline_manifest, baseline_count, &t);
    LSM_PROF_END("pipeline", "4_dump_and_persist", t_dump);
    return rc;
}

#define MIN_FILES_FOR_PARALLEL 50

/* Run structure + extraction passes (parallel or sequential). */
static int run_extraction_phase(lsm_pipeline_t *p, lsm_pipeline_ctx_t *ctx,
                                const lsm_file_info_t *files, int file_count) {
    struct timespec t;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t);
    LSM_PROF_START(t_struct);
    pass_structure(p, files, file_count);
    LSM_PROF_END_N("pipeline", "pass_structure", t_struct, file_count);
    lsm_log_info("pass.timing", "pass", "structure", "elapsed_ms", itoa_buf((int)elapsed_ms(t)));
    if (check_cancel(p)) {
        return LSM_NOT_FOUND;
    }

    int worker_count = effective_worker_count(true);
    LSM_PROF_START(t_extract_total);
    int rc = (worker_count > SKIP_ONE && file_count > MIN_FILES_FOR_PARALLEL)
                 ? run_parallel_pipeline(p, ctx, files, file_count, worker_count, &t)
                 : run_sequential_pipeline(p, ctx, files, file_count, &t);
    LSM_PROF_END_N("pipeline", "2_extraction_total", t_extract_total, file_count);
    if (check_cancel(p)) {
        return LSM_NOT_FOUND;
    }
    return rc;
}

static int lsm_pipeline_run_staged(lsm_pipeline_t *p) {
    if (!p) {
        return LSM_NOT_FOUND;
    }

    LSM_PROF_START(t_pipeline_total);
    struct timespec t0;
    lsm_clock_gettime(CLOCK_MONOTONIC, &t0);
    lsm_path_alias_collection_t *path_aliases = NULL;
    lsm_file_hash_t *baseline_manifest = NULL;
    int baseline_count = 0;
    char **requested_excluded_dirs = NULL;
    int requested_excluded_count = 0;
    lsm_ignored_file_t *requested_ignored_files = NULL;
    int requested_ignored_count = 0;
    int requested_ignored_total = 0;
    bool restore_requested_discovery = false;

    p->mode = p->requested_mode;
    bool mode_promoted = promote_mode_to_existing_coverage(p);

    /* lsm_pipeline_new() may precede the actual run by an arbitrary interval.
     * Refresh once here, then use this exact snapshot for both Branch graph
     * construction and the baseline semantic manifest. */
    if (pipeline_refresh_git_context(p) != 0) {
        return LSM_NOT_FOUND;
    }

    /* C/C++ #define Macro nodes (#375) dominate extraction on macro-dense repos
     * (≈49% of nodes on the Linux kernel), so gate them to full mode — moderate
     * and fast skip them entirely. Set before any extraction dispatch. */
    lsm_set_macro_extraction(p->mode == LSM_MODE_FULL);

    /* Load user-defined extension overrides (fail-open: NULL on error) */
    LSM_PROF_START(t_userconfig);
    p->userconfig = lsm_userconfig_load(p->repo_path);
    lsm_set_user_lang_config(p->userconfig);
    LSM_PROF_END("pipeline", "0_userconfig_load", t_userconfig);

    /* Phase 1: Discover files */
    LSM_PROF_START(t_discover);
    lsm_discover_opts_t opts = {
        .mode = p->requested_mode,
        .ignore_file = NULL,
        .max_file_size = 0,
    };
    lsm_file_info_t *files = NULL;
    int file_count = 0;
    /* Capture skipped subtrees on the pipeline so the MCP layer can report
     * which directories were excluded (#411), plus the individually-ignored
     * files (#963 "purposely not indexed"). Replace any prior lists (e.g. a
     * re-run on the same pipeline) to avoid leaking the previous ones. */
    lsm_discover_free_excluded(p->excluded_dirs, p->excluded_count);
    p->excluded_dirs = NULL;
    p->excluded_count = 0;
    lsm_discover_free_ignored(p->ignored_files, p->ignored_count);
    p->ignored_files = NULL;
    p->ignored_count = 0;
    p->ignored_total = 0;
    int rc = lsm_discover_ex2(p->repo_path, &opts, &files, &file_count, &p->excluded_dirs,
                              &p->excluded_count, &p->ignored_files, &p->ignored_count,
                              &p->ignored_total);
    if (rc != 0) {
        lsm_log_error("pipeline.err", "phase", "discover", "rc", itoa_buf(rc));
    }
    LSM_PROF_END_N("pipeline", "1_discover", t_discover, file_count);
    lsm_log_info("pipeline.discover", "files", itoa_buf(file_count), "elapsed_ms",
                 itoa_buf((int)elapsed_ms(t0)));
    if (rc != 0 || check_cancel(p)) {
        rc = LSM_NOT_FOUND;
        goto cleanup;
    }

    /* Snapshot every semantic input once before routing/extraction. The same
     * bytes drive exact no-op comparison and are checked against a fresh
     * rediscovery immediately before any replacement is published. */
    rc = mode_promoted
             ? lsm_pipeline_build_fresh_semantic_manifest(p->project_name, p->repo_path, p->mode,
                                                          &baseline_manifest, &baseline_count)
             : lsm_pipeline_build_semantic_manifest(p->project_name, p->repo_path, files,
                                                    file_count, p->excluded_dirs, p->excluded_count,
                                                    &p->git_ctx, p->userconfig, &baseline_manifest,
                                                    &baseline_count);
    if (rc != 0) {
        rc = LSM_PIPELINE_ABORT_PRESERVE_DB;
        goto cleanup;
    }

    /* Check for existing DB → try incremental or delete for reindex */
    rc = try_incremental_or_delete_db(p, files, file_count, baseline_manifest, baseline_count,
                                      mode_promoted);
    if (rc == LSM_PIPELINE_ABORT_PRESERVE_DB || rc == LSM_PIPELINE_PERSIST_FAILED) {
        goto cleanup;
    }
    if (rc >= 0) {
        goto cleanup;
    }
    if (rc != LSM_PIPELINE_FORCE_FULL_REINDEX) {
        goto cleanup;
    }

    /* A changed downgrade rebuilds the complete graph at the stored effective
     * mode. Keep the requested discovery lists to report the caller's scope. */
    if (mode_promoted) {
        lsm_discover_free(files, file_count);
        files = NULL;
        file_count = 0;

        requested_excluded_dirs = p->excluded_dirs;
        requested_excluded_count = p->excluded_count;
        requested_ignored_files = p->ignored_files;
        requested_ignored_count = p->ignored_count;
        requested_ignored_total = p->ignored_total;
        restore_requested_discovery = true;

        p->excluded_dirs = NULL;
        p->excluded_count = 0;
        p->ignored_files = NULL;
        p->ignored_count = 0;
        p->ignored_total = 0;

        opts.mode = p->mode;
        rc = lsm_discover_ex2(p->repo_path, &opts, &files, &file_count, &p->excluded_dirs,
                              &p->excluded_count, &p->ignored_files, &p->ignored_count,
                              &p->ignored_total);
        lsm_log_info("pipeline.rediscover", "requested_mode", pipeline_mode_name(p->requested_mode),
                     "effective_mode", pipeline_mode_name(p->mode), "files", itoa_buf(file_count));
        if (rc != 0 || check_cancel(p)) {
            rc = LSM_NOT_FOUND;
            goto cleanup;
        }
    }
    lsm_log_info("pipeline.route", "path", "full");

    /* Phase 2: Create graph buffer and registry */
    p->gbuf = lsm_gbuf_new(p->project_name, p->repo_path);
    p->registry = lsm_registry_new();

    /* Phase 2b: Load build-tool path aliases (tsconfig/jsconfig today). NULL
     * when no usable configs are found — non-TS projects pay nothing. */
    path_aliases =
        lsm_load_path_aliases_excluded(p->repo_path, p->excluded_dirs, p->excluded_count);

    /* Build shared context for pass functions */
    lsm_pipeline_ctx_t ctx = {
        .project_name = p->project_name,
        .repo_path = p->repo_path,
        .gbuf = p->gbuf,
        .registry = p->registry,
        .cancelled = p->cancelled,
        .pipeline = p, /* so passes can record per-file skips (Track B) */
        .mode = (int)p->mode,
        .path_aliases = path_aliases,
        .excluded_dirs = p->excluded_dirs,
        .excluded_count = p->excluded_count,
    };

    rc = run_extraction_phase(p, &ctx, files, file_count);
    if (rc != 0) {
        goto cleanup;
    }

    rc = run_post_extraction(p, &ctx, files, file_count, baseline_manifest, baseline_count);
    if (rc != 0) {
        goto cleanup;
    }

    lsm_log_info("pipeline.done", "nodes", itoa_buf(p->committed_nodes), "edges",
                 itoa_buf(p->committed_edges), "elapsed_ms", itoa_buf((int)elapsed_ms(t0)));
    LSM_PROF_END("pipeline", "TOTAL", t_pipeline_total);

cleanup:
    lsm_pkgmap_free(lsm_pipeline_get_pkgmap());
    lsm_pipeline_set_pkgmap(NULL);
    lsm_discover_free(files, file_count);
    lsm_pipeline_free_semantic_manifest(baseline_manifest, baseline_count);
    lsm_gbuf_free(p->gbuf);
    p->gbuf = NULL;
    lsm_registry_free(p->registry);
    p->registry = NULL;
    lsm_path_alias_collection_free(path_aliases);
    if (restore_requested_discovery) {
        lsm_discover_free_excluded(p->excluded_dirs, p->excluded_count);
        lsm_discover_free_ignored(p->ignored_files, p->ignored_count);
        p->excluded_dirs = requested_excluded_dirs;
        p->excluded_count = requested_excluded_count;
        p->ignored_files = requested_ignored_files;
        p->ignored_count = requested_ignored_count;
        p->ignored_total = requested_ignored_total;
    }
    /* Clear and free user extension config */
    lsm_set_user_lang_config(NULL);
    lsm_userconfig_free(p->userconfig);
    p->userconfig = NULL;
    return rc;
}

static void cleanup_staging_db(const char *path) {
    if (!path) {
        return;
    }
    (void)lsm_unlink(path);
    (void)lsm_remove_db_sidecars(path);
}

static bool ensure_db_parent(const char *path) {
    if (!path) {
        return false;
    }
    char *dir = strdup(path);
    if (!dir) {
        return false;
    }
    char *slash = strrchr(dir, '/');
#ifdef _WIN32
    char *backslash = strrchr(dir, '\\');
    if (backslash && (!slash || backslash > slash)) {
        slash = backslash;
    }
#endif
    if (!slash) {
        free(dir);
        return true;
    }
    *slash = '\0';
    bool ok = dir[0] == '\0' || lsm_mkdir_p(dir, LSM_DIR_PERMS);
    free(dir);
    return ok;
}

static char *create_staging_path(const char *final_path) {
    if (!final_path) {
        return NULL;
    }
    static const char suffix[] = ".stage.XXXXXX";
    size_t final_len = strlen(final_path);
    if (final_len > SIZE_MAX - sizeof(suffix)) {
        return NULL;
    }
    size_t path_size = final_len + sizeof(suffix);
#ifdef _WIN32
    /* The Windows lsm_mkstemp compatibility contract may expand a /tmp/
     * prefix in-place and copies through a 4 KiB scratch path. Give it that
     * full capacity, and reject longer inputs exactly rather than truncating. */
    if (path_size > LSM_SZ_4K) {
        return NULL;
    }
    path_size = LSM_SZ_4K;
#endif
    char *path = (char *)malloc(path_size);
    if (!path) {
        return NULL;
    }
    memcpy(path, final_path, final_len);
    memcpy(path + final_len, suffix, sizeof(suffix));
    int fd = lsm_mkstemp(path);
    if (fd < 0) {
        free(path);
        return NULL;
    }
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
    return path;
}

/* A backup-failed destination may still have the only recoverable WAL or
 * rollback journal. Publication may replace its main file only when no
 * sidecar exists; otherwise fail without mutating the old generation. */
static bool db_sidecars_absent(const char *db_path) {
    if (!db_path || !db_path[0]) {
        return false;
    }
    enum { SIDECAR_PATH_MAX = 4096 };
    char side[SIDECAR_PATH_MAX];
    if (strlen(db_path) > sizeof(side) - sizeof("-journal")) {
        return false;
    }
    static const char *const suffixes[] = {"-wal", "-shm", "-journal"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        int n = snprintf(side, sizeof(side), "%s%s", db_path, suffixes[i]);
        if (n <= 0 || (size_t)n >= sizeof(side)) {
            return false;
        }
        struct stat side_st;
        if (stat(side, &side_st) == 0 || errno != ENOENT) {
            return false;
        }
    }
    return true;
}

/* Ready the real destination to receive the staged generation. Returns 0, or a
 * LSM_PIPELINE_* code the caller propagates.
 *
 * `prepared` records whether the previous destination was moved aside, so a
 * failed rename can put it back. It is zeroed here and is only meaningful on a
 * 0 return. */
static int prepare_publish_destination(const char *final_path, bool final_existed,
                                       bool backup_succeeded, lsm_replacement_prepare_t *prepared) {
    memset(prepared, 0, sizeof(*prepared));
    struct stat current_st;
    bool final_exists_now = stat(final_path, &current_st) == 0;
    if (final_exists_now != final_existed) {
        /* The destination appeared or vanished while we were indexing. Someone
         * else owns it now; leave whatever is there alone. */
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    if (!final_exists_now) {
        /* A crashed generation can leave sidecars without a main file. */
        return lsm_remove_db_sidecars(final_path) == 0 ? 0 : LSM_PIPELINE_PERSIST_FAILED;
    }
    if (!backup_succeeded) {
        /* Sidecars alongside an un-copyable destination may hold the only
         * committed pages; refuse rather than drop them. */
        if (!db_sidecars_absent(final_path)) {
            lsm_log_error("pipeline.err", "phase", "publish", "reason",
                          "backup_failed_sidecars_preserved", "path", final_path);
            return LSM_PIPELINE_PERSIST_FAILED;
        }
        /* The destination could not be copied. If it is not a readable SQLite
         * database it is corrupt, and the publishing rename would destroy the
         * only copy of those bytes -- so move it aside under a fresh .corrupt
         * name first, never overwriting an earlier quarantine. A destination
         * that IS valid (the backup failed for some other reason) is sealed and
         * replaced as usual, never renamed away. */
        return prepare_existing_generation_for_replace(final_path, prepared, true);
    }
    return lsm_store_prepare_path_for_replace(final_path) == LSM_STORE_OK &&
                   lsm_remove_db_sidecars(final_path) == 0
               ? 0
               : LSM_PIPELINE_PERSIST_FAILED;
}

static int seal_staging_db(const char *staging_path) {
    lsm_store_t *store = lsm_store_open_path(staging_path);
    if (!store) {
        return LSM_NOT_FOUND;
    }
    int rc =
        lsm_store_check_integrity(store) && lsm_store_prepare_for_publish(store) == LSM_STORE_OK
            ? 0
            : LSM_NOT_FOUND;
    lsm_store_close(store);
    if (rc == 0 && lsm_remove_db_sidecars(staging_path) != 0) {
        rc = LSM_NOT_FOUND;
    }
    return rc;
}

static int export_after_publish(lsm_pipeline_t *p, const char *final_path) {
    if (p->persistence) {
        LSM_PROF_START(t_art);
        int rc = lsm_artifact_export(final_path, p->repo_path, p->project_name, LSM_ARTIFACT_BEST);
        LSM_PROF_END("persist", "6_artifact_export", t_art);
        if (rc != 0) {
            const char *err = lsm_artifact_export_last_error();
            lsm_log_error("pipeline.err", "phase", "artifact_export", "err", err ? err : "unknown");
        }
        return rc;
    }
    if (p->repo_path && lsm_artifact_exists(p->repo_path)) {
        (void)lsm_artifact_export(final_path, p->repo_path, p->project_name, LSM_ARTIFACT_FAST);
    }
    return 0;
}

int lsm_pipeline_run(lsm_pipeline_t *p) {
    if (!p) {
        return LSM_NOT_FOUND;
    }
    char *final_path = resolve_db_path(p);
    if (!final_path || !ensure_db_parent(final_path)) {
        free(final_path);
        return LSM_NOT_FOUND;
    }
    struct stat final_st;
    bool final_existed = stat(final_path, &final_st) == 0;
    char *staging_path = create_staging_path(final_path);
    if (!staging_path) {
        free(final_path);
        return LSM_NOT_FOUND;
    }

    bool backup_succeeded = false;
    if (final_existed) {
        backup_succeeded = lsm_store_backup_path(final_path, staging_path) == LSM_STORE_OK;
        if (!backup_succeeded) {
            lsm_log_warn("pipeline.stage", "action", "backup_failed_full_rebuild", "path",
                         final_path);
            cleanup_staging_db(staging_path);
        }
    }

    char *configured_db_path = p->db_path;
    p->db_path = strdup(staging_path);
    if (!p->db_path) {
        p->db_path = configured_db_path;
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_NOT_FOUND;
    }
    int rc = lsm_pipeline_run_staged(p);
    free(p->db_path);
    p->db_path = configured_db_path;

    /* Report WHY the run stopped. Everything below happens before the final
     * rename, so the live database is still the previous generation and an
     * abort is genuinely non-destructive -- a caller that cannot tell an
     * aborted run from a failed persist cannot tell whether its data survived.
     * The staging file is discarded on every one of these paths. */
    if (rc != 0) {
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return rc;
    }
    if (check_cancel(p)) {
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }
    if (seal_staging_db(staging_path) != 0) {
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }

    if (p->before_publish_hook) {
        p->before_publish_hook(p, staging_path, p->before_publish_hook_ctx);
    }
    if (check_cancel(p)) {
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_PIPELINE_ABORT_PRESERVE_DB;
    }

    /* A test hook may inspect the DB through SQLite and re-enable WAL mode;
     * seal once more before installing the standalone main file. */
    if (seal_staging_db(staging_path) != 0) {
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }

    lsm_replacement_prepare_t prepared = {0};
    int prepare_rc =
        prepare_publish_destination(final_path, final_existed, backup_succeeded, &prepared);
    if (prepare_rc != 0) {
        lsm_log_error("pipeline.err", "phase", "publish", "path", final_path);
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return prepare_rc;
    }
    if ((p->rename_hook ? p->rename_hook(staging_path, final_path, p->rename_hook_ctx)
                        : lsm_rename_replace(staging_path, final_path)) != 0) {
        lsm_log_error("pipeline.err", "phase", "publish", "path", final_path);
        /* Put a quarantined destination back: the publish did not happen, so
         * leaving the previous generation parked under .corrupt would present
         * the caller with no database at all. */
        (void)rollback_quarantined_generation(final_path, &prepared);
        cleanup_staging_db(staging_path);
        free(staging_path);
        free(final_path);
        return LSM_PIPELINE_PERSIST_FAILED;
    }

    rc = export_after_publish(p, final_path);
    free(staging_path);
    free(final_path);
    return rc;
}
