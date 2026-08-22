#ifndef LSM_LSP_TYPE_REGISTRY_H
#define LSM_LSP_TYPE_REGISTRY_H

#include "type_rep.h"
#include "../arena.h"
#include <stdbool.h>

// Language-specific function metadata. Added at struct tail so existing
// callers that memset to zero before populating other fields keep working.
typedef enum {
    LSM_FUNC_FLAG_NONE = 0,
    LSM_FUNC_FLAG_PROPERTY = 1 << 0,        // @property -> obj.attr returns getter return
    LSM_FUNC_FLAG_CLASSMETHOD = 1 << 1,     // @classmethod -> first arg is cls (the class)
    LSM_FUNC_FLAG_STATICMETHOD = 1 << 2,    // @staticmethod -> no implicit self/cls
    LSM_FUNC_FLAG_ABSTRACTMETHOD = 1 << 3,  // @abstractmethod -> still callable for resolution
    LSM_FUNC_FLAG_OVERLOAD = 1 << 4,        // @overload entry — non-implementation stub
    LSM_FUNC_FLAG_ASYNC = 1 << 5,           // async def — return is Coroutine[..., T]
    LSM_FUNC_FLAG_GENERATOR = 1 << 6,       // contains yield — return is Generator[T, ...]
    LSM_FUNC_FLAG_FINAL = 1 << 7,           // @final — overrides not allowed
    LSM_FUNC_FLAG_RUST_TRAIT_IMPL = 1 << 8, // exact method from impl Trait for Type
    LSM_FUNC_FLAG_RUST_ABSTRACT = 1 << 9,   // required trait method without a default body
    /* Python only: more than one registered definition has this exact QN.
     * Ordinary call resolution keeps its historical language-specific choice,
     * but a function value cannot name one materialized definition exactly. */
    LSM_FUNC_FLAG_AMBIGUOUS_BINDING = 1 << 10,
} LSMFuncFlags;

// Registered function/method with full type signature.
typedef struct {
    const char *qualified_name;    // e.g., "proj.pkg.TypeName.MethodName"
    const char *receiver_type;     // e.g., "proj.pkg.TypeName" (NULL for functions)
    const char *short_name;        // e.g., "MethodName"
    const LSMType *signature;      // FUNC type with param/return types
    const char **type_param_names; // NULL-terminated, e.g., ["T", "R", NULL] for generics
    int min_params;                // Minimum required params (excluding defaulted). -1 = unknown.
    int flags;                     // LSM_FUNC_FLAG_* bitfield
    const char **decorator_qns;    // NULL-terminated decorator QNs (Python only); used for
                                   // user-decorator return-type substitution.
    /* Rust only: canonical trait QN for a concrete trait-impl method. It may
     * remain NULL when raw cross-file provenance is ambiguous; the Rust trait
     * flag still prevents that method from being mistaken for inherent. */
    const char *impl_trait_qn;
} LSMRegisteredFunc;

// Registered type with fields and method names.
typedef struct {
    const char *qualified_name;    // e.g., "proj.pkg.TypeName"
    const char *short_name;        // e.g., "TypeName"
    const char **field_names;      // NULL-terminated
    const LSMType **field_types;   // NULL-terminated (parallel to field_names)
    const char **method_names;     // NULL-terminated (short names)
    const char **method_qns;       // NULL-terminated (qualified names, parallel)
    const char **embedded_types;   // NULL-terminated (embedded/anonymous field type QNs)
    const char *alias_of;          // QN of aliased type (type Foo = Bar), NULL if not alias
    const char **type_param_names; // NULL-terminated, e.g., ["T", "K", NULL] for template classes
    bool is_interface;
    bool is_object; // Kotlin `object`/`companion object` singleton (member calls are static)

    // --- TS-specific fields (NULL/empty for non-TS types — backward compatible) ---
    // TS interfaces / object types may be callable: `interface F { (x:number): string }`.
    const LSMType *call_signature; // FUNC type or NULL
    // TS objects can have an index signature: `{ [key:string]: V }` or `{ [i:number]: V }`.
    const LSMType *index_key_type;   // BUILTIN("string"|"number") or NULL
    const LSMType *index_value_type; // V or NULL
    // Generic constraints, parallel to type_param_names. NULL or shorter array means "any".
    const LSMType **type_param_constraints; // NULL-terminated, parallel to type_param_names
} LSMRegisteredType;

