#ifndef LSM_LSP_TYPE_REP_H
#define LSM_LSP_TYPE_REP_H

#include "../arena.h"
#include <stdbool.h>
#include <stdint.h>

// LSMTypeKind enumerates all type representations.
typedef enum {
    LSM_TYPE_UNKNOWN = 0,
    LSM_TYPE_NAMED,       // named type: "Database", "http.Request"
    LSM_TYPE_POINTER,     // *T
    LSM_TYPE_SLICE,       // []T
    LSM_TYPE_MAP,         // map[K]V
    LSM_TYPE_CHANNEL,     // chan T
    LSM_TYPE_FUNC,        // func(params) returns
    LSM_TYPE_INTERFACE,   // interface{...}
    LSM_TYPE_STRUCT,      // struct{...}
    LSM_TYPE_BUILTIN,     // int, string, bool, error, etc.
    LSM_TYPE_TUPLE,       // multi-return (T1, T2) / TS tuple [T,U]
    LSM_TYPE_TYPE_PARAM,  // generic type parameter: T, K, V
    LSM_TYPE_REFERENCE,   // T& (C++ lvalue reference)
    LSM_TYPE_RVALUE_REF,  // T&& (C++ rvalue reference)
    LSM_TYPE_TEMPLATE,    // Parameterized type: vector<T> — stores template name + args
    LSM_TYPE_ALIAS,       // Type alias: using/typedef — stores alias name + underlying type
    LSM_TYPE_UNION,       // Python: A | B; TS: A | B | C — sorted-canonical list (shared)
    LSM_TYPE_LITERAL,     // Python: Literal["foo", 3] — wraps a base type + literal value text
    LSM_TYPE_PROTOCOL,    // Python: typing.Protocol — like INTERFACE but matched structurally
    LSM_TYPE_MODULE,      // Python: import os; os is a module-typed binding
    LSM_TYPE_CALLABLE,    // Python: Callable[[A, B], R] — untyped-named callable variant of FUNC

    // --- TS-specific kinds (added in TS LSP integration) ---
    LSM_TYPE_INTERSECTION,  // TS: A & B — intersection type
    LSM_TYPE_TS_LITERAL,    // TS: "foo" / 42 / true literal types (tag+value layout, distinct
                            // from Python's LSM_TYPE_LITERAL which uses base+literal_text)
    LSM_TYPE_INDEXED,       // TS: T[K] — indexed access type
    LSM_TYPE_KEYOF,         // TS: keyof T
    LSM_TYPE_TYPEOF_QUERY,  // TS: typeof x in type position
    LSM_TYPE_CONDITIONAL,   // TS: T extends U ? X : Y
    LSM_TYPE_OBJECT_LIT,    // TS: { a: T1; b: T2 } anonymous object type
    LSM_TYPE_INFER,         // TS: `infer X` placeholder inside conditional
    LSM_TYPE_MAPPED,        // TS: {[K in keyof T]: ...} — v1 stub, members may be NULL
} LSMTypeKind;

// Forward declaration
typedef struct LSMType LSMType;

// Language-specific adapter used to parse one ordered signature type spelling.
typedef const LSMType *(*LSMTypeTextParser)(LSMArena *arena, const char *text, void *parser_ctx);

// LSMTypeParam represents a generic type parameter with optional constraint.
typedef struct {
    const char* name;        // "T", "K", "V"
    const LSMType* constraint; // interface constraint, or NULL for "any"
} LSMTypeParam;

