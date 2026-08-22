/*
 * pass_usages.c — Resolve usages, throws, and read/write edges.
 *
 * For each file, re-extracts and resolves:
 *   - USAGE edges: ordinary identifier references to registered symbols
 *   - CALL_REFERENCE edges: explicit callable-reference syntax (not invocations)
 *   - THROWS/RAISES edges: exception types
 *   - READS/WRITES edges: variable read/write access patterns
 *
 * All three use the same registry lookup strategy. Combined into one pass
 * to avoid triple re-extraction.
 *
 * Depends on: pass_definitions having populated the registry and graph buffer
 */
#include "foundation/constants.h"
#include "foundation/str_util.h" // lsm_json_escape
#include "pipeline/pipeline.h"
#include "pipeline/pipeline_internal.h"
#include "pipeline/lsp_resolve.h"
#include "pipeline/pass_lsp_cross.h"
#include "graph_buffer/graph_buffer.h"
#include "foundation/log.h"
#include "foundation/compat.h"
#include "foundation/compat_fs.h"
#include "foundation/limits.h"
#include "lsm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(LSM_CALL_REFERENCE_LOOKUP_TEST_API) && LSM_CALL_REFERENCE_LOOKUP_TEST_API
#include <stdatomic.h>

static _Atomic uint64_t g_lsp_reference_lookup_rows_examined = 0;

void lsm_pipeline_lsp_reference_lookup_test_note_row(void) {
    atomic_fetch_add_explicit(&g_lsp_reference_lookup_rows_examined, 1, memory_order_relaxed);
}

void lsm_pipeline_lsp_reference_lookup_test_reset(void) {
    atomic_store_explicit(&g_lsp_reference_lookup_rows_examined, 0, memory_order_relaxed);
}

uint64_t lsm_pipeline_lsp_reference_lookup_test_rows_examined(void) {
    return atomic_load_explicit(&g_lsp_reference_lookup_rows_examined, memory_order_relaxed);
}
#endif

/* True for languages whose module QN derives from the CONTAINING DIRECTORY
 * (Java/Go package). MUST match lsm_lang_module_is_dir() (internal/lsm/helpers.c)
 * so same-module resolution keys against the directory-based def-node QNs. */
static bool pu_module_is_dir(LSMLanguage lang) {
    return lang == LSM_LANG_JAVA || lang == LSM_LANG_GO;
}

/* Read file into heap buffer. Caller must free(). */
static char *read_file(const char *path, int *out_len) {
    FILE *f = lsm_fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > lsm_max_file_bytes()) { /* generous, env-configurable cap (B4) */
        (void)fclose(f);
        return NULL;
    }
    /* +pad: tree-sitter lexer lookahead reads past EOF; keep it in-bounds */
    enum { LSM_TS_LOOKAHEAD_PAD = 16 };
    char *buf = malloc((size_t)size + LSM_TS_LOOKAHEAD_PAD);
    if (!buf) {
        (void)fclose(f);
        return NULL;
    }
    size_t nread = fread(buf, SKIP_ONE, size, f);
    (void)fclose(f);
    if (nread > (size_t)size) {
        nread = (size_t)size;
    }
    memset(buf + nread, 0, LSM_TS_LOOKAHEAD_PAD);
    *out_len = (int)nread;
    return buf;
}

static const char *itoa_log(int val) {
    enum { RING_BUF_COUNT = 4, RING_BUF_MASK = 3 };
    static LSM_TLS char bufs[RING_BUF_COUNT][LSM_SZ_32];
    static LSM_TLS int idx = 0;
    int i = idx;
    idx = (idx + SKIP_ONE) & RING_BUF_MASK;
    snprintf(bufs[i], sizeof(bufs[i]), "%d", val);
    return bufs[i];
}

/* Check if an exception name is a "checked" exception (Java-style).
 * Checked: Exception, IOException, etc. (extends Exception, not RuntimeException).
 * Simple heuristic: if name contains "Error" or "Panic", it's a runtime exception. */
static bool is_checked_exception(const char *name) {
    if (!name) {
        return false;
    }
    if (strstr(name, "Error") || strstr(name, "Panic") || strstr(name, "error") ||
        strstr(name, "panic")) {
        return false;
    }
    return true; /* Default: treat as checked */
}

/* Build the same semantic import map used by cross-LSP and parallel resolution.
 * The old sequential-only shortcut treated raw paths such as `./types` as QNs,
 * so duplicate symbol names fell through to an ambiguous unique-name lookup. */
static int build_import_map(lsm_pipeline_ctx_t *ctx, const char *rel_path,
                            const LSMFileResult *result, const char ***out_keys,
                            const char ***out_vals, int *out_count, LSMLanguage lang) {
    return lsm_pxc_build_import_map(ctx->gbuf, ctx->project_name, rel_path, lang, result, out_keys,
                                    out_vals, out_count);
}

static void free_import_map(const char **keys, const char **vals, int count) {
    lsm_pxc_free_import_map(keys, vals, count);
}