// Hash-table bucket entry. Chains collisions via next-index list for overload sets.
typedef struct {
    uint64_t hash;     // FNV-1a of key
    int payload_index; // index into reg->funcs[] or reg->types[]
    int next_index;    // -1 = end of chain; else index of next bucket entry in same chain
    int slot;          // bucket slot this entry sits in (for resize)
} LSMRegistryHashEntry;

// Cross-file type/function registry.
typedef struct LSMTypeRegistry {
    LSMRegisteredFunc *funcs;
    int func_count;
    int func_cap;

    LSMRegisteredType *types;
    int type_count;
    int type_cap;

    LSMArena *arena; // owns all string data

    /* Optional fallback registry (Tier 2 two-level lookup). When a
     * lookup misses in this registry, it chains to `fallback`. Used by
     * TS/PHP cross-LSP: a small per-file registry (the file's own
     * AST-refined types) chains to a shared, immutable base registry
     * (stdlib + all project defs) built once. NULL = no chaining. */
    const struct LSMTypeRegistry *fallback;

    // Hash indexes (built lazily by lsm_registry_finalize, NULL until then).
    // Lookups fall back to linear scan when these are NULL.
    int *func_qn_buckets; // bucket → first entry index in func_qn_entries; -1 = empty
    LSMRegistryHashEntry *func_qn_entries; // entries indexed by linear order
    int func_qn_bucket_count;
    int func_qn_entry_count;

    int *type_qn_buckets;
    LSMRegistryHashEntry *type_qn_entries;
    int type_qn_bucket_count;
    int type_qn_entry_count;

    // Methods indexed by (receiver_type, short_name) — chain holds overloads.
    int *method_buckets;
    LSMRegistryHashEntry *method_entries;
    int method_bucket_count;
    int method_entry_count;

    // Auxiliary short-name / embedded-type indexes (built by finalize alongside the
    // QN buckets). Turn the Rust trait- and free-function fallback scans from
    // O(type_count)/O(func_count) into O(chain). Read-only after finalize.
    // Embedded-type index: fnv1a(bare last-'.'-segment of each embedded_type) -> chain
    // of TYPE indices declaring it. payload_index = type index (a type may appear once
    // per embedded entry; consumers dedup adjacent same-type via the iterator).
    int *type_embed_buckets;
    LSMRegistryHashEntry *type_embed_entries;
    int type_embed_bucket_count;
    int type_embed_entry_count;
    // Free-function short-name index: fnv1a(short_name) -> chain of FREE-function
    // (receiver_type==NULL) indices. payload_index = func index.
    int *type_short_buckets;
    LSMRegistryHashEntry *type_short_entries;
    int type_short_bucket_count;
    int type_short_entry_count;
    int *ffunc_short_buckets;
    LSMRegistryHashEntry *ffunc_short_entries;
    int ffunc_short_bucket_count;
    int ffunc_short_entry_count;

    /* Sealed / read-only. Set true by the lsm_X_build_cross_registry builders
     * (c/cpp, python, c#, ts, go) right after finalize: a Tier-2 cross-registry
     * is built ONCE and shared READ-ONLY across the parallel resolve workers.
     * lsm_registry_add_func/_type no-op on a sealed registry, so a per-file
     * resolver can never mutate the shared, finalized registry. Without this,
     * post-finalize adds accumulate in a tail the hash index does not cover ->
     * every lookup linear-scans it -> O(files*defs) (the Linux-kernel full-index
     * hang) plus a heap data race across workers. */
    bool read_only;
} LSMTypeRegistry;

// Initialize a registry.
void lsm_registry_init(LSMTypeRegistry *reg, LSMArena *arena);

