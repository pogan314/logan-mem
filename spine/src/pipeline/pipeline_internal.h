#include "foundation/constants.h"
/*
 * pipeline_internal.h — Internal pipeline state shared between pass files.
 *
 * NOT a public header. Only included by pipeline.c and pass_*.c files.
 * Exposes the pipeline context struct for direct field access by passes.
 */
#ifndef LSM_PIPELINE_INTERNAL_H
#define LSM_PIPELINE_INTERNAL_H

#include "pipeline/pipeline.h"
#include "pipeline/path_alias.h"
#include "graph_buffer/graph_buffer.h"
#include "store/store.h"
#include "discover/discover.h"
#include "discover/userconfig.h"
#include "git/git_context.h"
#include "foundation/hash_table.h"
#include "lsm.h"
#include "lsp/go_lsp.h" /* LSMLSPDef for lsm_parallel_resolve cross-LSP inputs */
#include <stdatomic.h>
#include <string.h>

/* ── Shared pipeline constants ─────────────────────────────────── */

/* Maximum byte budget for tree-sitter extraction per file */
#define LSM_EXTRACT_BUDGET 5000000

/* Route node QN buffer size (must fit __route__METHOD__/full/url/path) */
#define LSM_ROUTE_QN_SIZE 768

/* Incremental integrity failure: abort the run and preserve the existing DB.
 * Distinct from LSM_NOT_FOUND, which the orchestrator uses as the normal
 * "no incremental route; continue with a full index" sentinel. */
#define LSM_PIPELINE_ABORT_PRESERVE_DB (-2)
#define LSM_PIPELINE_FORCE_FULL_REINDEX (-3)
#define LSM_PIPELINE_PERSIST_FAILED (-4)

/* Canonicalize route-path parameter placeholders (":id", "{id}", "<id>",
 * "${...}") to a single "{}" token so that client call sites and server
 * handlers rendezvous on the same Route QN regardless of framework syntax.
 * Parameter names are intentionally discarded ("/u/{id}" and "/u/{slug}" both
 * canonicalize to "/u/{}"). The result never exceeds the input length, so
 * out_sz >= strlen(in) + 1 always suffices. Returns out. */
const char *lsm_route_canon_path(const char *in, char *out, size_t out_sz);

/* True when a graph node is a structural directory container (Folder/Project)
 * rather than a code node. In a directory-based-module language (Java/Go, see
 * lsm_lang_module_is_dir) a file's module QN equals its directory QN, so an
 * enclosing-scope lookup for a CLASS-LEVEL usage/call (enclosing_func_qn ==
 * module_qn) resolves to the ONE Folder/Project node shared by every file in
 * that package. Sourcing an edge there conflates all same-package files into a
 * single source node with an arbitrary file_path (#787). Source-node finders
 * must treat such a hit as a miss and fall back to the per-file File node. */
static inline bool lsm_pipeline_node_is_dir_container(const lsm_gbuf_node_t *node) {
    return node && node->label &&
           (strcmp(node->label, "Folder") == 0 || strcmp(node->label, "Project") == 0);
}

/* Time unit conversions */
#define LSM_NS_PER_SEC 1000000000LL
#define LSM_US_PER_SEC 1000000LL
#define LSM_MS_PER_SEC 1000.0
#define LSM_US_PER_SEC_F 1e6

/* ── Pipeline context (internal) ─────────────────────────────────── */

/* Per-worker manifest collection entry. */
typedef struct {
    char *pkg_name;  /* heap: "@myorg/pkg", "github.com/foo/bar" */
    char *entry_rel; /* heap: "packages/pkg/src/index" (no extension) */
} lsm_pkg_entry_t;

/* Growable array of package entries (per-worker, no thread contention). */
typedef struct {
    lsm_pkg_entry_t *items;
    int count;
    int cap;
} lsm_pkg_entries_t;

void lsm_pkg_entries_init(lsm_pkg_entries_t *e);
void lsm_pkg_entries_free(lsm_pkg_entries_t *e);

/* Shared context passed to each pass function.
 * Derived from lsm_pipeline_t fields during run. */