/* Find the graph buffer node for an enclosing function QN, falling back to file node. */
static const lsm_gbuf_node_t *find_enclosing_node(lsm_pipeline_ctx_t *ctx, const char *func_qn,
                                                  const char *rel_path) {
    const lsm_gbuf_node_t *node = NULL;
    if (func_qn && func_qn[0]) {
        node = lsm_gbuf_find_by_qn(ctx->gbuf, func_qn);
        /* A class-level reference in a directory-module language carries the
         * DIRECTORY module QN, which hits the shared Folder/Project node —
         * attribute to this file's File node instead (#787). */
        if (lsm_pipeline_node_is_dir_container(node)) {
            node = NULL;
        }
    }
    if (!node) {
        char *file_qn = lsm_pipeline_fqn_compute(ctx->project_name, rel_path, "__file__");
        node = lsm_gbuf_find_by_qn(ctx->gbuf, file_qn);
        free(file_qn);
    }
    return node;
}

/* Resolve USAGE edges for one file's extracted usages. */
static int resolve_usage_edges(lsm_pipeline_ctx_t *ctx, const LSMFileResult *result,
                               const char *rel, const char *module_qn, const char **imp_keys,
                               const char **imp_vals, int imp_count, LSMLanguage lang) {
    int resolved = 0;
    lsm_pipeline_lsp_reference_index_t reference_index = {0};
    bool reference_index_ready =
        lsm_pipeline_lsp_reference_index_build(&result->resolved_calls, &reference_index);
    for (int u = 0; u < result->usages.count; u++) {
        LSMUsage *usage = &result->usages.items[u];
        if (!usage->ref_name) {
            continue;
        }
        const lsm_gbuf_node_t *src = find_enclosing_node(ctx, usage->enclosing_func_qn, rel);
        if (!src) {
            continue;
        }

        const lsm_gbuf_node_t *tgt = NULL;
        bool precise_call_reference = false;
        const LSMResolvedCall *semantic_reference = NULL;
        if (lsm_pipeline_usage_semantic_reference_candidate(usage)) {
            bool allow_tail = lsm_pipeline_lsp_allow_tail_match(lang);
            semantic_reference = lsm_pipeline_find_lsp_reference_indexed_in_graph(
                &result->resolved_calls, reference_index_ready ? &reference_index : NULL, usage,
                allow_tail, ctx->gbuf, ctx->project_name);
            if (semantic_reference &&
                !lsm_pipeline_usage_allows_semantic_reference(usage, semantic_reference)) {
                semantic_reference = NULL;
            }
            if (semantic_reference) {
                tgt = lsm_pipeline_lsp_target_node(ctx->gbuf, ctx->project_name,
                                                   semantic_reference->callee_qn, allow_tail);
                precise_call_reference = lsm_pipeline_node_is_callable_target(tgt);
            }
        }

        /* A syntactically explicit reference remains useful when exact semantic
         * resolution is unavailable, but the textual registry fallback proves
         * only value use—not an occurrence-exact callable target. Emit USAGE in
         * that case; CALL_REFERENCE is reserved for the exact LSP join. */
        if (!tgt) {
            /* An occurrence-exact semantic record owns this reference even when
             * its target is not materialized in the graph (for example, a
             * Kotlin local function). Falling back by raw name here would bind
             * an unrelated same-named declaration. */
            if (semantic_reference) {
                continue;
            }
            lsm_resolution_t res = lsm_registry_resolve(ctx->registry, usage->ref_name, module_qn,
                                                        imp_keys, imp_vals, imp_count);
            if (!res.qualified_name || res.qualified_name[0] == '\0') {
                continue;
            }
            if (!lsm_pipeline_reference_candidate_fallback_allowed(lang, usage, res.strategy,
                                                                   imp_keys, imp_count)) {
                continue;
            }
            tgt = lsm_gbuf_find_by_qn(ctx->gbuf, res.qualified_name);
            if (usage->semantic_reference_blocked && (usage->semantic_reference_local_shadow ||
                                                      lsm_pipeline_node_is_callable_target(tgt))) {
                continue;
            }
        }
        if (!tgt || src->id == tgt->id) {
            continue;
        }

        /* ref_name is sliced source text and can contain quotes/newlines —
         * escape it or the edge properties JSON is malformed. */
        char esc_ref[LSM_SZ_256];
        lsm_json_escape(esc_ref, sizeof(esc_ref), usage->ref_name);
        char uprops[LSM_SZ_512];
        snprintf(uprops, sizeof(uprops), "{\"callee\":\"%s\"}", esc_ref);
        const char *edge_type = precise_call_reference ? "CALL_REFERENCE" : "USAGE";
        lsm_gbuf_insert_edge(ctx->gbuf, src->id, tgt->id, edge_type, uprops);
        resolved++;
    }
    lsm_pipeline_lsp_reference_index_free(&reference_index);
    return resolved;
}

