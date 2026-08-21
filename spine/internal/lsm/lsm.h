#ifndef LSM_H
#define LSM_H

#include <stdint.h>
#include <stdbool.h>
#include "arena.h"
#include "tree_sitter/api.h"

// Language enum mirrors lang.Language in Go.
// Order must match lang_specs.c tables.
typedef enum {
    LSM_LANG_GO = 0,
    LSM_LANG_PYTHON,
    LSM_LANG_JAVASCRIPT,
    LSM_LANG_TYPESCRIPT,
    LSM_LANG_TSX,
    LSM_LANG_RUST,
    LSM_LANG_JAVA,
    LSM_LANG_CPP,
    LSM_LANG_CSHARP,
    LSM_LANG_PHP,
    LSM_LANG_LUA,
    LSM_LANG_SCALA,
    LSM_LANG_KOTLIN,
    LSM_LANG_RUBY,
    LSM_LANG_C,
    LSM_LANG_BASH,
    LSM_LANG_ZIG,
    LSM_LANG_ELIXIR,
    LSM_LANG_HASKELL,
    LSM_LANG_OCAML,
    LSM_LANG_OBJC,
    LSM_LANG_SWIFT,
    LSM_LANG_DART,
    LSM_LANG_PERL,
    LSM_LANG_GROOVY,
    LSM_LANG_ERLANG,
    LSM_LANG_R,
    LSM_LANG_HTML,
    LSM_LANG_CSS,
    LSM_LANG_SCSS,
    LSM_LANG_YAML,
    LSM_LANG_TOML,
    LSM_LANG_HCL,
    LSM_LANG_SQL,
    LSM_LANG_DOCKERFILE,
    // New languages (v0.5 expansion)
    LSM_LANG_CLOJURE,
    LSM_LANG_FSHARP,
    LSM_LANG_JULIA,
    LSM_LANG_VIMSCRIPT,
    LSM_LANG_NIX,
    LSM_LANG_COMMONLISP,
    LSM_LANG_ELM,
    LSM_LANG_FORTRAN,
    LSM_LANG_CUDA,
    LSM_LANG_COBOL,
    LSM_LANG_VERILOG,
    LSM_LANG_EMACSLISP,
    LSM_LANG_JSON,
    LSM_LANG_XML,
    LSM_LANG_MARKDOWN,
    LSM_LANG_MAKEFILE,
    LSM_LANG_CMAKE,
    LSM_LANG_PROTOBUF,
    LSM_LANG_GRAPHQL,
    LSM_LANG_VUE,
    LSM_LANG_SVELTE,
    LSM_LANG_MESON,
    LSM_LANG_GLSL,
    LSM_LANG_INI,
    // Scientific/math languages
    LSM_LANG_MATLAB,
    LSM_LANG_LEAN,
    LSM_LANG_FORM,
    LSM_LANG_MAGMA,
    LSM_LANG_WOLFRAM,
    LSM_LANG_SOLIDITY,
    LSM_LANG_TYPST,
    LSM_LANG_GDSCRIPT,
    LSM_LANG_GLEAM,
    LSM_LANG_POWERSHELL,
    LSM_LANG_PASCAL,
    LSM_LANG_DLANG,
    LSM_LANG_NIM,
    LSM_LANG_SCHEME,
    LSM_LANG_FENNEL,
    LSM_LANG_FISH,
    LSM_LANG_AWK,
    LSM_LANG_ZSH,
    LSM_LANG_TCL,
    LSM_LANG_ADA,
    LSM_LANG_AGDA,
    LSM_LANG_RACKET,
    LSM_LANG_ODIN,
    LSM_LANG_RESCRIPT,
    LSM_LANG_PURESCRIPT,
    LSM_LANG_NICKEL,
    LSM_LANG_CRYSTAL,
    LSM_LANG_TEAL,
    LSM_LANG_HARE,
    LSM_LANG_PONY,
    LSM_LANG_LUAU,
    LSM_LANG_JANET,
    LSM_LANG_SWAY,
    LSM_LANG_NASM,
    LSM_LANG_ASSEMBLY,
    LSM_LANG_ASTRO,
    LSM_LANG_BLADE,
    LSM_LANG_JUST,
    LSM_LANG_GOTEMPLATE,
    LSM_LANG_TEMPL,
    LSM_LANG_LIQUID,
    LSM_LANG_JINJA2,
    LSM_LANG_PRISMA,
    LSM_LANG_HYPRLANG,
    LSM_LANG_DOTENV,
    LSM_LANG_DIFF,
    LSM_LANG_WGSL,
    LSM_LANG_KDL,
    LSM_LANG_JSON5,
    LSM_LANG_JSONNET,
    LSM_LANG_RON,
    LSM_LANG_THRIFT,
    LSM_LANG_CAPNP,
    LSM_LANG_PROPERTIES,
    LSM_LANG_SSHCONFIG,
    LSM_LANG_BIBTEX,
    LSM_LANG_STARLARK,
    LSM_LANG_BICEP,
    LSM_LANG_CSV,
    LSM_LANG_REQUIREMENTS,
    LSM_LANG_HLSL,
    LSM_LANG_VHDL,
    LSM_LANG_SYSTEMVERILOG,
    LSM_LANG_DEVICETREE,
    LSM_LANG_LINKERSCRIPT,
    LSM_LANG_GN,
    LSM_LANG_KCONFIG,
    LSM_LANG_BITBAKE,
    LSM_LANG_SMALI,
    LSM_LANG_TABLEGEN,
    LSM_LANG_ISPC,
    LSM_LANG_CAIRO,
    LSM_LANG_MOVE,
    LSM_LANG_SQUIRREL,
    LSM_LANG_FUNC,
    LSM_LANG_REGEX,
    LSM_LANG_JSDOC,
    LSM_LANG_RST,
    LSM_LANG_BEANCOUNT,
    LSM_LANG_MERMAID,
    LSM_LANG_PUPPET,
    LSM_LANG_PO,
    LSM_LANG_GITATTRIBUTES,
    LSM_LANG_GITIGNORE,
    LSM_LANG_SLANG,
    LSM_LANG_LLVM_IR,
    LSM_LANG_SMITHY,
    LSM_LANG_WIT,
    LSM_LANG_TLAPLUS,
    LSM_LANG_PKL,
    LSM_LANG_GOMOD,
    LSM_LANG_APEX,
    LSM_LANG_SOQL,
    LSM_LANG_SOSL,
    LSM_LANG_KUSTOMIZE,            // kustomization.yaml — Kubernetes overlay tool
    LSM_LANG_K8S,                  // Generic Kubernetes manifest (apiVersion: detected)
    LSM_LANG_PINE,                 // Pine Script (TradingView indicator / strategy language)
    LSM_LANG_QML,                  // Qt QML (Qt Modeling Language — declarative UI + embedded JS)
    LSM_LANG_CFSCRIPT,             // CFML script dialect (.cfc components — Lucee/ColdFusion)
    LSM_LANG_CFML,                 // CFML tag dialect (.cfm templates — Lucee/ColdFusion)
    LSM_LANG_MOJO,                 // Mojo
    LSM_LANG_OBJECTSCRIPT_UDL,     // InterSystems ObjectScript UDL (.cls class files)
    LSM_LANG_OBJECTSCRIPT_ROUTINE, // InterSystems ObjectScript routine (.mac/.int/.rtn/.inc)
    LSM_LANG_OBJECTSCRIPT_EXPORT,  // InterSystems Studio Export XML (<Export generator="Cache">)
    LSM_LANG_COUNT
} LSMLanguage;