typedef struct {
    const char *project_name; /* borrowed from pipeline */
    const char *repo_path;    /* borrowed from pipeline */
    lsm_gbuf_t *gbuf;         /* owned by pipeline */
    lsm_registry_t *registry; /* owned by pipeline */
    atomic_int *cancelled;    /* pointer to pipeline's cancelled flag */
    lsm_pipeline_t *pipeline; /* back-pointer for recording per-file skips
                               * (Stage 2 / Track B). May be NULL on paths that
                               * don't record; lsm_pipeline_add_file_error is
                               * NULL-safe. */
    int mode;                 /* lsm_index_mode_t (0=full, 1=moderate, 2=fast, 3=advanced) */

    /* Extraction result cache (sequential pipeline optimization).
     * When non-NULL, pass_definitions stores results here instead of freeing,
     * and pass_calls/usages/semantic reuse cached results instead of re-extracting.
     * Indexed by file position in the files[] array. Owned by pipeline.c. */
    LSMFileResult **result_cache;

    /* Build-tool path aliases (tsconfig/jsconfig today; webpack/vite-style
     * configs are an easy follow-on). NULL when no usable configs were found.
     * Owned by pipeline.c / pipeline_incremental.c. */
    const lsm_path_alias_collection_t *path_aliases;

    /* Directory subtrees excluded during discovery. Borrowed from pipeline.c. */
    char **excluded_dirs;
    int excluded_count;

    /* Sequential cross-LSP registry arena. The lsp_cross pass builds its
     * shared per-language registries here; resolved_calls entries may BORROW
     * strings owned by these registries, and the later calls pass still
     * reads them — so the arena is OWNED and destroyed by
     * run_sequential_pipeline AFTER all passes, never by the lsp_cross pass
     * itself (destroying at pass end was a use-after-free in pass_calls).
     * Mirrors the parallel path, where cross_lsp_arena outlives the fused
     * resolve. */
    LSMArena seq_cross_arena;
    bool seq_cross_arena_live;
    /* Sequential lsp_cross only: the per-file module-QN strings the collected
     * defs (and through them the shared cross registries in seq_cross_arena)
     * borrow. The registries outlive the pass so pass_calls can read borrowed
     * strings -- these must too. Ownership transfers here at the end of the
     * pass; released beside the arena. Freeing them at pass end was a
     * use-after-free first observable on the real-repo corpus tier. */
    char **seq_cross_def_modules;
    int seq_cross_def_module_count;

    /* ObjectScript $$$macro table built from .inc files in the repo (NULL if
     * no ObjectScript include files were found). Owned by pipeline.c. */
    const LSMMacroTable *macro_table;

    /* ObjectScript method-return-type table built from extracted definitions
     * (NULL until pass_calls builds it). Owned by pipeline.c. */
    const LSMReturnTypeTable *return_type_table;
} lsm_pipeline_ctx_t;

/* Transcode an ObjectScript Studio Export XML file and compose every generated
 * UDL class into one cacheable result. The returned result owns all child
 * extraction arenas and is released with the ordinary lsm_free_result(). */
LSMFileResult *lsm_pipeline_extract_objectscript_export(
    const char *source, int source_len, const char *project_name, const char *rel_path,
    const LSMMacroTable *macro_table, const LSMReturnTypeTable *return_type_table);

/* Materialize CONFIGURES edges from one extracted file's env-access carriers.
 * Shared by sequential definition processing and the parallel cache registry. */
int lsm_pipeline_create_env_configures_for_file(lsm_pipeline_ctx_t *ctx,
                                                const LSMFileResult *result, const char *rel);

static inline int lsm_pipeline_relpath_is_excluded(const char *rel_path, char *const *excluded_dirs,
                                                   int excluded_count) {
    if (!rel_path || rel_path[0] == '\0' || !excluded_dirs || excluded_count <= 0) {
        return 0;
    }
    for (int i = 0; i < excluded_count; i++) {
        const char *excluded = excluded_dirs[i];
        if (!excluded || excluded[0] == '\0') {
            continue;
        }
        size_t n = strlen(excluded);
        if (strncmp(rel_path, excluded, n) == 0 && (rel_path[n] == '\0' || rel_path[n] == '/')) {
            return SKIP_ONE;
        }
    }
    return 0;
}

/* Get the current pipeline's package map (NULL if none). */
LSMHashTable *lsm_pipeline_get_pkgmap(void);
void lsm_pipeline_set_pkgmap(LSMHashTable *map);

/* Unified module resolver: relative → pkgmap → fqn_module fallback.
 * Handles bare specifiers via pkgmap lookup with prefix matching.
 * Caller must free() the returned string. */
char *lsm_pipeline_resolve_module(const lsm_pipeline_ctx_t *ctx, const char *source_rel,
                                  const char *module_path);

/* Resolve an import to its in-graph target node, or NULL if unresolvable.
 *
 * Resolution order (first hit wins):
 *   1. Module-path resolution (relative / pkgmap / fqn_module) → existing node.
 *      This preserves the behavior for Python/TS/Go whose module path maps
 *      directly to a sibling Module/File QN.
 *   2. namespace_map[module_path-prefix] → File node QN (Java/Kotlin/C#/PHP
 *      `using`/`import` of a NAMESPACE that the path-based QN cannot express).
 *   3. Symbol-name fallback: the import's last path segment matched against an
 *      in-graph definition node of the same simple name in a different file
 *      (Rust `use crate::util::helper`, Java `import com.example.Util`, ...).
 *
 * `namespace_map` may be NULL (skips step 2).  `source_file_qn` is the importing
 * file's __file__ QN, used to avoid self-imports in step 3. */
const lsm_gbuf_node_t *lsm_pipeline_resolve_import_node(const lsm_pipeline_ctx_t *ctx,
                                                        const char *source_rel,
                                                        const char *source_file_qn,
                                                        const LSMImport *imp,
                                                        LSMHashTable *namespace_map);

/* Build a namespace → File-node-QN map from a set of extraction results.
 * Each result that declared a namespace/package contributes one entry keyed by
 * the namespace string (e.g. "App.Utils", "com.example").  Returns NULL when no
 * results declared a namespace.  Caller frees via lsm_pipeline_namespace_map_free. */
LSMHashTable *lsm_pipeline_namespace_map_build(const char *project_name,
                                               LSMFileResult *const *results,
                                               const char *const *rels, int count);
void lsm_pipeline_namespace_map_free(LSMHashTable *map);

/* Parse a manifest file and collect pkg entries. Returns true if basename matched. */
bool lsm_pkgmap_try_parse(const char *basename, const char *rel_path, const char *source,
                          int source_len, lsm_pkg_entries_t *entries);