/* Resolve THROWS/RAISES edges for one file's extracted throws. */
static int resolve_throw_edges(lsm_pipeline_ctx_t *ctx, const LSMFileResult *result,
                               const char *rel, const char *module_qn, const char **imp_keys,
                               const char **imp_vals, int imp_count) {
    int resolved = 0;
    for (int t = 0; t < result->throws.count; t++) {
        LSMThrow *thr = &result->throws.items[t];
        if (!thr->exception_name || !thr->enclosing_func_qn) {
            continue;
        }

        const lsm_gbuf_node_t *src = find_enclosing_node(ctx, thr->enclosing_func_qn, rel);
        if (!src) {
            continue;
        }

        const char *edge_type = is_checked_exception(thr->exception_name) ? "THROWS" : "RAISES";
        lsm_resolution_t res = lsm_registry_resolve(ctx->registry, thr->exception_name, module_qn,
                                                    imp_keys, imp_vals, imp_count);

        const lsm_gbuf_node_t *tgt = NULL;
        if (res.qualified_name && res.qualified_name[0]) {
            tgt = lsm_gbuf_find_by_qn(ctx->gbuf, res.qualified_name);
        }
        if (!tgt || src->id == tgt->id) {
            continue;
        }

        lsm_gbuf_insert_edge(ctx->gbuf, src->id, tgt->id, edge_type, "{}");
        resolved++;
    }
    return resolved;
}

/* Resolve READS/WRITES edges for one file's extracted read/write accesses. */
static int resolve_rw_edges(lsm_pipeline_ctx_t *ctx, const LSMFileResult *result, const char *rel,
                            const char *module_qn, const char **imp_keys, const char **imp_vals,
                            int imp_count) {
    int resolved = 0;
    for (int r = 0; r < result->rw.count; r++) {
        LSMReadWrite *rw = &result->rw.items[r];
        if (!rw->var_name) {
            continue;
        }

        const lsm_gbuf_node_t *src = find_enclosing_node(ctx, rw->enclosing_func_qn, rel);
        if (!src) {
            continue;
        }

        lsm_resolution_t res = lsm_registry_resolve(ctx->registry, rw->var_name, module_qn,
                                                    imp_keys, imp_vals, imp_count);
        if (!res.qualified_name || res.qualified_name[0] == '\0') {
            continue;
        }

        const lsm_gbuf_node_t *tgt = lsm_gbuf_find_by_qn(ctx->gbuf, res.qualified_name);
        if (!tgt || src->id == tgt->id) {
            continue;
        }

        const char *edge_type = rw->is_write ? "WRITES" : "READS";
        lsm_gbuf_insert_edge(ctx->gbuf, src->id, tgt->id, edge_type, "{}");
        resolved++;
    }
    return resolved;
}

int lsm_pipeline_pass_usages(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                             int file_count) {
    lsm_log_info("pass.start", "pass", "usages", "files", itoa_log(file_count));

    int usage_resolved = 0;
    int throw_resolved = 0;
    int rw_resolved = 0;
    int errors = 0;

    for (int i = 0; i < file_count; i++) {
        if (lsm_pipeline_check_cancel(ctx)) {
            return LSM_NOT_FOUND;
        }

        const char *path = files[i].path;
        const char *rel = files[i].rel_path;

        LSMFileResult *result = NULL;
        bool result_owned = false;
        if (ctx->result_cache) {
            result = ctx->result_cache[i];
        }
        if (!result) {
            int source_len = 0;
            char *source = read_file(path, &source_len);
            if (!source) {
                errors++;
                continue;
            }
            result = lsm_extract_file(source, source_len, files[i].language, ctx->project_name, rel,
                                      LSM_EXTRACT_BUDGET, NULL, NULL);
            free(source);
            if (!result) {
                errors++;
                continue;
            }
            result_owned = true;
        }

        if (result->usages.count == 0 && result->throws.count == 0 && result->rw.count == 0) {
            if (result_owned) {
                lsm_free_result(result);
            }
            continue;
        }

        const char **imp_keys = NULL;
        const char **imp_vals = NULL;
        int imp_count = 0;
        build_import_map(ctx, rel, result, &imp_keys, &imp_vals, &imp_count, files[i].language);

        char *module_qn = lsm_pipeline_fqn_module_dir(ctx->project_name, rel,
                                                      pu_module_is_dir(files[i].language));

        usage_resolved += resolve_usage_edges(ctx, result, rel, module_qn, imp_keys, imp_vals,
                                              imp_count, files[i].language);
        throw_resolved +=
            resolve_throw_edges(ctx, result, rel, module_qn, imp_keys, imp_vals, imp_count);
        rw_resolved += resolve_rw_edges(ctx, result, rel, module_qn, imp_keys, imp_vals, imp_count);

        free(module_qn);
        free_import_map(imp_keys, imp_vals, imp_count);
        if (result_owned) {
            lsm_free_result(result);
        }
    }

    lsm_log_info("pass.done", "pass", "usages", "usage", itoa_log(usage_resolved), "throws",
                 itoa_log(throw_resolved), "rw", itoa_log(rw_resolved), "errors", itoa_log(errors));
    return 0;
}