// LSMType is a tagged union representing Go types.
struct LSMType {
    LSMTypeKind kind;
    union {
        struct { const char* qualified_name; } named;      // NAMED
        struct { const LSMType* elem; } pointer;            // POINTER
        struct { const LSMType* elem; } slice;              // SLICE
        struct { const LSMType* key; const LSMType* value; } map;  // MAP
        struct { const LSMType* elem; int direction; } channel;    // CHANNEL (0=bidi, 1=send, 2=recv)
        struct {
            const char** param_names;  // NULL-terminated
            const LSMType** param_types; // NULL-terminated
            const LSMType** return_types; // NULL-terminated
        } func;                                             // FUNC
        struct {
            const char** method_names;  // NULL-terminated
            const LSMType** method_sigs; // NULL-terminated (each is FUNC)
        } interface_type;                                   // INTERFACE
        struct {
            const char** field_names;   // NULL-terminated
            const LSMType** field_types; // NULL-terminated
        } struct_type;                                      // STRUCT
        struct { const char* name; } builtin;               // BUILTIN
        struct {
            const LSMType** elems;      // NULL-terminated
            int count;
        } tuple;                                            // TUPLE
        struct { const char* name; } type_param;            // TYPE_PARAM
        struct { const LSMType* elem; } reference;            // REFERENCE / RVALUE_REF
        struct {
            const char* template_name;      // "std::vector", "std::map"
            const LSMType** template_args;  // NULL-terminated
            int arg_count;
        } template_type;                                      // TEMPLATE
        struct {
            const char* alias_qn;          // "proj.ns.MyAlias"
            const LSMType* underlying;     // the actual type it aliases
        } alias;                                              // ALIAS
        struct {
            const LSMType** members;       // NULL-terminated, deduplicated, sorted by kind/qn
            int count;
        } union_type;                                         // UNION / INTERSECTION (shared)
        struct {
            const LSMType* base;           // base type (e.g. BUILTIN("int"), BUILTIN("str"))
            const char* literal_text;      // canonical text: "3", "\"foo\"", "True"
        } literal;                                            // LITERAL (Python)
        struct {
            const char* qualified_name;    // e.g. "typing.Iterable"
            const char** method_names;     // NULL-terminated method names — structural matching
            const LSMType** method_sigs;   // NULL-terminated signatures (each is FUNC/CALLABLE)
        } protocol;                                           // PROTOCOL
        struct {
            const char* module_qn;         // module qualified name (matches LSMImport.module_path)
        } module;                                             // MODULE
        struct {
            const LSMType** param_types;   // NULL-terminated; NULL element means "Any" / unknown
            const LSMType* return_type;    // single return; for tuples wrap in LSM_TYPE_TUPLE
            int param_count;               // -1 = elliptic / Callable[..., R]
        } callable;                                           // CALLABLE

        // --- TS-specific data ---
        struct {
            // Tag distinguishes string / number / boolean / bigint / null / undefined literals.
            // For boolean literals, value points to "true" or "false".
            const char* tag;               // "string" | "number" | "boolean" | "bigint" | "null" | "undefined"
            const char* value;             // textual representation; arena-owned
        } literal_ts;                                         // TS_LITERAL
        struct {
            const LSMType* object;         // T in T[K]
            const LSMType* index;          // K in T[K]
        } indexed;                                            // INDEXED
        struct { const LSMType* operand; } keyof;             // KEYOF
        struct { const char* expr; } typeof_query;            // TYPEOF_QUERY (referenced expression text)
        struct {
            const LSMType* check;          // T
            const LSMType* extends;        // U
            const LSMType* true_branch;    // X
            const LSMType* false_branch;   // Y
        } conditional;                                        // CONDITIONAL
        struct {
            const char** prop_names;       // NULL-terminated
            const LSMType** prop_types;    // NULL-terminated, parallel to prop_names
            const LSMType* call_signature; // FUNC type or NULL
            const LSMType* index_value;    // type produced by string/number index, or NULL
        } object_lit;                                         // OBJECT_LIT
        struct { const char* name; } infer;                   // INFER (e.g., `infer R`)
        struct {
            const char* key_name;          // "K" in {[K in keyof T]: V}
            const LSMType* key_constraint; // `keyof T`
            const LSMType* value;          // V (may reference key_name as TYPE_PARAM)
        } mapped;                                             // MAPPED (v1 stub-friendly)
    } data;
};

// Constructors (arena-allocated)
const LSMType* lsm_type_unknown(void);
const LSMType* lsm_type_named(LSMArena* a, const char* qualified_name);
const LSMType* lsm_type_pointer(LSMArena* a, const LSMType* elem);
const LSMType* lsm_type_slice(LSMArena* a, const LSMType* elem);
const LSMType* lsm_type_map(LSMArena* a, const LSMType* key, const LSMType* value);
const LSMType* lsm_type_channel(LSMArena* a, const LSMType* elem, int direction);
const LSMType* lsm_type_func(LSMArena* a, const char** param_names, const LSMType** param_types, const LSMType** return_types);
// Materialize exactly count positional parameter slots. NULL, empty, exact "?",
// and parser failures become UNKNOWN; the returned vector is NULL-terminated.
const LSMType **lsm_type_materialize_signature_params(LSMArena *a, const char *const *type_texts,
                                                      int count, LSMTypeTextParser parser,
                                                      void *parser_ctx);
// Rebuild a FUNC with new returns while preserving its parameter names/types.
const LSMType *lsm_type_func_replace_returns(LSMArena *a, const LSMType *old_signature,
                                             const LSMType *const *new_return_types);