/* Merge per-worker entries into a hash table. Returns NULL if no entries. */
LSMHashTable *lsm_pkgmap_build(lsm_pkg_entries_t *worker_entries, int worker_count,
                               const char *project_name);

/* Build pkgmap by reading manifest files from the files array (sequential path). */
int lsm_pkgmap_scan_repo(const char *repo_path, lsm_pkg_entries_t *entries, char **excluded_dirs,
                         int excluded_count);
LSMHashTable *lsm_pkgmap_build_from_repo(const char *repo_path, const lsm_file_info_t *files,
                                         int file_count, const char *project_name,
                                         char **excluded_dirs, int excluded_count);
LSMHashTable *lsm_pkgmap_build_from_files(const lsm_file_info_t *files, int file_count,
                                          const char *project_name);

/* Free pkgmap and all owned strings. */
void lsm_pkgmap_free(LSMHashTable *pkgmap);

/* Check cancellation. Returns non-zero if cancelled. */
static inline int lsm_pipeline_check_cancel(const lsm_pipeline_ctx_t *ctx) {
    return atomic_load(ctx->cancelled) ? LSM_NOT_FOUND : 0;
}

/* ── Testable helpers ────────────────────────────────────────────── */

/* Check if a file path is worth tracking for git history analysis. */
bool lsm_is_trackable_file(const char *path);

/* Check if a file path looks like a test file (language-agnostic). */
bool lsm_is_test_path(const char *path);

/* Check if a function name looks like a test function (language-agnostic). */
bool lsm_is_test_func_name(const char *name);

/* Coupling result from computeChangeCoupling */
typedef struct {
    char file_a[LSM_SZ_512];
    char file_b[LSM_SZ_512];
    int co_change_count;
    double coupling_score;
    /* Unix epoch of the most recent commit that touched both files together.
     * 0 when no timestamp was available (e.g. older callers / popen path
     * without %ct). */
    long long last_co_change;
} lsm_change_coupling_t;

/* Commit data for coupling analysis */
typedef struct {
    char **files;
    int count;
    /* Unix epoch of the commit. 0 means unknown — coupling computation
     * still works but last_co_change on the resulting edge will be 0. */
    long long timestamp;
} lsm_commit_files_t;

/* Per-file temporal metadata. Populated alongside change-coupling so File
 * nodes can carry change_count and last_modified for hotspot / risk
 * analysis queries. */
typedef struct {
    char file_path[LSM_SZ_512];
    int change_count;
    long long last_modified; /* unix epoch of most recent commit */
} lsm_file_temporal_t;

/* Compute change coupling from commit history.
 * Returns number of couplings written to out (up to max_out).
 * Caller owns out[]. */
int lsm_compute_change_coupling(const lsm_commit_files_t *commits, int commit_count,
                                lsm_change_coupling_t *out, int max_out);

/* Go-style implicit interface satisfaction on graph buffer.
 * Finds Interface nodes, matches method sets against Class nodes,
 * creates IMPLEMENTS + OVERRIDE edges. Returns edge count created. */
int lsm_pipeline_implements_go(lsm_pipeline_ctx_t *ctx);

/* Edge type for an explicit base-class relation, keyed off the resolved
 * TARGET node's label: Interface → IMPLEMENTS, anything else → INHERITS.
 * The single decision point for BOTH the sequential semantic pass and the
 * parallel per-file resolve — the two venues must never diverge. */
const char *lsm_semantic_base_edge_type(const lsm_gbuf_node_t *base_node);

/* Explicit-language override detection on the full graph (serial tail).
 * For every IMPLEMENTS/INHERITS edge whose source is a non-Go class, matches
 * the class's DEFINES_METHOD children by name against the base's and creates
 * Method→Method OVERRIDE edges (Java @Override, TS/C#/Kotlin override, PHP
 * redefinition). Go is excluded: implicit satisfaction already covers it.
 * Returns edge count created. */
int lsm_pipeline_override_explicit(lsm_pipeline_ctx_t *ctx);

/* ── Git diff helpers (pass_gitdiff.c) ───────────────────────────── */

typedef struct {
    char status[LSM_SZ_4]; /* M/A/D/R */ /* "M", "A", "D", "R" */
    char path[LSM_SZ_512];
    char old_path[LSM_SZ_512]; /* non-empty only for renames */
} lsm_changed_file_t;

/* lsm_changed_hunk_t + lsm_parse_hunks moved to pipeline.h (public — consumed
 * by src/mcp/mcp.c's detect_changes for line-scoped seed detection). Visible
 * here via the `#include "pipeline/pipeline.h"` above. */

/* Parse git diff --name-status output. Returns count written to out. */
int lsm_parse_name_status(const char *output, lsm_changed_file_t *out, int max_out);

/* Parse "start,count" or "start" → (start, count). */
void lsm_parse_range(const char *s, int *out_start, int *out_count);

/* ── Config helpers (pass_configures.c) ──────────────────────────── */

/* Check if a string looks like an environment variable name
 * (uppercase + underscore + digits, at least 2 chars with uppercase). */
bool lsm_is_env_var_name(const char *s);

/* Normalize a config key: split camelCase/snake/dots, lowercase.
 * Writes normalized form to norm_out (underscore-joined).
 * Returns token count. tokens_out[] receives borrowed pointers into norm_out. */