// Build the hash indexes after all funcs/types have been added. Subsequent lookups
// use O(1) hashed dispatch instead of linear scans. Calling this is OPTIONAL — the
// linear-scan path remains correct. Single-file resolvers (small registries) skip
// finalize and stay linear; project-wide registries (many thousands of entries) call
// it once after pass-1.5 def-collection.
void lsm_registry_finalize(LSMTypeRegistry *reg);

// Like lsm_registry_finalize, but the hash-index allocations (buckets/entries)
// come from idx_arena instead of reg->arena. Per-file cross resolvers MUST use
// this with a scratch arena destroyed after the walk: their reg->arena is the
// pipeline-lifetime result arena, and per-file index allocations accumulated
// there add GBs across a large repo (FastAPI incremental test: +1.1 GB RSS).
void lsm_registry_finalize_into(LSMTypeRegistry *reg, LSMArena *idx_arena);

// Register a function/method.
void lsm_registry_add_func(LSMTypeRegistry *reg, LSMRegisteredFunc func);

// Register a type.
void lsm_registry_add_type(LSMTypeRegistry *reg, LSMRegisteredType type);

// Look up a method by receiver type QN + method name.
const LSMRegisteredFunc *lsm_registry_lookup_method(const LSMTypeRegistry *reg,
                                                    const char *receiver_qn,
                                                    const char *method_name);

// Look up a type by qualified name.
const LSMRegisteredType *lsm_registry_lookup_type(const LSMTypeRegistry *reg,
                                                  const char *qualified_name);

// Look up a function by qualified name.
const LSMRegisteredFunc *lsm_registry_lookup_func(const LSMTypeRegistry *reg,
                                                  const char *qualified_name);

// Look up a symbol (type or function) in a package by short name.
// package_qn is the package prefix (e.g., "proj.pkg").
const LSMRegisteredFunc *lsm_registry_lookup_symbol(const LSMTypeRegistry *reg,
                                                    const char *package_qn, const char *name);

// Resolve type alias chain: follow alias_of until concrete type found (max 16 levels).
const LSMRegisteredType *lsm_registry_resolve_alias(const LSMTypeRegistry *reg,
                                                    const char *type_qn);

// Look up a method by receiver type QN + method name, following alias chains.
const LSMRegisteredFunc *lsm_registry_lookup_method_aliased(const LSMTypeRegistry *reg,
                                                            const char *receiver_qn,
                                                            const char *method_name);

// Look up a method by receiver type + name, preferring the overload with matching arg count.
// Falls back to any match if no exact arg count match found.
const LSMRegisteredFunc *lsm_registry_lookup_method_by_args(const LSMTypeRegistry *reg,
                                                            const char *receiver_qn,
                                                            const char *method_name, int arg_count);

// Look up a free function by package + name, preferring matching arg count.
const LSMRegisteredFunc *lsm_registry_lookup_symbol_by_args(const LSMTypeRegistry *reg,
                                                            const char *package_qn,
                                                            const char *name, int arg_count);

// Look up a method by receiver type + name, scoring overloads by parameter type match.
// arg_types may contain NULL entries for unknown types. Falls back to arg-count matching.
const LSMRegisteredFunc *lsm_registry_lookup_method_by_types(const LSMTypeRegistry *reg,
                                                             const char *receiver_qn,
                                                             const char *method_name,
                                                             const LSMType **arg_types,
                                                             int arg_count);

// Look up a free function by package + name, scoring overloads by parameter type match.
const LSMRegisteredFunc *lsm_registry_lookup_symbol_by_types(const LSMTypeRegistry *reg,
                                                             const char *package_qn,
                                                             const char *name,
                                                             const LSMType **arg_types,
                                                             int arg_count);