// --- Extraction result structs ---

typedef struct {
    const char *name;           // short name
    const char *qualified_name; // project.path.name
    const char *label;          // "Function", "Method", "Class", "Variable", "Module"
    const char *file_path;      // relative path
    uint32_t start_line;
    uint32_t end_line;
    const char *signature;              // parameter text (NULL if none)
    const char *return_type;            // return type text (NULL if none)
    const char *receiver;               // Go method receiver (NULL if none)
    const char *docstring;              // leading doc comment (NULL if none)
    const char *parent_class;           // enclosing class QN for methods (NULL if none)
    const char **decorators;            // NULL-terminated array (NULL if none)
    const char **base_classes;          // NULL-terminated array (NULL if none)
    const char **param_names;           // NULL-terminated array (NULL if none)
    const char **param_types;           // NULL-terminated array (NULL if none)
    const char **signature_param_types; // ordered internal signature types; "?" means unknown
    int signature_param_count;          // number of entries in signature_param_types
    const char **return_types;          // NULL-terminated array (NULL if none)
    const char *route_path;   // HTTP route path from decorator (e.g., "/api/users") or NULL
    const char *route_method; // HTTP method from decorator (e.g., "POST") or NULL
    int complexity;           // cyclomatic complexity
    int cognitive;            // cognitive complexity (nesting-weighted)
    int loop_count;           // number of loop constructs in the body
    int loop_depth;           // max nested-loop depth (bottleneck proxy)
    bool is_recursive;        // body contains a direct self-call (seed for "recursive")
    int param_count;          // number of parameters (large = complexity smell)
    int max_access_depth;     // deepest chained member/subscript access (a.b.c.d)
    int linear_scan_in_loop;  // count of linear-scan calls (find/contains/indexOf) inside loops
    int alloc_in_loop;        // count of allocation/append calls inside loops
    bool recursion_in_loop;   // a self-call occurs inside a loop body
    bool unguarded_recursion; // recursive with no self-call guarded by a conditional
    int lines;                // body line count
    uint32_t *fingerprint;    // MinHash fingerprint (arena-allocated, K values) or NULL
    int fingerprint_k;        // number of hash values (LSM_MINHASH_K or 0)
    bool is_exported;
    bool is_abstract;
    bool is_test;
    bool is_entry_point;
    const char *structural_profile; // AST structural profile (arena-allocated) or NULL
    const char *body_tokens; // space-separated raw identifier tokens from body (arena) or NULL
    /* Rust only: raw trait path from the exact `impl Trait for Type` block
     * that declared this method.  Kept at the tail so zero-initialised
     * callers in every other language remain ABI/source compatible. */
    const char *impl_trait;
} LSMDefinition;