int lsm_normalize_config_key(const char *key, char *norm_out, size_t norm_sz);

/* Check if a file path has a config file extension (.toml, .yaml, .env, etc.) */
bool lsm_has_config_extension(const char *path);

/* ── Enrichment helpers (pass_enrichment.c) ──────────────────────── */

/* Split camelCase string on lowercase→uppercase transitions.
 * Writes substrings to out[]. Returns count. Caller must free each out[i]. */
int lsm_split_camel_case(const char *s, char **out, int max_out);

/* Tokenize a decorator into lowercase words, filtering stopwords.
 * E.g. "@login_required" → ["login", "required"].
 * Writes words to out[]. Returns count. Caller must free each out[i]. */
int lsm_tokenize_decorator(const char *dec, char **out, int max_out);

/* ── Compile commands helpers (pass_compile_commands.c) ──────────── */

typedef struct {
    char **include_paths;
    int include_count;
    char **defines;
    int define_count;
    char standard[LSM_SZ_32];
} lsm_compile_flags_t;

/* Split a shell command string into arguments (handles quoting).
 * Writes args to out[]. Returns count. Caller must free each out[i]. */
int lsm_split_command(const char *cmd, char **out, int max_out);

/* Extract -I, -isystem, -D, -std= flags from compiler arguments.
 * Caller must free result with lsm_compile_flags_free(). */
lsm_compile_flags_t *lsm_extract_flags(const char **args, int argc, const char *directory);

/* Free a compile_flags_t allocated by lsm_extract_flags(). */
void lsm_compile_flags_free(lsm_compile_flags_t *f);

/* Parse compile_commands.json content. Returns map as parallel arrays.
 * out_paths[i] is the relative file path, out_flags[i] is its flags.
 * Returns count. Caller must free out_paths[i] and lsm_compile_flags_free(out_flags[i]). */
int lsm_parse_compile_commands(const char *json_data, const char *repo_path, char ***out_paths,
                               lsm_compile_flags_t ***out_flags);

/* ── Infrascan helpers (pass_infrascan.c) ─────────────────────────── */

/* File identification helpers */
bool lsm_is_dockerfile(const char *name);
bool lsm_is_compose_file(const char *name);
bool lsm_is_cloudbuild_file(const char *name);
bool lsm_is_env_file(const char *name);
bool lsm_is_shell_script(const char *name, const char *ext);
bool lsm_is_kustomize_file(const char *name);
bool lsm_is_k8s_manifest(const char *name, const char *content);

/* Secret detection */
bool lsm_is_secret_binding(const char *key, const char *value);
bool lsm_is_secret_value(const char *value);

/* Clean JSON array brackets from CMD/ENTRYPOINT values.
 * E.g. ["./app", "--flag"] → ./app --flag
 * Writes result to out (up to out_sz). */
void lsm_clean_json_brackets(const char *s, char *out, size_t out_sz);

/* Key-value pair for environment variables / config entries */
typedef struct {
    char key[LSM_SZ_128];
    char value[LSM_SZ_512];
} lsm_env_kv_t;

/* Dockerfile parsing result */
typedef struct {
    char base_image[LSM_SZ_256];
    char stage_images[LSM_SZ_16][LSM_SZ_256];
    char stage_names[LSM_SZ_16][LSM_SZ_128];
    int stage_count;
    char exposed_ports[LSM_SZ_16][LSM_SZ_32];
    int port_count;
    lsm_env_kv_t env_vars[LSM_SZ_64];
    int env_count;
    char build_args[LSM_SZ_32][LSM_SZ_128];
    int build_arg_count;
    char workdir[LSM_SZ_256];
    char cmd[LSM_SZ_512];
    char entrypoint[LSM_SZ_512];
    char healthcheck[LSM_SZ_512];
    char user[LSM_SZ_64];
} lsm_dockerfile_result_t;

/* Dotenv parsing result */
typedef struct {
    lsm_env_kv_t env_vars[LSM_SZ_64];
    int env_count;
} lsm_dotenv_result_t;

/* Shell script parsing result */
typedef struct {
    char shebang[LSM_SZ_256];
    lsm_env_kv_t env_vars[LSM_SZ_64];
    int env_count;
    char sources[LSM_SZ_16][LSM_SZ_256];
    int source_count;
    char docker_cmds[LSM_SZ_16][LSM_SZ_256];
    int docker_cmd_count;
} lsm_shell_result_t;

/* Terraform variable */
typedef struct {
    char name[LSM_SZ_128];
    char type[LSM_SZ_64];
    char default_val[LSM_SZ_256];
    char description[LSM_SZ_256];
} lsm_tf_variable_t;

/* Terraform resource / data source */
typedef struct {
    char type[LSM_SZ_128];
    char name[LSM_SZ_128];
} lsm_tf_resource_t;

/* Terraform module */
typedef struct {
    char tf_name[LSM_SZ_128];
    char source[LSM_SZ_256];
} lsm_tf_module_t;

/* Terraform parsing result */
typedef struct {
    lsm_tf_resource_t resources[LSM_SZ_32];
    int resource_count;
    lsm_tf_variable_t variables[LSM_SZ_32];
    int variable_count;
    char outputs[LSM_SZ_32][LSM_SZ_128];
    int output_count;
    char providers[LSM_SZ_16][LSM_SZ_128];
    int provider_count;
    lsm_tf_module_t modules[LSM_SZ_16];
    int module_count;
    lsm_tf_resource_t data_sources[LSM_SZ_16];
    int data_source_count;
    char backend[LSM_SZ_128];
    bool has_locals;
} lsm_terraform_result_t;