const LSMType* lsm_type_builtin(LSMArena* a, const char* name);
const LSMType* lsm_type_tuple(LSMArena* a, const LSMType** elems, int count);
const LSMType* lsm_type_type_param(LSMArena* a, const char* name);
const LSMType* lsm_type_reference(LSMArena* a, const LSMType* elem);
const LSMType* lsm_type_rvalue_ref(LSMArena* a, const LSMType* elem);
const LSMType* lsm_type_template(LSMArena* a, const char* name, const LSMType** args, int arg_count);
const LSMType* lsm_type_alias(LSMArena* a, const char* alias_qn, const LSMType* underlying);

// Python-flavored constructors. UNION normalizes input: nested unions are
// flattened, duplicates removed, single-member unions collapse to that
// member, and the empty union is UNKNOWN. Members must be arena-allocated.
// Shared with TS LSP — both call this same constructor for `A | B`.
const LSMType* lsm_type_union(LSMArena* a, const LSMType** members, int count);
const LSMType* lsm_type_optional(LSMArena* a, const LSMType* t);  // Optional[T] == Union[T, None]
const LSMType* lsm_type_literal(LSMArena* a, const LSMType* base, const char* literal_text);
const LSMType* lsm_type_protocol(LSMArena* a, const char* qualified_name,
    const char** method_names, const LSMType** method_sigs);
const LSMType* lsm_type_module(LSMArena* a, const char* module_qn);
const LSMType* lsm_type_callable(LSMArena* a, const LSMType** param_types, int param_count,
    const LSMType* return_type);

// --- TS-specific constructors ---
const LSMType* lsm_type_intersection(LSMArena* a, const LSMType** members, int count);
// tag is one of "string"|"number"|"boolean"|"bigint"|"null"|"undefined".
// Distinct from lsm_type_literal (Python) which uses base+literal_text.
const LSMType* lsm_type_ts_literal(LSMArena* a, const char* tag, const char* value);
const LSMType* lsm_type_indexed(LSMArena* a, const LSMType* object, const LSMType* index);
const LSMType* lsm_type_keyof(LSMArena* a, const LSMType* operand);
const LSMType* lsm_type_typeof_query(LSMArena* a, const char* expr);
const LSMType* lsm_type_conditional(LSMArena* a,
    const LSMType* check, const LSMType* extends,
    const LSMType* true_branch, const LSMType* false_branch);
// prop_names and prop_types are NULL-terminated parallel arrays; either may be NULL for empty.
const LSMType* lsm_type_object_lit(LSMArena* a,
    const char** prop_names, const LSMType** prop_types,
    const LSMType* call_signature, const LSMType* index_value);
const LSMType* lsm_type_infer(LSMArena* a, const char* name);
const LSMType* lsm_type_mapped(LSMArena* a,
    const char* key_name, const LSMType* key_constraint, const LSMType* value);

// Operations
const LSMType* lsm_type_deref(const LSMType* t);         // remove one pointer level
const LSMType* lsm_type_elem(const LSMType* t);           // get element type (slice/chan/pointer)
bool lsm_type_is_unknown(const LSMType* t);
bool lsm_type_is_interface(const LSMType* t);
bool lsm_type_is_pointer(const LSMType* t);
bool lsm_type_is_reference(const LSMType* t);
bool lsm_type_is_union(const LSMType* t);
bool lsm_type_is_protocol(const LSMType* t);
bool lsm_type_is_module(const LSMType* t);

// Structural equality on type representation (used by union dedup and
// protocol-method-set matching). Two types are equal if their kinds match
// and their structural members match recursively.
bool lsm_type_equal(const LSMType* a, const LSMType* b);

// Test whether `candidate` satisfies the structural protocol `proto`.
// Walks proto.method_names against candidate's method set (NAMED → registry
// lookup is the caller's job; this helper only matches existing method
// signatures stored on a PROTOCOL).
bool lsm_type_protocol_satisfied_by(const LSMType* proto, const LSMType* candidate);

// Follow alias chain with cycle detection (max 16 levels).
const LSMType* lsm_type_resolve_alias(const LSMType* t);

// Generic type substitution: replace type params in t with concrete types.
// type_params: NULL-terminated array of param names
// type_args: corresponding concrete types
const LSMType* lsm_type_substitute(LSMArena* a, const LSMType* t,
    const char** type_params, const LSMType** type_args);

#endif // LSM_LSP_TYPE_REP_H