/* Argument captured from a call expression */
typedef struct {
    const char *expr;    // raw expression text ("payload.info", "MY_URL", "'hello'")
    const char *value;   // resolved string value or NULL (constant propagation)
    const char *keyword; // keyword name if keyword arg ("url", "topic_id"), NULL if positional
    int index;           // positional index (0-based)
} LSMCallArg;

#define LSM_MAX_CALL_ARGS 8

/* Byte offsets are meaningful only within the source buffer that produced
 * them. C/C++/CUDA run both raw and preprocessed extraction passes, and those
 * buffers can contain unrelated occurrences at the same numeric span. */
typedef enum {
    LSM_SOURCE_ORIGIN_RAW = 0,
    LSM_SOURCE_ORIGIN_PREPROCESSED,
} LSMSourceOrigin;

typedef struct {
    const char *callee_name;            // raw callee text ("pkg.Func", "foo")
    const char *enclosing_func_qn;      // QN of enclosing function (or module QN)
    const char *first_string_arg;       // first string literal argument (URL, topic, key) or NULL
    const char *second_arg_name;        // second argument identifier (handler ref) or NULL
    LSMCallArg args[LSM_MAX_CALL_ARGS]; // first N arguments with expressions
    int arg_count;                      // number of captured arguments
    int loop_depth;                     // enclosing loop nesting at the call site
    int branch_depth;                   // enclosing branch nesting at the call site
    int start_line;                     // 1-based source line of the call (for def range-match)
    uint32_t site_start_byte;           // exact AST occurrence span; end > start when present
    uint32_t site_end_byte;             // exclusive byte offset in the source file
    LSMSourceOrigin source_origin;      // raw source or C-family preprocessed buffer
    bool is_method;                     // method/member call with a non-self receiver. Perl:
                                        // arrow/method call ($obj->m). TS/JS/TSX: member call
                                        // x.foo() whose receiver is not this/super. Default false.
    bool requires_lsp_resolution;       // synthetic semantic candidate (for example an implicit
                                        // C++ operator). Never fall back to textual resolution.
} LSMCall;

typedef struct {
    const char *local_name;  // local alias or name
    const char *module_path; // resolved module path / QN
} LSMImport;