/* Parse a Dockerfile from source text. Returns 0 if parsed, -1 if empty/invalid. */
int lsm_parse_dockerfile_source(const char *source, lsm_dockerfile_result_t *out);

/* Parse a .env file from source text. Returns 0 if parsed, -1 if empty. */
int lsm_parse_dotenv_source(const char *source, lsm_dotenv_result_t *out);

/* Parse a shell script from source text. Returns 0 if parsed, -1 if empty. */
int lsm_parse_shell_source(const char *source, lsm_shell_result_t *out);

/* Parse a Terraform file from source text. Returns 0 if parsed, -1 if empty. */
int lsm_parse_terraform_source(const char *source, lsm_terraform_result_t *out);

/* Helm Chart.yaml parse result: chart name + dependency chart names (#338). */
enum { LSM_HELM_MAX_DEPS = 128, LSM_HELM_NAME_MAX = 128 };
typedef struct {
    char chart_name[LSM_HELM_NAME_MAX];
    char deps[LSM_HELM_MAX_DEPS][LSM_HELM_NAME_MAX];
    int dep_count;
} lsm_helm_chart_t;

/* Parse a Helm Chart.yaml: top-level `name:` and `dependencies:` list names.
 * Returns 0 if parsed (name or deps found), -1 otherwise. */
int lsm_parse_helm_chart(const char *source, lsm_helm_chart_t *out);

/* Build an infrastructure QN. Caller must free the returned string. */
char *lsm_infra_qn(const char *project_name, const char *rel_path, const char *infra_type,
                   const char *service_name);

/* ── Parallel pipeline prototypes (pass_parallel.c) ─────────────── */

/* Phase 3A: Parallel extract + create definition nodes.
 * Each worker creates nodes in a per-worker gbuf, then merges into ctx->gbuf.
 * Caches LSMFileResult* in result_cache[file_idx] for reuse in Phase 3B/4.
 * shared_ids provides globally unique node/edge IDs across workers. */

/* Source-retention tuning for lsm_parallel_extract_ex. Zero-valued byte caps
 * mean "use the derived default" (RAM-fraction total, clamped to an absolute
 * ceiling; modest per-file cap); LSM_RETAIN_TOTAL_MB / LSM_RETAIN_PER_FILE_MB
 * override those. retain_sources_set=false keeps the default retain policy. */
typedef struct {
    bool retain_sources;
    bool retain_sources_set; /* false keeps the default retain_sources policy */
    size_t retain_total_budget_bytes;
    size_t retain_per_file_max_bytes;
} lsm_parallel_extract_opts_t;

int lsm_parallel_extract_ex(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count,
                            LSMFileResult **result_cache, _Atomic int64_t *shared_ids,
                            int worker_count, const lsm_parallel_extract_opts_t *opts);
int lsm_parallel_extract(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count,
                         LSMFileResult **result_cache, _Atomic int64_t *shared_ids,
                         int worker_count);

/* Phase 3B: Serial registry build from cached extraction results.
 * Creates DEFINES, DEFINES_METHOD, and IMPORTS edges in ctx->gbuf.
 * Registers callable symbols (Function/Method/Class) in ctx->registry. */
int lsm_build_registry_from_cache(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                                  int file_count, LSMFileResult **result_cache);

/* Phase 4: Parallel call/usage/semantic resolution.
 * Each worker resolves calls, usages, throws, rw, inherits, decorates,
 * and implements edges into per-worker edge bufs, then merges.
 * Runs Go-style implicit IMPLEMENTS as serial post-step. */
/* Opaque module-def index — defined in pass_lsp_cross.c. Forward-declared
 * here so we can include it in lsm_parallel_resolve's signature without
 * pulling the pass header into every consumer of pipeline_internal.h. */
struct LSMModuleDefIndex;

/* lsm_parallel_resolve's cross_registries param is typed `void*` to avoid
 * pulling lsp/go_lsp.h into every TU that includes pipeline_internal.h.
 * Callers cast a LSMCrossLspRegistries* (defined in pass_lsp_cross.h). */

int lsm_parallel_resolve(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count,
                         LSMFileResult **result_cache, _Atomic int64_t *shared_ids,
                         int worker_count,
                         /* Cross-file LSP inputs — pre-built once by the caller and
                          * shared read-only across workers (typed non-const to match
                          * the existing lsm_run_X_lsp_cross signatures the resolve
                          * worker forwards them to). Pass NULL/0/NULL to skip. */
                         LSMLSPDef *all_defs, int def_count, char *const *def_modules,
                         /* Optional inverted index module_qn → defs[] — fallback
                          * path when there's no pre-built registry for this lang. */
                         struct LSMModuleDefIndex *module_def_index,
                         /* Optional Tier 2 full: pre-built per-language registries.
                          * For each language with a non-NULL entry, workers use the
                          * lsm_run_X_lsp_cross_with_registry fast path (skip per-
                          * file registry build entirely). Falls back to the filter
                          * + per-file build path when entry is NULL or struct is NULL.
                          * Typed as void* here to dodge the typedef/tag ordering
                          * problem — pass_parallel.c casts back to LSMCrossLspRegistries*. */
                         void *cross_registries);