// --- Auxiliary index iterators (Rust trait / free-function fallback fast paths) ---
//
// Iterate registry TYPE indices whose embedded_types contain an entry whose BARE
// name (last '.'-segment) equals `bare`. On a finalized registry this walks the
// embedded-type index plus any post-finalize tail; on an unfinalized registry it
// degrades to a full linear scan over all types (identical candidate set). Each
// matching type index is yielded at most once, in ascending registry order. The
// index is a bare-name PREFILTER — the caller MUST still apply its own exact
// predicate on each yielded type. Read-only, allocation-free. Usage:
//   LSMTypeEmbedIter it; lsm_registry_types_by_embedded_bare(reg, bare, &it);
//   int ti; while ((ti = lsm_type_embed_iter_next(&it)) >= 0) { ... reg->types[ti] ... }
typedef struct {
    const LSMTypeRegistry *reg;
    uint64_t hash;
    int chain_idx; // next entry in the embed chain, or -1
    int tail_i;    // next tail/linear type index
    int tail_end;  // reg->type_count snapshot
    int prev_type; // last yielded type index (adjacent-dedup); -1 = none
} LSMTypeEmbedIter;
void lsm_registry_types_by_embedded_bare(const LSMTypeRegistry *reg, const char *bare,
                                         LSMTypeEmbedIter *out);
int lsm_type_embed_iter_next(LSMTypeEmbedIter *it);

// Iterate FREE-function (receiver_type==NULL) indices whose short_name equals
// `short_name`. Same finalized/unfinalized behavior as above; caller re-checks its
// own predicate. Read-only, allocation-free.
typedef struct {
    const LSMTypeRegistry *reg;
    uint64_t hash;
    int chain_idx;
    int tail_i;
    int tail_end;
} LSMFreeFuncIter;
/* Iterate TYPE indices sharing a short name — the type-side twin of the free-
 * function iterator. Built by finalize into type_short_buckets; degrades to a
 * full types[] scan on an unfinalized registry (same correctness, old cost).
 * Added for cs_resolve_type_name's step-9 fallback, which scanned type_count
 * per unresolved name — quadratic against the shared Tier-2 registry. */
typedef struct {
    const LSMTypeRegistry *reg;
    uint64_t hash;
    int chain_idx;
    int tail_i;
    int tail_end;
} LSMTypeShortIter;
void lsm_registry_types_by_short_name(const LSMTypeRegistry *reg, const char *short_name,
                                      LSMTypeShortIter *out);
int lsm_type_short_iter_next(LSMTypeShortIter *it);

void lsm_registry_free_funcs_by_short_name(const LSMTypeRegistry *reg, const char *short_name,
                                           LSMFreeFuncIter *out);
int lsm_free_func_iter_next(LSMFreeFuncIter *it);

// Iterate function indices for one exact (receiver QN, method name) key.  This
// exposes the existing finalized method bucket without making Rust scan the
// project-wide func array merely to distinguish inherent and trait-impl
// entries that intentionally share the same source-level QN.  The caller may
// filter on language-specific flags. Read-only and allocation-free.
typedef struct {
    const LSMTypeRegistry *reg;
    const char *receiver_qn;
    const char *method_name;
    uint64_t hash;
    int chain_idx;
    int tail_i;
    int tail_end;
} LSMMethodIter;
void lsm_registry_methods(const LSMTypeRegistry *reg, const char *receiver_qn,
                          const char *method_name, LSMMethodIter *out);
int lsm_method_iter_next(LSMMethodIter *it);

// --- TS-specific helpers (return NULL for types without these signatures) ---

// If the type has a call signature (e.g., `interface F { (x:number): string }`), return
// a synthesised LSMRegisteredFunc whose qualified_name is "<type_qn>.__call" and
// short_name is "__call". Returns NULL if no call signature is present, the type is
// missing, or the receiver type was not registered. Caller must NOT free.
const LSMRegisteredFunc *lsm_registry_lookup_callable(const LSMTypeRegistry *reg, LSMArena *arena,
                                                      const char *type_qn);

// If the type has an index signature, return the value type produced by indexing with
// the given key type (string vs number). Returns NULL if no matching index signature.
const LSMType *lsm_registry_lookup_index_signature(const LSMTypeRegistry *reg, const char *type_qn,
                                                   const LSMType *key_type);

#endif // LSM_LSP_TYPE_REGISTRY_H