typedef enum {
    LSM_USAGE_VALUE = 0,
    LSM_USAGE_CALL_REFERENCE,
} LSMUsageKind;

typedef struct {
    const char *ref_name;            // referenced identifier
    const char *enclosing_func_qn;   // QN of enclosing function (or module QN)
    LSMUsageKind kind;               // ordinary USAGE or explicit callable reference
    bool may_be_call_reference;      // syntactic candidate; exact LSP proof may upgrade its edge
    bool semantic_reference_blocked; // lexical evidence blocks only unproven textual fallback
    bool semantic_reference_local_shadow; // blocker belongs to a non-module lexical scope
    uint32_t lexical_scope_id;            // extraction-local scope instance; never graph identity
    uint32_t site_start_byte;             // exact reference-token span; end > start when present
    uint32_t site_end_byte;               // exclusive byte offset in the source file
    LSMSourceOrigin source_origin;        // raw source or C-family preprocessed buffer
} LSMUsage;

typedef struct {
    const char *exception_name;    // exception class/type name
    const char *enclosing_func_qn; // QN of enclosing function
} LSMThrow;

typedef struct {
    const char *var_name;          // variable name
    const char *enclosing_func_qn; // QN of enclosing function
    bool is_write;                 // true = write, false = read
} LSMReadWrite;

typedef struct {
    const char *type_name;         // referenced type/class name
    const char *enclosing_func_qn; // QN of enclosing function
} LSMTypeRef;

typedef struct {
    const char *env_key;           // environment variable key
    const char *enclosing_func_qn; // QN of enclosing function
} LSMEnvAccess;

typedef struct {
    const char *var_name;          // variable being assigned
    const char *type_name;         // class/type name of RHS constructor
    const char *enclosing_func_qn; // QN of enclosing function
} LSMTypeAssign;

// String reference: URL, config key, or async target found in source.
// Extracted from string literals during AST walk.
typedef enum {
    LSM_STRREF_URL = 0,    // REST path or full URL
    LSM_STRREF_CONFIG = 1, // config file path or env var key
} LSMStringRefKind;

typedef struct {
    const char *value;             // the string literal content
    const char *enclosing_func_qn; // QN of enclosing function
    const char *key_path;          // dotted key path from YAML/JSON nesting (NULL if flat)
    LSMStringRefKind kind;         // URL, CONFIG
} LSMStringRef;

/* Infrastructure binding: topic/queue → endpoint URL.
 * Extracted from YAML/HCL/JSON subscription/scheduler configs.
 * Used by pass_route_nodes to connect async Route nodes to handler services. */
typedef struct {
    const char *source_name; // topic, queue, or schedule name
    const char *target_url;  // push_endpoint, uri, or http_target URL
    const char *broker;      // "pubsub", "cloud_tasks", "cloud_scheduler", "sqs", "kafka"
} LSMInfraBinding;

/* Pub/sub channel participation.  One record per emit() or on()/addListener()
 * call detected in source — the receiver (e.g. Socket.IO client, EventEmitter
 * instance) is intentionally NOT identified; matching is by channel_name
 * across files, which captures the common pattern of one logical bus per
 * service.  Transport disambiguates Socket.IO vs EventEmitter vs future
 * detectors (Kafka, Cloud Pub/Sub, etc.). */
typedef enum {
    LSM_CHANNEL_EMIT = 0,
    LSM_CHANNEL_LISTEN = 1,
} LSMChannelDirection;

typedef struct {
    const char *channel_name;      // literal channel name (e.g. "user.created")
    const char *transport;         // "socketio", "event_emitter", ...
    const char *enclosing_func_qn; // QN of the function containing the emit/on call
    LSMChannelDirection direction;
} LSMChannel;

// Rust: impl Trait for Struct
typedef struct {
    const char *trait_name;  // trait name (raw text)
    const char *struct_name; // struct/type name (raw text)
    /* Exact extracted QN of the implementing type.  Unlike struct_name this
     * does not need a later leaf-name guess, and the relation exists even for
     * an empty `impl Trait for Type {}` block. */
    const char *struct_qn;
} LSMImplTrait;