/* Post-merge: create Route nodes for HTTP_CALLS/ASYNC_CALLS edges that
 * have url_path in properties but point to library functions instead of routes.
 * Re-targets these edges to Route nodes for cross-service traversal. */
void lsm_pipeline_create_route_nodes(lsm_gbuf_t *gb);

/* ── Pass function prototypes ────────────────────────────────────── */

int lsm_pipeline_pass_definitions(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                                  int file_count);

int lsm_pipeline_pass_k8s(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count);

int lsm_pipeline_pass_calls(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count);

/* Cross-file LSP type-aware call resolution pass. Augments per-file
 * resolved_calls with cross-file resolutions before call edges are emitted.
 * Implementation: src/pipeline/pass_lsp_cross.c. */
int lsm_pipeline_pass_lsp_cross(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                                int file_count, LSMFileResult **cache);

/* Sub-passes called from pass_calls: pattern-based edge extraction */
void lsm_pipeline_pass_fastapi_depends(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                                       int file_count);

int lsm_pipeline_pass_usages(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count);

int lsm_pipeline_pass_semantic(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files,
                               int file_count);

int lsm_pipeline_pass_tests(lsm_pipeline_ctx_t *ctx, const lsm_file_info_t *files, int file_count);

int lsm_pipeline_pass_githistory(lsm_pipeline_ctx_t *ctx);

/* Pre-computed git history result for fused post-pass parallelism. */
typedef struct {
    lsm_change_coupling_t *couplings;
    int count;
    int commit_count;
    /* Per-file temporal data (change_count + last_modified) for File nodes.
     * NULL when the history pass had no commits to analyse. */
    lsm_file_temporal_t *file_temporal;
    int file_temporal_count;
} lsm_githistory_result_t;

/* Compute change couplings without touching the graph buffer.
 * Can run on a separate thread while other passes use the gbuf. */
int lsm_pipeline_githistory_compute(const char *repo_path, lsm_githistory_result_t *result);

/* Apply pre-computed couplings to the graph buffer (main thread only). */
int lsm_pipeline_githistory_apply(lsm_pipeline_ctx_t *ctx, const lsm_githistory_result_t *result);

/* Pre-dump pass: decorator tags enrichment (operates on gbuf). */
int lsm_pipeline_pass_decorator_tags(lsm_gbuf_t *gbuf, const char *project);

/* Pre-dump pass: config ↔ code linking. */
int lsm_pipeline_pass_configlink(lsm_pipeline_ctx_t *ctx);

/* Pre-dump pass: SIMILAR_TO edges via MinHash fingerprinting. */
int lsm_pipeline_pass_similarity(lsm_pipeline_ctx_t *ctx);

/* Pre-dump pass: SEMANTICALLY_RELATED edges via algorithmic embeddings.
 * Runs in MODERATE/FULL/ADVANCED modes; skipped in FAST (see run_predump_passes). */
int lsm_pipeline_pass_semantic_edges(lsm_pipeline_ctx_t *ctx);

/* Pre-dump pass: interprocedural complexity propagation (Tier B).
 * Propagates per-function loop_depth along CALLS edges into a transitive
 * worst-case nested-loop estimate (transitive_loop_depth) and flags call-graph
 * cycles (recursive). Runs on the graph buffer before the dump. */
void lsm_pipeline_pass_complexity(lsm_pipeline_ctx_t *ctx);

/* ── Env URL scanner (pass_envscan.c) ────────────────────────────── */

typedef struct {
    char key[LSM_SZ_128];
    char value[LSM_SZ_512];
    char file_path[LSM_SZ_256];
} lsm_env_binding_t;

/* Scan a project directory for environment variable assignments with URL values.
 * Walks the filesystem, scans Dockerfiles, shell scripts, .env, YAML, TOML,
 * Terraform, and .properties files. Filters out secrets.
 * Returns number of bindings written to out (up to max_out).
 * NOTE: this walker currently has no production callers — it is exercised
 * only by tests. The _excluded variant honors discovery exclusions for
 * consistency with the pkgmap/path-alias walks (#792); the plain variant
 * scans unexcluded (NULL exclusion list). */
int lsm_scan_project_env_urls(const char *root_path, lsm_env_binding_t *out, int max_out);
int lsm_scan_project_env_urls_excluded(const char *root_path, lsm_env_binding_t *out, int max_out,
                                       char **excluded_dirs, int excluded_count);

/* ── Incremental pipeline (pipeline_incremental.c) ───────────────── */

/* Run incremental re-index on an existing disk DB.
 * Classifies files by mtime+size, deletes changed nodes, re-parses changed
 * files, merges into disk DB. Returns 0 on success. */
int lsm_pipeline_run_incremental(lsm_pipeline_t *p, const char *db_path, lsm_file_info_t *files,
                                 int file_count, const lsm_file_hash_t *baseline_manifest,
                                 int baseline_count, bool force_full_on_mismatch);

/* Exact semantic inputs for no-op/forced-full routing. The manifest contains
 * every discovered source plus repository controls actually consumed by
 * discovery, package mapping, path aliases, and extension overrides. */