typedef enum {
    LSM_RESOLVED_INVOCATION = 0,
    LSM_RESOLVED_CALL_REFERENCE,
} LSMResolvedKind;

// LSP-resolved invocation/reference: high-confidence type-aware resolution.
typedef struct {
    const char *caller_qn;         // enclosing function QN
    const char *callee_qn;         // resolved target QN (fully qualified)
    const char *strategy;          // "lsp_type_dispatch", "lsp_direct", etc.
    float confidence;              // 0.90-0.95
    const char *reason;            // diagnostic label for unresolved calls (NULL if resolved)
    LSMResolvedKind kind;          // invocation (CALLS) or explicit callable reference
    uint32_t site_start_byte;      // exact source occurrence; end > start when present
    uint32_t site_end_byte;        // exclusive byte offset in the source file
    LSMSourceOrigin source_origin; // raw source or C-family preprocessed buffer
} LSMResolvedCall;

typedef struct {
    LSMResolvedCall *items;
    int count;
    int cap;
} LSMResolvedCallArray;

// Growable arrays used during extraction.
typedef struct {
    LSMDefinition *items;
    int count;
    int cap;
} LSMDefArray;

typedef struct {
    LSMCall *items;
    int count;
    int cap;
} LSMCallArray;

typedef struct {
    LSMImport *items;
    int count;
    int cap;
} LSMImportArray;

typedef struct {
    LSMUsage *items;
    int count;
    int cap;
} LSMUsageArray;

typedef struct {
    LSMThrow *items;
    int count;
    int cap;
} LSMThrowArray;

typedef struct {
    LSMReadWrite *items;
    int count;
    int cap;
} LSMRWArray;

typedef struct {
    LSMTypeRef *items;
    int count;
    int cap;
} LSMTypeRefArray;

typedef struct {
    LSMEnvAccess *items;
    int count;
    int cap;
} LSMEnvAccessArray;

typedef struct {
    LSMTypeAssign *items;
    int count;
    int cap;
} LSMTypeAssignArray;

typedef struct {
    LSMStringRef *items;
    int count;
    int cap;
} LSMStringRefArray;

typedef struct {
    LSMInfraBinding *items;
    int count;
    int cap;
} LSMInfraBindingArray;

typedef struct {
    LSMImplTrait *items;
    int count;
    int cap;
} LSMImplTraitArray;

typedef struct {
    LSMChannel *items;
    int count;
    int cap;
} LSMChannelArray;

// Full extraction result for one file.
typedef struct LSMFileResult {
    LSMArena arena; // owns local memory; composites may also retain child arenas below

    LSMDefArray defs;
    LSMCallArray calls;
    LSMImportArray imports;
    LSMUsageArray usages;
    LSMThrowArray throws;
    LSMRWArray rw;
    LSMTypeRefArray type_refs;
    LSMEnvAccessArray env_accesses;
    LSMTypeAssignArray type_assigns;
    LSMImplTraitArray impl_traits;       // Rust: impl Trait for Struct pairs
    LSMResolvedCallArray resolved_calls; // LSP-resolved invocations/references (high confidence)
    LSMStringRefArray string_refs;       // URL/config string literals from AST
    LSMInfraBindingArray infra_bindings; // topic→URL pairs from IaC configs
    LSMChannelArray channels;            // Socket.IO / EventEmitter pub/sub participation

    const char *module_qn;      // module qualified name
    const char *namespace_name; // declared namespace/package (Java/Kotlin/C#/PHP), NULL if none
    const char **exports;       // NULL-terminated (NULL if none)
    const char **constants;     // NULL-terminated (NULL if none)
    const char **global_vars;   // NULL-terminated (NULL if none)
    const char **macros;        // NULL-terminated, C/C++ only (NULL if none)

    bool has_error;
    const char *error_msg;
    /* Best-effort parse-coverage signal (experimental). parse_incomplete is true
     * when the parse tree contains tree-sitter ERROR/MISSING nodes — constructs
     * in those regions are silently absent from the graph. error_ranges is a
     * compact "start-end,start-end" list of 1-based line ranges (arena-owned) or
     * NULL. This only marks what we can DETECT: the absence of a flag is NOT a
     * completeness guarantee. Callers should treat a flagged file as "prefer
     * grep here", never treat an unflagged file as provably complete. */
    bool parse_incomplete;
    const char *error_ranges;
    int error_region_count;
    bool is_test_file;
    int imports_count;
    TSTree *cached_tree;     // retained parse tree (caller frees via lsm_free_tree)
    LSMLanguage cached_lang; // language of cached tree (for parser selection)

    // Retained source bytes — copied into `arena` by the parallel
    // extract pass so the fused cross-file LSP step in resolve_worker
    // can run without re-reading the file from disk. NULL when the
    // file exceeded the per-file (100 MB) or total (2 GB) retention
    // cap; in that case the cross-file LSP step is skipped for this
    // file (defs/calls already extracted are unaffected).
    const char *source;
    int source_len;

    // Composite extraction results (currently ObjectScript Studio Export)
    // retain their per-unit results so shallow-copied carrier strings remain
    // valid for the composite's full lifetime. Owned and recursively released
    // by lsm_free_result(); ordinary single-file results leave these zeroed.
    struct LSMFileResult **owned_results;
    int owned_result_count;
} LSMFileResult;