#define LSM_SEMANTIC_INPUT_PREFIX ".logan-spine/.semantic-input/"
#define LSM_SEMANTIC_INPUT_GIT_CONTEXT LSM_SEMANTIC_INPUT_PREFIX "git-context-v1"
#define LSM_SEMANTIC_INPUT_GLOBAL_CONFIG LSM_SEMANTIC_INPUT_PREFIX "global-extension-config-v1"
#define LSM_SEMANTIC_INPUT_PROJECT_CONFIG LSM_SEMANTIC_INPUT_PREFIX "project-extension-config-v1"

int lsm_pipeline_build_semantic_manifest(const char *project, const char *repo_path,
                                         const lsm_file_info_t *files, int file_count,
                                         char **excluded_dirs, int excluded_count,
                                         const lsm_git_context_t *git_ctx,
                                         const lsm_userconfig_t *userconfig, lsm_file_hash_t **out,
                                         int *out_count);
void lsm_pipeline_free_semantic_manifest(lsm_file_hash_t *manifest, int count);
bool lsm_pipeline_semantic_manifests_equal(const lsm_file_hash_t *left, int left_count,
                                           const lsm_file_hash_t *right, int right_count);
/* Re-run discovery and hash its exact semantic inputs. Used at the publication
 * boundary so late additions/deletions cannot escape a frozen file list. */
int lsm_pipeline_build_fresh_semantic_manifest(const char *project, const char *repo_path, int mode,
                                               lsm_file_hash_t **out, int *out_count);

/* Compatibility contract persisted in coverage metadata. Increment when a
 * graph/manifest semantic change makes prior exact-input indexes unsafe. */
enum { LSM_SEMANTIC_INDEX_VERSION = 3 };

typedef struct {
    lsm_gbuf_t *gbuf;
    const char *final_db_path;
    const char *project;
    atomic_int *cancelled;
    const lsm_file_hash_t *manifest;
    int manifest_count;
    const char *adr_content;
    const lsm_coverage_row_t *coverage;
    int coverage_count;
    lsm_coverage_meta_t coverage_meta;
    /* Per-file LSP surfaces for the generation being published (may be NULL:
     * cross-LSP disabled, or a caller that has none). Written into the
     * staging store alongside the manifest so surface data and graph always
     * belong to the same generation. */
    const lsm_lsp_surface_row_t *surface_rows;
    int surface_row_count;
    /* True when the caller already wrote this generation's surface rows
     * into the staging store (delta patch); publish then skips the
     * wholesale delete+rewrite. */
    bool surfaces_in_place;
} lsm_pipeline_generation_t;

/* Serialize and fully populate a sibling staging database, then atomically
 * replace final_db_path. The old generation is untouched on every failure or
 * cancellation observed before the rename commit point. */
int lsm_pipeline_publish_generation(const lsm_pipeline_generation_t *generation);
/* Final leg shared by the dump-built and delta-patched publication paths:
 * sidecar removal, previous-generation quarantine, atomic rename. The stage
 * must be complete and sealed with its store handle closed. Discards the
 * stage on every failure. Does NOT free stage_path. */
int lsm_pipeline_finalize_staged_generation(char *stage_path, const char *final_db_path,
                                            atomic_int *cancelled, bool destination_known_healthy);
/* Metadata writes + FTS policy + integrity + seal + finalize for an
 * already-materialized staging DB. Takes ownership of stage_path. The dump
 * path passes fts_wholesale=true; the delta path passes false (its patch
 * wrote row-level FTS inserts). generation->gbuf is not read here. */
int lsm_pipeline_publish_staged(char *stage_path, const lsm_pipeline_generation_t *generation,
                                bool fts_wholesale, bool destination_known_healthy);

/* mkstemp-minted staging sibling of final_path (exported for the delta
 * executor; the dump path uses it internally). malloc'd, caller frees. */
char *lsm_pipeline_create_staging_path(const char *final_path);

/* ── Delta-repair staging primitives (pipeline_delta.c) ──────────
 * Closure-route-only subsystem: clone the live generation, patch exactly
 * the repaired node/edge set, publish through the shared finalize leg. */
typedef struct {
    char *source_qn;
    char *target_qn;
    char *type;
    char *props;
} lsm_delta_saved_edge_t;

int lsm_delta_stage_clone(const char *final_db_path, char **out_stage_path);
int lsm_delta_snapshot_inbound(lsm_store_t *store, const char *project, const char *const *paths,
                               int path_count, lsm_delta_saved_edge_t **out, int *out_count);
void lsm_delta_free_snapshot(lsm_delta_saved_edge_t *items, int count);
int lsm_delta_purge(lsm_store_t *store, const char *project, const char *const *paths,
                    int path_count);
/* Pre-seed proxy nodes with their REAL database ids and move the gbuf id
 * watermark above MAX(id); returns that max id, or -1 on failure. */
int64_t lsm_delta_preseed(lsm_store_t *store, const char *project, lsm_gbuf_t *gbuf);
int lsm_delta_patch(lsm_store_t *store, const char *project, lsm_gbuf_t *gbuf, int64_t max_db_id,
                    const lsm_delta_saved_edge_t *snapshot, int snapshot_count);
/* discard helper shared with the delta executor (unlink stage + sidecars). */
void lsm_pipeline_discard_stage(const char *stage_path);
/* Hand the pipeline the per-file LSP-surface rows serialized at the
 * collect_all_defs seam (the only moment the result cache is alive).
 * Takes ownership; dump_and_persist_hashes writes them into the staging
 * store and lsm_pipeline_free releases them. Passing NULL/0 clears. */