// --- Enclosing function cache ---
// Avoids repeated parent-chain walks for nodes within the same function body.
// Each entry records a function's byte range and its precomputed QN.
#define EFC_SIZE 64 // power of 2 for fast modulo

typedef struct {
    uint32_t start_byte;
    uint32_t end_byte;
    const char *qn;
} EFCEntry;

typedef struct {
    EFCEntry entries[EFC_SIZE];
    int count;
} EFCache;

// --- Extraction context passed to sub-extractors ---

// Module-level string constant map (for constant propagation)
#define LSM_MAX_STRING_CONSTANTS 256
typedef struct {
    const char *names[LSM_MAX_STRING_CONSTANTS];
    const char *values[LSM_MAX_STRING_CONSTANTS];
    int count;
} LSMStringConstantMap;

// Forward declaration: ObjectScript macro table (defined in macro_table.h).
typedef struct LSMMacroTable LSMMacroTable;

// Method-return-type table for ObjectScript variable type inference. Populated
// from definition nodes (method QN -> declared return type) so a later
// `Set x = obj.Method()` can resolve x's class.
#define LSM_RETURN_TYPE_TABLE_CAP 2048

typedef struct {
    const char *method_qn;
    const char *return_type;
} LSMReturnTypeEntry;

typedef struct {
    LSMReturnTypeEntry entries[LSM_RETURN_TYPE_TABLE_CAP];
    int count;
} LSMReturnTypeTable;

typedef struct {
    LSMArena *arena;
    LSMFileResult *result;
    const char *source;
    int source_len;
    LSMLanguage language;
    const char *project;
    const char *rel_path;
    const char *module_qn;
    TSNode root;
    EFCache ef_cache;                            // enclosing function cache
    const char *enclosing_class_qn;              // for nested class QN computation
    LSMStringConstantMap string_constants;       // module-level NAME = "value" pairs
    const LSMMacroTable *macro_table;            // ObjectScript $$$macro table (NULL if none)
    const LSMReturnTypeTable *return_type_table; // ObjectScript method return types (NULL if none)
    /* Set by extract_class_variables around its extract_var_names calls, so a
     * class-body variable def records which class declares it (parent_class)
     * without changing its module-level qualified name. NULL elsewhere. */
    const char *var_parent_class;
} LSMExtractCtx;

// --- Public API ---

// Bind third-party allocators (tree-sitter, sqlite3) to mimalloc as
// defense-in-depth, so they never depend on the fragile MI_OVERRIDE symbol
// override (#424). MUST be called as the very first statement of main(), before
// any sqlite3_open*/sqlite3_initialize (SQLITE_CONFIG_MALLOC returns
// SQLITE_MISUSE once sqlite has initialized).
// Idempotent (static guard); intended for single-threaded startup. lsm_init()
// also calls it so non-main entry points (pipeline passes) still get the binds.
// In the test build (no LSM_BIND_TS_ALLOCATOR) this is a no-op.
void lsm_alloc_init(void);