void lsm_pipeline_set_lsp_surfaces(lsm_pipeline_t *p, lsm_lsp_surface_row_t *rows, int count);

/* Pipeline accessors for incremental use */
const char *lsm_pipeline_repo_path(const lsm_pipeline_t *p);
atomic_int *lsm_pipeline_cancelled_ptr(lsm_pipeline_t *p);
/* Record committed graph size (#334 gate axis) from the incremental path,
 * which cannot see the opaque lsm_pipeline struct. Call before the dump. */
void lsm_pipeline_set_committed_counts(lsm_pipeline_t *p, int nodes, int edges);

/* Test seam: invoked after a complete staging DB is sealed and immediately
 * before the cancellation check + atomic replace. Not part of the public API. */
void lsm_pipeline_set_before_publish_hook_for_tests(
    lsm_pipeline_t *p, void (*hook)(lsm_pipeline_t *, const char *, void *), void *ctx);
void lsm_pipeline_set_rename_hook_for_tests(lsm_pipeline_t *p,
                                            int (*hook)(const char *, const char *, void *),
                                            void *ctx);

/* Synchronous thread-local seam for deterministic cross-repo cancellation
 * tests. The callback runs immediately after a CROSS_* edge is committed and
 * is never retained; it must not re-enter cross-repo matching. */
typedef void (*lsm_cross_repo_after_insert_test_hook_t)(const char *project, const char *edge_type,
                                                        void *context);
void lsm_cross_repo_set_after_insert_hook_for_tests(lsm_cross_repo_after_insert_test_hook_t hook,
                                                    void *context);

/* Parse a gRPC stub call "<service-stub>.<method>" into the canonical proto
 * service name + method. Returns true ONLY when a recognized gRPC stub/client
 * suffix is present (the stub-type signal that gates Route emission, #294).
 * Exposed for testing. */
bool extract_grpc_service_method(const char *callee, char *service, size_t srv_sz, char *method,
                                 size_t meth_sz);

/* Extraction back-pressure observability (pass_parallel.c): nap-cycle counter
 * for the over-budget collect+nap gate. Test hook — asserts the gate stops
 * re-paying the nap tax once a full cycle failed to reclaim under budget
 * (futile: the resident floor, not transients, holds the memory). */
long lsm_pp_bp_nap_cycles(void);
void lsm_pp_bp_nap_cycles_reset(void);

/* Number of resolved-call rows handed to the parallel resolver's linear LSP
 * fallback since the last reset. Test observability for occurrence-index
 * coverage; deterministic and independent of wall-clock timing. */
uint64_t lsm_pp_lsp_linear_fallback_rows(void);
void lsm_pp_lsp_linear_fallback_rows_reset(void);

#if defined(LSM_CALL_REFERENCE_LOOKUP_TEST_API) && LSM_CALL_REFERENCE_LOOKUP_TEST_API
/* Deterministic test-only operation count for the shared semantic-reference
 * matcher used by both sequential and fused-parallel usage materialization. */
void lsm_pipeline_lsp_reference_lookup_test_reset(void);
uint64_t lsm_pipeline_lsp_reference_lookup_test_rows_examined(void);
#endif

#if defined(LSM_INCREMENTAL_TEST_API) && LSM_INCREMENTAL_TEST_API
typedef enum {
    LSM_INCREMENTAL_ROUTE_NONE = 0,
    LSM_INCREMENTAL_ROUTE_NOOP,
    LSM_INCREMENTAL_ROUTE_FORCED_FULL,
    LSM_INCREMENTAL_ROUTE_LEGACY_PARTIAL,
    LSM_INCREMENTAL_ROUTE_CLOSURE_REPAIR,
} lsm_incremental_route_t;

/* Deterministic one-shot fault injection for the incremental-parallel result
 * cache allocation. Reset explicitly so one test cannot affect another. */
void lsm_pipeline_incremental_test_fail_result_cache_alloc_once(void);
void lsm_pipeline_incremental_test_force_legacy_partial_once(void);
void lsm_pipeline_incremental_test_fail_after_stage_dump_once(void);
void lsm_pipeline_incremental_test_cancel_after_predump_once(void);
void lsm_pipeline_incremental_test_cancel_after_destination_prepare_once(void);
void lsm_pipeline_incremental_test_fail_adr_capture_once(void);
typedef void (*lsm_pipeline_test_hook_fn)(void *userdata);
void lsm_pipeline_incremental_test_before_final_manifest_once(lsm_pipeline_test_hook_fn hook,
                                                              void *userdata);
lsm_incremental_route_t lsm_pipeline_incremental_test_last_route(void);
void lsm_pipeline_incremental_test_reset_faults(void);

/* Shared persistence-hook plumbing. Tests use the incremental facade above so
 * one reset covers route, extraction, and publication faults. */
bool lsm_pipeline_persist_test_take_failure_after_stage_dump(void);
bool lsm_pipeline_persist_test_take_cancel_after_predump(void);
bool lsm_pipeline_persist_test_take_cancel_after_destination_prepare(void);
void lsm_pipeline_persist_test_run_before_final_manifest(void);
void lsm_pipeline_persist_test_reset_faults(void);
#endif

#endif /* LSM_PIPELINE_INTERNAL_H */