// Initialize the library. Call once at startup. Returns 0 on success.
int lsm_init(void);

// True when rel_path is in the crash-quarantine set — the newline-delimited list
// of files (LSM_INDEX_QUARANTINE_FILE) the crash supervisor pinned as crashers
// during its single-threaded recovery re-run. Loaded once, lazily; read-only
// after load. lsm_extract_file short-circuits such files to an empty result so no
// pass can crash on them; the pipeline extract loops call this to also REPORT the
// skip as phase="crash". Always false (cheap no-op) when the env var is unset.
bool lsm_index_is_quarantined(const char *rel_path);

// Phase a quarantined file was pinned under: "crash" (a fault signal) or "hang"
// (killed for making no progress). Returns NULL when rel_path is not quarantined.
// Drives the same lazy once-load as lsm_index_is_quarantined. Used by the pipeline
// extract loops to report the skip's phase in skipped[] (falls back to "crash").
const char *lsm_index_quarantine_phase(const char *rel_path);

// Crash-supervisor marker journal (parallel-safe): appends "S <rel_path>" /
// "D <rel_path>" to LSM_INDEX_MARKER_FILE. Files with an S but no D form the
// parent's crash/hang suspect set. No-ops when the env var is unset.
// lsm_extract_file journals its own start/done; long-running per-file phases
// (cross-LSP resolve) call these around their per-file work so a hang there
// is attributed to the RIGHT file instead of a stale extraction marker.
void lsm_index_mark_start(const char *rel_path);
void lsm_index_mark_done(const char *rel_path);

// Extract all data from one file. Caller must call lsm_free_result().
// source must remain valid for the duration of the call.
// timeout_micros: per-file parse timeout in microseconds (0 = no timeout).
LSMFileResult *lsm_extract_file(const char *source, int source_len, LSMLanguage language,
                                const char *project, const char *rel_path, int64_t timeout_micros,
                                const char **extra_defines, // NULL-terminated, or NULL
                                const char **include_paths  // NULL-terminated, or NULL
);

// Pipeline-internal variant of lsm_extract_file() carrying ObjectScript
// per-project tables (macro table + method-return-type table). The public
// lsm_extract_file() is a thin wrapper that passes NULL, NULL for both.
LSMFileResult *lsm_extract_file_ex(
    const char *source, int source_len, LSMLanguage language, const char *project,
    const char *rel_path, int64_t timeout_micros,
    const char **extra_defines,                 // NULL-terminated, or NULL
    const char **include_paths,                 // NULL-terminated, or NULL
    const LSMMacroTable *macro_table,           // ObjectScript macros, or NULL
    const LSMReturnTypeTable *return_type_table // OS return types, or NULL
);

// Free all memory associated with a result.
void lsm_free_result(LSMFileResult *result);

// Free only the cached tree from a result (caller retained it for reuse).
void lsm_free_tree(LSMFileResult *result);

// Free a standalone TSTree pointer (for Go layer cleanup).
void lsm_free_tree_ptr(TSTree *tree);

// Reset the thread-local parser's internal state, releasing slab-allocated
// subtrees. Must be called BEFORE lsm_slab_reset_thread() so the slab rebuild
// doesn't corrupt live parser state.
void lsm_reset_thread_parser(void);

// Destroy the thread-local parser. Call on worker thread exit.
void lsm_destroy_thread_parser(void);

// Shutdown the library. Call once at exit.
void lsm_shutdown(void);

// Profiling: get accumulated parse/extraction times and file count.
typedef struct {
    uint64_t *parse_ns;
    uint64_t *extract_ns;
    uint64_t *files;
} lsm_profile_out_t;
void lsm_get_profile(lsm_profile_out_t out);
uint64_t lsm_get_lsp_ns(void);
uint64_t lsm_get_preprocess_ns(void);
uint64_t lsm_get_files_preprocessed(void);
void lsm_reset_profile(void);

#if defined(LSM_KOTLIN_DEDUP_TEST_API) && LSM_KOTLIN_DEDUP_TEST_API
// Test-build-only operation counter for Kotlin operator-carrier deduplication.
// Production builds do not expose or retain this instrumentation.
void lsm_kotlin_operator_dedup_test_reset(void);
uint64_t lsm_kotlin_operator_dedup_test_comparisons(void);
#endif

#if defined(LSM_CALL_REFERENCE_LOOKUP_TEST_API) && LSM_CALL_REFERENCE_LOOKUP_TEST_API
// Test-build-only work counter for resolving a node's field role while
// classifying value references. Production builds retain no instrumentation.
void lsm_usage_field_lookup_test_reset(void);
uint64_t lsm_usage_field_lookup_test_work(void);
uint64_t lsm_usage_slow_parent_fallback_test_count(void);
#endif

// Toggle C/C++ preprocessor Macro-node extraction (#375). The pipeline enables
// it only for full/advanced index modes (it dominates extraction on macro-dense
// codebases). Default ON. Set before extraction; read-only during.
void lsm_set_macro_extraction(int enabled);
int lsm_macro_extraction_enabled(void);

// --- Internal helpers used by extractors ---

// Growable array push functions (arena-allocated, no individual free needed).
void lsm_defs_push(LSMDefArray *arr, LSMArena *a, LSMDefinition def);
void lsm_calls_push(LSMCallArray *arr, LSMArena *a, LSMCall call);
void lsm_imports_push(LSMImportArray *arr, LSMArena *a, LSMImport imp);
void lsm_usages_push(LSMUsageArray *arr, LSMArena *a, LSMUsage usage);
void lsm_throws_push(LSMThrowArray *arr, LSMArena *a, LSMThrow thr);
void lsm_rw_push(LSMRWArray *arr, LSMArena *a, LSMReadWrite rw);
void lsm_typerefs_push(LSMTypeRefArray *arr, LSMArena *a, LSMTypeRef tr);
void lsm_envaccess_push(LSMEnvAccessArray *arr, LSMArena *a, LSMEnvAccess ea);
void lsm_typeassign_push(LSMTypeAssignArray *arr, LSMArena *a, LSMTypeAssign ta);
void lsm_stringref_push(LSMStringRefArray *arr, LSMArena *a, LSMStringRef sr);
void lsm_infrabinding_push(LSMInfraBindingArray *arr, LSMArena *a, LSMInfraBinding ib);
void lsm_impltrait_push(LSMImplTraitArray *arr, LSMArena *a, LSMImplTrait it);
void lsm_resolvedcall_push(LSMResolvedCallArray *arr, LSMArena *a, LSMResolvedCall rc);
void lsm_channels_push(LSMChannelArray *arr, LSMArena *a, LSMChannel ch);

// --- Sub-extractor entry points ---

void lsm_extract_definitions(LSMExtractCtx *ctx);
void lsm_extract_imports(LSMExtractCtx *ctx);
void lsm_extract_usages(LSMExtractCtx *ctx);
void lsm_extract_semantic(LSMExtractCtx *ctx);
void lsm_extract_type_refs(LSMExtractCtx *ctx);
void lsm_extract_env_accesses(LSMExtractCtx *ctx);
void lsm_extract_type_assigns(LSMExtractCtx *ctx);
void lsm_extract_channels(LSMExtractCtx *ctx);

// Single-pass unified extraction (replaces the 7 calls above except defs+imports).
void lsm_extract_unified(LSMExtractCtx *ctx);

// K8s / Kustomize semantic extractor (called when language is LSM_LANG_K8S or LSM_LANG_KUSTOMIZE).
void lsm_extract_k8s(LSMExtractCtx *ctx);

// --- Label predicates ---

// True when `label` names a TYPE-LIKE container definition — a node that can own
// methods/fields, be a base/embedded type, satisfy/declare an interface, and be a
// target of name→type resolution. The canonical set is:
//   Class, Struct, Interface, Enum, Type, Trait.
// Single source of truth for every type-resolution / registry-seeding /
// INHERITS·IMPLEMENTS / LSP-type-registrar consumer, so adding a new type-like
// label (e.g. "Struct" for Rust/Go/Swift/D structs) updates them all at once
// instead of scattering `|| strcmp(label,"Struct")==0` across the tree.
// `label` may be NULL (returns false). Defined in helpers.c.
bool lsm_label_is_type_like(const char *label);

#endif // LSM_H
