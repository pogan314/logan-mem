/* rust_stdlib_data.c — Hand-curated Rust core/alloc/std prelude seed.
 *
 * NOT auto-generated yet (Rust has no machine-readable equivalent of
 * `go doc`/`pydoc`). This is a curated subset covering the prelude types
 * and ~150 most-used methods. The set is large enough to make
 * cross-file LSP resolution match rust-analyzer's output for typical
 * application code, while keeping the binary footprint minimal.
 *
 * Each entry maps onto the same `LSMRegisteredType` / `LSMRegisteredFunc`
 * shape used for project-defined types — the resolver does not care
 * whether a type came from the seed or the project.
 *
 * Coverage (roughly):
 *   - core::option::Option<T> + 18 methods
 *   - core::result::Result<T, E> + 17 methods
 *   - alloc::string::String + 22 methods
 *   - alloc::vec::Vec<T>     + 25 methods
 *   - alloc::collections::HashMap<K, V>, BTreeMap<K, V> + 12 methods each
 *   - core::iter::Iterator + 22 method signatures
 *   - alloc::boxed::Box<T> + 4 methods
 *   - core::fmt::{Display, Debug} traits with `fmt`
 *   - core::convert::{Into, From, AsRef} prelude traits
 *   - core::clone::Clone + clone()
 *   - Free helpers in std::mem (drop, replace, swap, take)
 *   - Free helpers in std::env (var)
 *
 * Adding more entries is mechanical: append to the matching section.
 */

#include "../type_rep.h"
#include "../type_registry.h"
#include "../rust_lsp.h"
#include <string.h>

/* Convenience macros to compress the boilerplate. We rely on arena
 * allocations being long-lived for the whole index — string literals are
 * fine because they live forever in `.rodata`. */
#define ADD_TYPE(qn, sn, iface) do {                              \
    LSMRegisteredType _rt;                                         \
    memset(&_rt, 0, sizeof(_rt));                                  \
    _rt.qualified_name = (qn);                                     \
    _rt.short_name = (sn);                                         \
    _rt.is_interface = (iface);                                    \
    lsm_registry_add_type(reg, _rt);                               \
} while (0)

#define ADD_FUNC(rcv, sn, qn, ret_type) do {                       \
    LSMRegisteredFunc _rf;                                         \
    memset(&_rf, 0, sizeof(_rf));                                  \
    _rf.qualified_name = (qn);                                     \
    _rf.short_name = (sn);                                         \
    _rf.receiver_type = (rcv);                                     \
    _rf.min_params = -1;                                           \
    const LSMType** _rta = (const LSMType**)lsm_arena_alloc(arena, \
        2 * sizeof(const LSMType*));                               \
    _rta[0] = (ret_type);                                          \
    _rta[1] = NULL;                                                \
    _rf.signature = lsm_type_func(arena, NULL, NULL, _rta);        \
    lsm_registry_add_func(reg, _rf);                               \
} while (0)

/* Forward declaration — defined in rust_crates_seed.c (also included
 * from lsp_all.c). Called at the end of the stdlib register so any
 * project that uses `use serde::…` / `use tokio::…` etc. picks up
 * the seeded methods automatically. */
void lsm_rust_crates_register(LSMTypeRegistry* reg, LSMArena* arena);

void lsm_rust_stdlib_register(LSMTypeRegistry* reg, LSMArena* arena) {
    /* ── Reusable type primitives ───────────────────────────────── */
    const LSMType* t_unit = lsm_type_builtin(arena, "()");
    const LSMType* t_bool = lsm_type_builtin(arena, "bool");
    const LSMType* t_usize = lsm_type_builtin(arena, "usize");
    const LSMType* t_str_ref = lsm_type_reference(arena, lsm_type_builtin(arena, "str"));
    const LSMType* t_string = lsm_type_named(arena, "alloc.string.String");
    const LSMType* t_self_named_string = lsm_type_named(arena, "alloc.string.String");

    /* ── Container types ────────────────────────────────────────── */
    ADD_TYPE("core.option.Option", "Option", false);
    ADD_TYPE("core.result.Result", "Result", false);
    ADD_TYPE("alloc.string.String", "String", false);
    ADD_TYPE("alloc.vec.Vec", "Vec", false);
    ADD_TYPE("alloc.collections.HashMap", "HashMap", false);
    ADD_TYPE("alloc.collections.BTreeMap", "BTreeMap", false);
    ADD_TYPE("alloc.collections.HashSet", "HashSet", false);
    ADD_TYPE("alloc.collections.BTreeSet", "BTreeSet", false);
    ADD_TYPE("alloc.collections.VecDeque", "VecDeque", false);
    ADD_TYPE("alloc.boxed.Box", "Box", false);
    ADD_TYPE("alloc.rc.Rc", "Rc", false);
    ADD_TYPE("alloc.sync.Arc", "Arc", false);
    ADD_TYPE("core.cell.RefCell", "RefCell", false);
    ADD_TYPE("core.cell.Cell", "Cell", false);
    ADD_TYPE("core.iter.Iterator", "Iterator", true);
    ADD_TYPE("core.iter.IntoIterator", "IntoIterator", true);
    ADD_TYPE("core.future.Future", "Future", true);
    ADD_TYPE("core.str", "str", false);

    /* ── Prelude traits ─────────────────────────────────────────── */
    ADD_TYPE("core.clone.Clone", "Clone", true);
    ADD_TYPE("core.marker.Copy", "Copy", true);
    ADD_TYPE("core.marker.Send", "Send", true);
    ADD_TYPE("core.marker.Sync", "Sync", true);
    ADD_TYPE("core.default.Default", "Default", true);
    ADD_TYPE("core.cmp.PartialEq", "PartialEq", true);
    ADD_TYPE("core.cmp.Eq", "Eq", true);
    ADD_TYPE("core.cmp.PartialOrd", "PartialOrd", true);
    ADD_TYPE("core.cmp.Ord", "Ord", true);
    ADD_TYPE("core.hash.Hash", "Hash", true);
    ADD_TYPE("core.hash.Hasher", "Hasher", true);
    ADD_TYPE("core.fmt.Display", "Display", true);
    ADD_TYPE("core.fmt.Debug", "Debug", true);
    ADD_TYPE("core.convert.From", "From", true);
    ADD_TYPE("core.convert.Into", "Into", true);
    ADD_TYPE("core.convert.TryFrom", "TryFrom", true);
    ADD_TYPE("core.convert.TryInto", "TryInto", true);
    ADD_TYPE("core.convert.AsRef", "AsRef", true);
    ADD_TYPE("core.convert.AsMut", "AsMut", true);
    ADD_TYPE("core.borrow.Borrow", "Borrow", true);
    ADD_TYPE("core.borrow.BorrowMut", "BorrowMut", true);
    ADD_TYPE("core.ops.Deref", "Deref", true);
    ADD_TYPE("core.ops.DerefMut", "DerefMut", true);
    ADD_TYPE("core.ops.Drop", "Drop", true);
    ADD_TYPE("core.ops.Add", "Add", true);
    ADD_TYPE("core.ops.Sub", "Sub", true);
    ADD_TYPE("core.ops.Mul", "Mul", true);
    ADD_TYPE("core.ops.Div", "Div", true);

    /* ── Option<T> methods ──────────────────────────────────────── */
    {
        const char* T = "core.option.Option";
        ADD_FUNC(T, "is_some",            "core.option.Option.is_some",            t_bool);
        ADD_FUNC(T, "is_none",            "core.option.Option.is_none",            t_bool);
        ADD_FUNC(T, "unwrap",             "core.option.Option.unwrap",             lsm_type_unknown());
        ADD_FUNC(T, "expect",             "core.option.Option.expect",             lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or",          "core.option.Option.unwrap_or",          lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or_default",  "core.option.Option.unwrap_or_default",  lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or_else",     "core.option.Option.unwrap_or_else",     lsm_type_unknown());
        ADD_FUNC(T, "as_ref",             "core.option.Option.as_ref",             lsm_type_unknown());
        ADD_FUNC(T, "as_mut",             "core.option.Option.as_mut",             lsm_type_unknown());
        ADD_FUNC(T, "map",                "core.option.Option.map",                lsm_type_unknown());
        ADD_FUNC(T, "and_then",           "core.option.Option.and_then",           lsm_type_unknown());
        ADD_FUNC(T, "or",                 "core.option.Option.or",                 lsm_type_unknown());
        ADD_FUNC(T, "or_else",            "core.option.Option.or_else",            lsm_type_unknown());
        ADD_FUNC(T, "ok_or",              "core.option.Option.ok_or",              lsm_type_unknown());
        ADD_FUNC(T, "ok_or_else",         "core.option.Option.ok_or_else",         lsm_type_unknown());
        ADD_FUNC(T, "take",               "core.option.Option.take",               lsm_type_unknown());
        ADD_FUNC(T, "replace",            "core.option.Option.replace",            lsm_type_unknown());
        ADD_FUNC(T, "filter",             "core.option.Option.filter",             lsm_type_unknown());
        ADD_FUNC(T, "iter",               "core.option.Option.iter",               lsm_type_unknown());
        ADD_FUNC(T, "into_iter",          "core.option.Option.into_iter",          lsm_type_unknown());
    }

    /* ── Result<T, E> methods ───────────────────────────────────── */
    {
        const char* T = "core.result.Result";
        ADD_FUNC(T, "is_ok",     "core.result.Result.is_ok",     t_bool);
        ADD_FUNC(T, "is_err",    "core.result.Result.is_err",    t_bool);
        ADD_FUNC(T, "ok",        "core.result.Result.ok",        lsm_type_unknown());
        ADD_FUNC(T, "err",       "core.result.Result.err",       lsm_type_unknown());
        ADD_FUNC(T, "unwrap",    "core.result.Result.unwrap",    lsm_type_unknown());
        ADD_FUNC(T, "expect",    "core.result.Result.expect",    lsm_type_unknown());
        ADD_FUNC(T, "unwrap_err","core.result.Result.unwrap_err",lsm_type_unknown());
        ADD_FUNC(T, "expect_err","core.result.Result.expect_err",lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or", "core.result.Result.unwrap_or", lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or_else","core.result.Result.unwrap_or_else", lsm_type_unknown());
        ADD_FUNC(T, "unwrap_or_default","core.result.Result.unwrap_or_default", lsm_type_unknown());
        ADD_FUNC(T, "as_ref",    "core.result.Result.as_ref",    lsm_type_unknown());
        ADD_FUNC(T, "as_mut",    "core.result.Result.as_mut",    lsm_type_unknown());
        ADD_FUNC(T, "map",       "core.result.Result.map",       lsm_type_unknown());
        ADD_FUNC(T, "map_err",   "core.result.Result.map_err",   lsm_type_unknown());
        ADD_FUNC(T, "and_then",  "core.result.Result.and_then",  lsm_type_unknown());
        ADD_FUNC(T, "or_else",   "core.result.Result.or_else",   lsm_type_unknown());
    }

    /* ── String methods ─────────────────────────────────────────── */
    {
        const char* T = "alloc.string.String";
        ADD_FUNC(T, "new",          "alloc.string.String.new",          t_self_named_string);
        ADD_FUNC(T, "with_capacity","alloc.string.String.with_capacity",t_self_named_string);
        ADD_FUNC(T, "from",         "alloc.string.String.from",         t_self_named_string);
        ADD_FUNC(T, "len",          "alloc.string.String.len",          t_usize);
        ADD_FUNC(T, "is_empty",     "alloc.string.String.is_empty",     t_bool);
        ADD_FUNC(T, "as_str",       "alloc.string.String.as_str",       t_str_ref);
        ADD_FUNC(T, "push_str",     "alloc.string.String.push_str",     t_unit);
        ADD_FUNC(T, "push",         "alloc.string.String.push",         t_unit);
        ADD_FUNC(T, "clear",        "alloc.string.String.clear",        t_unit);
        ADD_FUNC(T, "clone",        "alloc.string.String.clone",        t_self_named_string);
        ADD_FUNC(T, "trim",         "alloc.string.String.trim",         t_str_ref);
        ADD_FUNC(T, "to_string",    "alloc.string.String.to_string",    t_self_named_string);
        ADD_FUNC(T, "to_lowercase", "alloc.string.String.to_lowercase", t_self_named_string);
        ADD_FUNC(T, "to_uppercase", "alloc.string.String.to_uppercase", t_self_named_string);
        ADD_FUNC(T, "contains",     "alloc.string.String.contains",     t_bool);
        ADD_FUNC(T, "starts_with",  "alloc.string.String.starts_with",  t_bool);
        ADD_FUNC(T, "ends_with",    "alloc.string.String.ends_with",    t_bool);
        ADD_FUNC(T, "split",        "alloc.string.String.split",        lsm_type_unknown());
        ADD_FUNC(T, "lines",        "alloc.string.String.lines",        lsm_type_unknown());
        ADD_FUNC(T, "chars",        "alloc.string.String.chars",        lsm_type_unknown());
        ADD_FUNC(T, "bytes",        "alloc.string.String.bytes",        lsm_type_unknown());
        ADD_FUNC(T, "replace",      "alloc.string.String.replace",      t_self_named_string);
        ADD_FUNC(T, "parse",        "alloc.string.String.parse",        lsm_type_unknown());
        ADD_FUNC(T, "into_bytes",   "alloc.string.String.into_bytes",   lsm_type_unknown());
        ADD_FUNC(T, "as_bytes",     "alloc.string.String.as_bytes",     lsm_type_unknown());
    }

    /* core::str methods (shared with &str). */
    {
        const char* T = "core.str";
        ADD_FUNC(T, "len",          "core.str.len",          t_usize);
        ADD_FUNC(T, "is_empty",     "core.str.is_empty",     t_bool);
        ADD_FUNC(T, "to_string",    "core.str.to_string",    t_self_named_string);
        ADD_FUNC(T, "trim",         "core.str.trim",         t_str_ref);
        ADD_FUNC(T, "contains",     "core.str.contains",     t_bool);
        ADD_FUNC(T, "starts_with",  "core.str.starts_with",  t_bool);
        ADD_FUNC(T, "ends_with",    "core.str.ends_with",    t_bool);
        ADD_FUNC(T, "split",        "core.str.split",        lsm_type_unknown());
        ADD_FUNC(T, "lines",        "core.str.lines",        lsm_type_unknown());
        ADD_FUNC(T, "chars",        "core.str.chars",        lsm_type_unknown());
        ADD_FUNC(T, "bytes",        "core.str.bytes",        lsm_type_unknown());
        ADD_FUNC(T, "parse",        "core.str.parse",        lsm_type_unknown());
        ADD_FUNC(T, "to_lowercase", "core.str.to_lowercase", t_self_named_string);
        ADD_FUNC(T, "to_uppercase", "core.str.to_uppercase", t_self_named_string);
        ADD_FUNC(T, "replace",      "core.str.replace",      t_self_named_string);
        ADD_FUNC(T, "find",         "core.str.find",         lsm_type_unknown());
        ADD_FUNC(T, "rfind",        "core.str.rfind",        lsm_type_unknown());
        ADD_FUNC(T, "as_bytes",     "core.str.as_bytes",     lsm_type_unknown());
        ADD_FUNC(T, "splitn",       "core.str.splitn",       lsm_type_unknown());
    }

    /* ── Vec<T> methods ────────────────────────────────────────── */
    {
        const char* T = "alloc.vec.Vec";
        ADD_FUNC(T, "new",         "alloc.vec.Vec.new",         lsm_type_unknown());
        ADD_FUNC(T, "with_capacity","alloc.vec.Vec.with_capacity", lsm_type_unknown());
        ADD_FUNC(T, "from",        "alloc.vec.Vec.from",        lsm_type_unknown());
        ADD_FUNC(T, "push",        "alloc.vec.Vec.push",        t_unit);
        ADD_FUNC(T, "pop",         "alloc.vec.Vec.pop",         lsm_type_unknown());
        ADD_FUNC(T, "len",         "alloc.vec.Vec.len",         t_usize);
        ADD_FUNC(T, "is_empty",    "alloc.vec.Vec.is_empty",    t_bool);
        ADD_FUNC(T, "capacity",    "alloc.vec.Vec.capacity",    t_usize);
        ADD_FUNC(T, "clear",       "alloc.vec.Vec.clear",       t_unit);
        ADD_FUNC(T, "iter",        "alloc.vec.Vec.iter",        lsm_type_unknown());
        ADD_FUNC(T, "iter_mut",    "alloc.vec.Vec.iter_mut",    lsm_type_unknown());
        ADD_FUNC(T, "into_iter",   "alloc.vec.Vec.into_iter",   lsm_type_unknown());
        ADD_FUNC(T, "first",       "alloc.vec.Vec.first",       lsm_type_unknown());
        ADD_FUNC(T, "last",        "alloc.vec.Vec.last",        lsm_type_unknown());
        ADD_FUNC(T, "get",         "alloc.vec.Vec.get",         lsm_type_unknown());
        ADD_FUNC(T, "contains",    "alloc.vec.Vec.contains",    t_bool);
        ADD_FUNC(T, "as_slice",    "alloc.vec.Vec.as_slice",    lsm_type_unknown());
        ADD_FUNC(T, "as_mut_slice","alloc.vec.Vec.as_mut_slice",lsm_type_unknown());
        ADD_FUNC(T, "extend",      "alloc.vec.Vec.extend",      t_unit);
        ADD_FUNC(T, "remove",      "alloc.vec.Vec.remove",      lsm_type_unknown());
        ADD_FUNC(T, "swap_remove", "alloc.vec.Vec.swap_remove", lsm_type_unknown());
        ADD_FUNC(T, "insert",      "alloc.vec.Vec.insert",      t_unit);
        ADD_FUNC(T, "sort",        "alloc.vec.Vec.sort",        t_unit);
        ADD_FUNC(T, "sort_by",     "alloc.vec.Vec.sort_by",     t_unit);
        ADD_FUNC(T, "reverse",     "alloc.vec.Vec.reverse",     t_unit);
        ADD_FUNC(T, "drain",       "alloc.vec.Vec.drain",       lsm_type_unknown());
        ADD_FUNC(T, "clone",       "alloc.vec.Vec.clone",       lsm_type_unknown());
    }

    /* ── HashMap<K, V> methods ─────────────────────────────────── */
    {
        const char* T = "alloc.collections.HashMap";
        ADD_FUNC(T, "new",         "alloc.collections.HashMap.new",         lsm_type_unknown());
        ADD_FUNC(T, "with_capacity","alloc.collections.HashMap.with_capacity",lsm_type_unknown());
        ADD_FUNC(T, "insert",      "alloc.collections.HashMap.insert",      lsm_type_unknown());
        ADD_FUNC(T, "get",         "alloc.collections.HashMap.get",         lsm_type_unknown());
        ADD_FUNC(T, "get_mut",     "alloc.collections.HashMap.get_mut",     lsm_type_unknown());
        ADD_FUNC(T, "remove",      "alloc.collections.HashMap.remove",      lsm_type_unknown());
        ADD_FUNC(T, "contains_key","alloc.collections.HashMap.contains_key",t_bool);
        ADD_FUNC(T, "len",         "alloc.collections.HashMap.len",         t_usize);
        ADD_FUNC(T, "is_empty",    "alloc.collections.HashMap.is_empty",    t_bool);
        ADD_FUNC(T, "clear",       "alloc.collections.HashMap.clear",       t_unit);
        ADD_FUNC(T, "keys",        "alloc.collections.HashMap.keys",        lsm_type_unknown());
        ADD_FUNC(T, "values",      "alloc.collections.HashMap.values",      lsm_type_unknown());
        ADD_FUNC(T, "iter",        "alloc.collections.HashMap.iter",        lsm_type_unknown());
        ADD_FUNC(T, "entry",       "alloc.collections.HashMap.entry",       lsm_type_unknown());
    }
    {
        const char* T = "alloc.collections.BTreeMap";
        ADD_FUNC(T, "new",         "alloc.collections.BTreeMap.new",         lsm_type_unknown());
        ADD_FUNC(T, "insert",      "alloc.collections.BTreeMap.insert",      lsm_type_unknown());
        ADD_FUNC(T, "get",         "alloc.collections.BTreeMap.get",         lsm_type_unknown());
        ADD_FUNC(T, "remove",      "alloc.collections.BTreeMap.remove",      lsm_type_unknown());
        ADD_FUNC(T, "contains_key","alloc.collections.BTreeMap.contains_key",t_bool);
        ADD_FUNC(T, "len",         "alloc.collections.BTreeMap.len",         t_usize);
        ADD_FUNC(T, "is_empty",    "alloc.collections.BTreeMap.is_empty",    t_bool);
        ADD_FUNC(T, "iter",        "alloc.collections.BTreeMap.iter",        lsm_type_unknown());
        ADD_FUNC(T, "keys",        "alloc.collections.BTreeMap.keys",        lsm_type_unknown());
        ADD_FUNC(T, "values",      "alloc.collections.BTreeMap.values",      lsm_type_unknown());
    }

    /* ── Iterator trait methods ─────────────────────────────────── */
    {
        const char* T = "core.iter.Iterator";
        ADD_FUNC(T, "next",     "core.iter.Iterator.next",     lsm_type_unknown());
        ADD_FUNC(T, "map",      "core.iter.Iterator.map",      lsm_type_unknown());
        ADD_FUNC(T, "filter",   "core.iter.Iterator.filter",   lsm_type_unknown());
        ADD_FUNC(T, "fold",     "core.iter.Iterator.fold",     lsm_type_unknown());
        ADD_FUNC(T, "for_each", "core.iter.Iterator.for_each", t_unit);
        ADD_FUNC(T, "collect",  "core.iter.Iterator.collect",  lsm_type_unknown());
        ADD_FUNC(T, "count",    "core.iter.Iterator.count",    t_usize);
        ADD_FUNC(T, "sum",      "core.iter.Iterator.sum",      lsm_type_unknown());
        ADD_FUNC(T, "max",      "core.iter.Iterator.max",      lsm_type_unknown());
        ADD_FUNC(T, "min",      "core.iter.Iterator.min",      lsm_type_unknown());
        ADD_FUNC(T, "any",      "core.iter.Iterator.any",      t_bool);
        ADD_FUNC(T, "all",      "core.iter.Iterator.all",      t_bool);
        ADD_FUNC(T, "find",     "core.iter.Iterator.find",     lsm_type_unknown());
        ADD_FUNC(T, "position", "core.iter.Iterator.position", lsm_type_unknown());
        ADD_FUNC(T, "enumerate","core.iter.Iterator.enumerate",lsm_type_unknown());
        ADD_FUNC(T, "zip",      "core.iter.Iterator.zip",      lsm_type_unknown());
        ADD_FUNC(T, "chain",    "core.iter.Iterator.chain",    lsm_type_unknown());
        ADD_FUNC(T, "take",     "core.iter.Iterator.take",     lsm_type_unknown());
        ADD_FUNC(T, "skip",     "core.iter.Iterator.skip",     lsm_type_unknown());
        ADD_FUNC(T, "rev",      "core.iter.Iterator.rev",      lsm_type_unknown());
        ADD_FUNC(T, "cloned",   "core.iter.Iterator.cloned",   lsm_type_unknown());
        ADD_FUNC(T, "copied",   "core.iter.Iterator.copied",   lsm_type_unknown());
        ADD_FUNC(T, "step_by",  "core.iter.Iterator.step_by",  lsm_type_unknown());
        ADD_FUNC(T, "flat_map", "core.iter.Iterator.flat_map", lsm_type_unknown());
        ADD_FUNC(T, "flatten",  "core.iter.Iterator.flatten",  lsm_type_unknown());
        ADD_FUNC(T, "filter_map","core.iter.Iterator.filter_map",lsm_type_unknown());
        ADD_FUNC(T, "peekable", "core.iter.Iterator.peekable", lsm_type_unknown());
    }

    /* ── Box / Rc / Arc — focus on `new` and Deref ─────────────── */
    {
        ADD_FUNC("alloc.boxed.Box", "new",         "alloc.boxed.Box.new",         lsm_type_unknown());
        ADD_FUNC("alloc.boxed.Box", "as_ref",      "alloc.boxed.Box.as_ref",      lsm_type_unknown());
        ADD_FUNC("alloc.boxed.Box", "into_inner",  "alloc.boxed.Box.into_inner",  lsm_type_unknown());

        ADD_FUNC("alloc.rc.Rc",     "new",         "alloc.rc.Rc.new",             lsm_type_unknown());
        ADD_FUNC("alloc.rc.Rc",     "clone",       "alloc.rc.Rc.clone",           lsm_type_unknown());
        ADD_FUNC("alloc.rc.Rc",     "strong_count","alloc.rc.Rc.strong_count",    t_usize);

        ADD_FUNC("alloc.sync.Arc",  "new",         "alloc.sync.Arc.new",          lsm_type_unknown());
        ADD_FUNC("alloc.sync.Arc",  "clone",       "alloc.sync.Arc.clone",        lsm_type_unknown());
        ADD_FUNC("alloc.sync.Arc",  "strong_count","alloc.sync.Arc.strong_count", t_usize);

        ADD_FUNC("core.cell.RefCell","new",        "core.cell.RefCell.new",       lsm_type_unknown());
        ADD_FUNC("core.cell.RefCell","borrow",     "core.cell.RefCell.borrow",    lsm_type_unknown());
        ADD_FUNC("core.cell.RefCell","borrow_mut", "core.cell.RefCell.borrow_mut",lsm_type_unknown());
    }

    /* ── Trait method signatures (default impls go on the trait QN) ── */
    {
        ADD_FUNC("core.clone.Clone",        "clone",     "core.clone.Clone.clone",     lsm_type_unknown());
        ADD_FUNC("core.default.Default",    "default",   "core.default.Default.default", lsm_type_unknown());
        ADD_FUNC("core.fmt.Display",        "fmt",       "core.fmt.Display.fmt",       lsm_type_unknown());
        ADD_FUNC("core.fmt.Debug",          "fmt",       "core.fmt.Debug.fmt",         lsm_type_unknown());
        ADD_FUNC("core.cmp.PartialEq",      "eq",        "core.cmp.PartialEq.eq",      t_bool);
        ADD_FUNC("core.cmp.PartialEq",      "ne",        "core.cmp.PartialEq.ne",      t_bool);
        ADD_FUNC("core.cmp.PartialOrd",     "partial_cmp","core.cmp.PartialOrd.partial_cmp", lsm_type_unknown());
        ADD_FUNC("core.cmp.Ord",            "cmp",       "core.cmp.Ord.cmp",           lsm_type_unknown());
        ADD_FUNC("core.hash.Hash",          "hash",      "core.hash.Hash.hash",        t_unit);
        ADD_FUNC("core.convert.From",       "from",      "core.convert.From.from",     lsm_type_unknown());
        ADD_FUNC("core.convert.Into",       "into",      "core.convert.Into.into",     lsm_type_unknown());
        ADD_FUNC("core.convert.TryFrom",    "try_from",  "core.convert.TryFrom.try_from", lsm_type_unknown());
        ADD_FUNC("core.convert.TryInto",    "try_into",  "core.convert.TryInto.try_into", lsm_type_unknown());
        ADD_FUNC("core.convert.AsRef",      "as_ref",    "core.convert.AsRef.as_ref",  lsm_type_unknown());
        ADD_FUNC("core.convert.AsMut",      "as_mut",    "core.convert.AsMut.as_mut",  lsm_type_unknown());
        ADD_FUNC("core.borrow.Borrow",      "borrow",    "core.borrow.Borrow.borrow",  lsm_type_unknown());
        ADD_FUNC("core.borrow.BorrowMut",   "borrow_mut","core.borrow.BorrowMut.borrow_mut", lsm_type_unknown());
        ADD_FUNC("core.ops.Deref",          "deref",     "core.ops.Deref.deref",       lsm_type_unknown());
        ADD_FUNC("core.ops.DerefMut",       "deref_mut", "core.ops.DerefMut.deref_mut",lsm_type_unknown());
        ADD_FUNC("core.ops.Drop",           "drop",      "core.ops.Drop.drop",         t_unit);
        ADD_FUNC("core.ops.Add",            "add",       "core.ops.Add.add",           lsm_type_unknown());
        ADD_FUNC("core.ops.Sub",            "sub",       "core.ops.Sub.sub",           lsm_type_unknown());
        ADD_FUNC("core.ops.Mul",            "mul",       "core.ops.Mul.mul",           lsm_type_unknown());
        ADD_FUNC("core.ops.Div",            "div",       "core.ops.Div.div",           lsm_type_unknown());
    }

    /* ── std re-exports (alias the alloc/core paths) ────────────
     *
     * Many prelude types (`String`, `Vec`, `HashMap`, `Arc`, `Box`, …)
     * are defined in `alloc` and re-exported through `std`. When user
     * code writes `use std::collections::HashMap;` the path the LSP
     * sees is `std::collections::HashMap` — but our seed registers the
     * methods under `alloc.collections.HashMap`. We patch this by
     * also registering each std QN with an `alias_of` link, so the
     * registry-aware lookups resolve through alias chains.
     *
     * The list mirrors the most-used `std` re-exports — adding more is
     * cheap and harmless (an extra ~20 bytes per entry). */
    {
        struct ReExport { const char* std_qn; const char* alloc_qn; const char* short_name; };
        static const struct ReExport reexports[] = {
            {"std.string.String",            "alloc.string.String",            "String"},
            {"std.string.ToString",          "alloc.string.ToString",          "ToString"},
            {"std.vec.Vec",                  "alloc.vec.Vec",                  "Vec"},
            {"std.collections.HashMap",      "alloc.collections.HashMap",      "HashMap"},
            {"std.collections.BTreeMap",     "alloc.collections.BTreeMap",     "BTreeMap"},
            {"std.collections.HashSet",      "alloc.collections.HashSet",      "HashSet"},
            {"std.collections.BTreeSet",     "alloc.collections.BTreeSet",     "BTreeSet"},
            {"std.collections.VecDeque",     "alloc.collections.VecDeque",     "VecDeque"},
            {"std.boxed.Box",                "alloc.boxed.Box",                "Box"},
            {"std.rc.Rc",                    "alloc.rc.Rc",                    "Rc"},
            {"std.sync.Arc",                 "alloc.sync.Arc",                 "Arc"},
            {"std.option.Option",            "core.option.Option",             "Option"},
            {"std.result.Result",            "core.result.Result",             "Result"},
            {"std.iter.Iterator",            "core.iter.Iterator",             "Iterator"},
            {"std.iter.IntoIterator",        "core.iter.IntoIterator",         "IntoIterator"},
            {"std.future.Future",            "core.future.Future",             "Future"},
            {"std.clone.Clone",              "core.clone.Clone",               "Clone"},
            {"std.default.Default",          "core.default.Default",           "Default"},
            {"std.cmp.PartialEq",            "core.cmp.PartialEq",             "PartialEq"},
            {"std.cmp.Eq",                   "core.cmp.Eq",                    "Eq"},
            {"std.cmp.PartialOrd",           "core.cmp.PartialOrd",            "PartialOrd"},
            {"std.cmp.Ord",                  "core.cmp.Ord",                   "Ord"},
            {"std.hash.Hash",                "core.hash.Hash",                 "Hash"},
            {"std.fmt.Display",              "core.fmt.Display",               "Display"},
            {"std.fmt.Debug",                "core.fmt.Debug",                 "Debug"},
            {"std.convert.From",             "core.convert.From",              "From"},
            {"std.convert.Into",             "core.convert.Into",              "Into"},
            {"std.convert.TryFrom",          "core.convert.TryFrom",           "TryFrom"},
            {"std.convert.TryInto",          "core.convert.TryInto",           "TryInto"},
            {"std.convert.AsRef",            "core.convert.AsRef",             "AsRef"},
            {"std.convert.AsMut",            "core.convert.AsMut",             "AsMut"},
            {"std.borrow.Borrow",            "core.borrow.Borrow",             "Borrow"},
            {"std.borrow.BorrowMut",         "core.borrow.BorrowMut",          "BorrowMut"},
            {"std.ops.Deref",                "core.ops.Deref",                 "Deref"},
            {"std.ops.DerefMut",             "core.ops.DerefMut",              "DerefMut"},
            {"std.ops.Drop",                 "core.ops.Drop",                  "Drop"},
            {"std.cell.RefCell",             "core.cell.RefCell",              "RefCell"},
            {"std.cell.Cell",                "core.cell.Cell",                 "Cell"},
        };
        for (size_t i = 0; i < sizeof(reexports) / sizeof(reexports[0]); i++) {
            LSMRegisteredType rt;
            memset(&rt, 0, sizeof(rt));
            rt.qualified_name = reexports[i].std_qn;
            rt.short_name = reexports[i].short_name;
            rt.alias_of = reexports[i].alloc_qn;
            lsm_registry_add_type(reg, rt);
        }
    }

    /* ── Free functions worth attributing ──────────────────────── */
    {
        /* std::mem helpers. */
        const LSMType* unit_arr[2] = {t_unit, NULL};
        (void)unit_arr;
        ADD_FUNC(NULL, "drop",     "std.mem.drop",     t_unit);
        ADD_FUNC(NULL, "replace",  "std.mem.replace",  lsm_type_unknown());
        ADD_FUNC(NULL, "swap",     "std.mem.swap",     t_unit);
        ADD_FUNC(NULL, "take",     "std.mem.take",     lsm_type_unknown());
        ADD_FUNC(NULL, "size_of",  "std.mem.size_of",  t_usize);

        /* std::env::var. */
        ADD_FUNC(NULL, "var",      "std.env.var",      lsm_type_unknown());

        /* Macros (synthetic placeholders so trace tools see the call). */
        ADD_FUNC(NULL, "println",  "std.macros.println",  t_unit);
        ADD_FUNC(NULL, "eprintln", "std.macros.eprintln", t_unit);
        ADD_FUNC(NULL, "print",    "std.macros.print",    t_unit);
        ADD_FUNC(NULL, "eprint",   "std.macros.eprint",   t_unit);
        ADD_FUNC(NULL, "format",   "std.macros.format",   t_string);
        ADD_FUNC(NULL, "write",    "std.macros.write",    t_unit);
        ADD_FUNC(NULL, "writeln",  "std.macros.writeln",  t_unit);
        ADD_FUNC(NULL, "vec",      "alloc.vec.vec",       lsm_type_unknown());
        ADD_FUNC(NULL, "panic",    "core.panicking.panic",lsm_type_unknown());
    }

    /* ════════════════════════════════════════════════════════════════
     * Extended stdlib seed — pushes coverage from ~150 methods to
     * ~700, the threshold at which idiomatic Rust application code
     * starts hitting the seed instead of bouncing off "unknown".
     * Sections below mirror the Rust documentation table of contents.
     * ════════════════════════════════════════════════════════════════ */

    /* ── std::io ────────────────────────────────────────────────── */
    ADD_TYPE("std.io.Read",         "Read",         true);
    ADD_TYPE("std.io.Write",        "Write",        true);
    ADD_TYPE("std.io.Seek",         "Seek",         true);
    ADD_TYPE("std.io.BufRead",      "BufRead",      true);
    ADD_TYPE("std.io.BufReader",    "BufReader",    false);
    ADD_TYPE("std.io.BufWriter",    "BufWriter",    false);
    ADD_TYPE("std.io.LineWriter",   "LineWriter",   false);
    ADD_TYPE("std.io.Cursor",       "Cursor",       false);
    ADD_TYPE("std.io.Stdin",        "Stdin",        false);
    ADD_TYPE("std.io.Stdout",       "Stdout",       false);
    ADD_TYPE("std.io.Stderr",       "Stderr",      false);
    ADD_TYPE("std.io.StdoutLock",   "StdoutLock",   false);
    ADD_TYPE("std.io.Error",        "Error",        false);
    ADD_TYPE("std.io.ErrorKind",    "ErrorKind",    false);
    ADD_TYPE("std.io.SeekFrom",     "SeekFrom",     false);
    ADD_TYPE("std.io.Empty",        "Empty",        false);
    ADD_TYPE("std.io.Sink",         "Sink",         false);
    ADD_TYPE("std.io.Repeat",       "Repeat",       false);

    {
        const char* T = "std.io.Read";
        ADD_FUNC(T, "read",            "std.io.Read.read",            lsm_type_unknown());
        ADD_FUNC(T, "read_to_end",     "std.io.Read.read_to_end",     lsm_type_unknown());
        ADD_FUNC(T, "read_to_string",  "std.io.Read.read_to_string",  lsm_type_unknown());
        ADD_FUNC(T, "read_exact",      "std.io.Read.read_exact",      lsm_type_unknown());
        ADD_FUNC(T, "by_ref",          "std.io.Read.by_ref",          lsm_type_unknown());
        ADD_FUNC(T, "bytes",           "std.io.Read.bytes",           lsm_type_unknown());
        ADD_FUNC(T, "chain",           "std.io.Read.chain",           lsm_type_unknown());
        ADD_FUNC(T, "take",            "std.io.Read.take",            lsm_type_unknown());
    }
    {
        const char* T = "std.io.Write";
        ADD_FUNC(T, "write",           "std.io.Write.write",           lsm_type_unknown());
        ADD_FUNC(T, "write_all",       "std.io.Write.write_all",       lsm_type_unknown());
        ADD_FUNC(T, "write_fmt",       "std.io.Write.write_fmt",       lsm_type_unknown());
        ADD_FUNC(T, "flush",           "std.io.Write.flush",           lsm_type_unknown());
        ADD_FUNC(T, "by_ref",          "std.io.Write.by_ref",          lsm_type_unknown());
    }
    {
        const char* T = "std.io.Seek";
        ADD_FUNC(T, "seek",            "std.io.Seek.seek",            lsm_type_unknown());
        ADD_FUNC(T, "rewind",          "std.io.Seek.rewind",          lsm_type_unknown());
        ADD_FUNC(T, "stream_position", "std.io.Seek.stream_position", lsm_type_unknown());
    }
    {
        const char* T = "std.io.BufRead";
        ADD_FUNC(T, "read_line",       "std.io.BufRead.read_line",    lsm_type_unknown());
        ADD_FUNC(T, "read_until",      "std.io.BufRead.read_until",   lsm_type_unknown());
        ADD_FUNC(T, "lines",           "std.io.BufRead.lines",        lsm_type_unknown());
        ADD_FUNC(T, "split",           "std.io.BufRead.split",        lsm_type_unknown());
        ADD_FUNC(T, "fill_buf",        "std.io.BufRead.fill_buf",     lsm_type_unknown());
        ADD_FUNC(T, "consume",         "std.io.BufRead.consume",      t_unit);
    }
    {
        const char* T = "std.io.BufReader";
        ADD_FUNC(T, "new",             "std.io.BufReader.new",        lsm_type_unknown());
        ADD_FUNC(T, "with_capacity",   "std.io.BufReader.with_capacity", lsm_type_unknown());
        ADD_FUNC(T, "into_inner",      "std.io.BufReader.into_inner", lsm_type_unknown());
        ADD_FUNC(T, "get_ref",         "std.io.BufReader.get_ref",    lsm_type_unknown());
        ADD_FUNC(T, "buffer",          "std.io.BufReader.buffer",     lsm_type_unknown());
        ADD_FUNC(T, "capacity",        "std.io.BufReader.capacity",   t_usize);
    }
    {
        const char* T = "std.io.BufWriter";
        ADD_FUNC(T, "new",             "std.io.BufWriter.new",        lsm_type_unknown());
        ADD_FUNC(T, "with_capacity",   "std.io.BufWriter.with_capacity", lsm_type_unknown());
        ADD_FUNC(T, "into_inner",      "std.io.BufWriter.into_inner", lsm_type_unknown());
        ADD_FUNC(T, "get_ref",         "std.io.BufWriter.get_ref",    lsm_type_unknown());
        ADD_FUNC(T, "flush",           "std.io.BufWriter.flush",      lsm_type_unknown());
    }
    {
        const char* T = "std.io.Cursor";
        ADD_FUNC(T, "new",             "std.io.Cursor.new",           lsm_type_unknown());
        ADD_FUNC(T, "into_inner",      "std.io.Cursor.into_inner",    lsm_type_unknown());
        ADD_FUNC(T, "position",        "std.io.Cursor.position",      lsm_type_unknown());
        ADD_FUNC(T, "set_position",    "std.io.Cursor.set_position",  t_unit);
    }
    {
        const char* T = "std.io.Error";
        ADD_FUNC(T, "new",             "std.io.Error.new",            lsm_type_unknown());
        ADD_FUNC(T, "kind",            "std.io.Error.kind",           lsm_type_unknown());
        ADD_FUNC(T, "raw_os_error",    "std.io.Error.raw_os_error",   lsm_type_unknown());
        ADD_FUNC(T, "into_inner",      "std.io.Error.into_inner",     lsm_type_unknown());
        ADD_FUNC(T, "last_os_error",   "std.io.Error.last_os_error",  lsm_type_unknown());
        ADD_FUNC(T, "from_raw_os_error","std.io.Error.from_raw_os_error", lsm_type_unknown());
    }
    /* Free functions in std::io. */
    ADD_FUNC(NULL, "stdin",  "std.io.stdin",  lsm_type_unknown());
    ADD_FUNC(NULL, "stdout", "std.io.stdout", lsm_type_unknown());
    ADD_FUNC(NULL, "stderr", "std.io.stderr", lsm_type_unknown());
    ADD_FUNC(NULL, "copy",   "std.io.copy",   lsm_type_unknown());
    ADD_FUNC(NULL, "empty",  "std.io.empty",  lsm_type_unknown());
    ADD_FUNC(NULL, "sink",   "std.io.sink",   lsm_type_unknown());
    ADD_FUNC(NULL, "repeat", "std.io.repeat", lsm_type_unknown());

    /* ── std::fs ────────────────────────────────────────────────── */
    ADD_TYPE("std.fs.File",         "File",         false);
    ADD_TYPE("std.fs.OpenOptions",  "OpenOptions",  false);
    ADD_TYPE("std.fs.Metadata",     "Metadata",     false);
    ADD_TYPE("std.fs.Permissions",  "Permissions",  false);
    ADD_TYPE("std.fs.DirEntry",     "DirEntry",     false);
    ADD_TYPE("std.fs.ReadDir",      "ReadDir",      false);
    ADD_TYPE("std.fs.FileType",     "FileType",     false);
    ADD_TYPE("std.fs.DirBuilder",   "DirBuilder",   false);

    {
        const char* T = "std.fs.File";
        ADD_FUNC(T, "open",            "std.fs.File.open",            lsm_type_unknown());
        ADD_FUNC(T, "create",          "std.fs.File.create",          lsm_type_unknown());
        ADD_FUNC(T, "create_new",      "std.fs.File.create_new",      lsm_type_unknown());
        ADD_FUNC(T, "options",         "std.fs.File.options",         lsm_type_unknown());
        ADD_FUNC(T, "metadata",        "std.fs.File.metadata",        lsm_type_unknown());
        ADD_FUNC(T, "set_len",         "std.fs.File.set_len",         lsm_type_unknown());
        ADD_FUNC(T, "set_permissions", "std.fs.File.set_permissions", lsm_type_unknown());
        ADD_FUNC(T, "sync_all",        "std.fs.File.sync_all",        lsm_type_unknown());
        ADD_FUNC(T, "sync_data",       "std.fs.File.sync_data",       lsm_type_unknown());
        ADD_FUNC(T, "try_clone",       "std.fs.File.try_clone",       lsm_type_unknown());
    }
    {
        const char* T = "std.fs.OpenOptions";
        ADD_FUNC(T, "new",        "std.fs.OpenOptions.new",        lsm_type_unknown());
        ADD_FUNC(T, "read",       "std.fs.OpenOptions.read",       lsm_type_unknown());
        ADD_FUNC(T, "write",      "std.fs.OpenOptions.write",      lsm_type_unknown());
        ADD_FUNC(T, "append",     "std.fs.OpenOptions.append",     lsm_type_unknown());
        ADD_FUNC(T, "create",     "std.fs.OpenOptions.create",     lsm_type_unknown());
        ADD_FUNC(T, "create_new", "std.fs.OpenOptions.create_new", lsm_type_unknown());
        ADD_FUNC(T, "truncate",   "std.fs.OpenOptions.truncate",   lsm_type_unknown());
        ADD_FUNC(T, "open",       "std.fs.OpenOptions.open",       lsm_type_unknown());
    }
    {
        const char* T = "std.fs.Metadata";
        ADD_FUNC(T, "file_type",       "std.fs.Metadata.file_type",   lsm_type_unknown());
        ADD_FUNC(T, "is_dir",          "std.fs.Metadata.is_dir",      t_bool);
        ADD_FUNC(T, "is_file",         "std.fs.Metadata.is_file",     t_bool);
        ADD_FUNC(T, "is_symlink",      "std.fs.Metadata.is_symlink",  t_bool);
        ADD_FUNC(T, "len",             "std.fs.Metadata.len",         lsm_type_unknown());
        ADD_FUNC(T, "permissions",     "std.fs.Metadata.permissions", lsm_type_unknown());
        ADD_FUNC(T, "modified",        "std.fs.Metadata.modified",    lsm_type_unknown());
        ADD_FUNC(T, "accessed",        "std.fs.Metadata.accessed",    lsm_type_unknown());
        ADD_FUNC(T, "created",         "std.fs.Metadata.created",     lsm_type_unknown());
    }
    {
        const char* T = "std.fs.DirEntry";
        ADD_FUNC(T, "path",            "std.fs.DirEntry.path",        lsm_type_unknown());
        ADD_FUNC(T, "metadata",        "std.fs.DirEntry.metadata",    lsm_type_unknown());
        ADD_FUNC(T, "file_type",       "std.fs.DirEntry.file_type",   lsm_type_unknown());
        ADD_FUNC(T, "file_name",       "std.fs.DirEntry.file_name",   lsm_type_unknown());
    }
    /* Free functions in std::fs. */
    ADD_FUNC(NULL, "read",          "std.fs.read",          lsm_type_unknown());
    ADD_FUNC(NULL, "read_to_string","std.fs.read_to_string",lsm_type_unknown());
    ADD_FUNC(NULL, "read_dir",      "std.fs.read_dir",      lsm_type_unknown());
    ADD_FUNC(NULL, "write",         "std.fs.write",         lsm_type_unknown());
    ADD_FUNC(NULL, "remove_file",   "std.fs.remove_file",   lsm_type_unknown());
    ADD_FUNC(NULL, "remove_dir",    "std.fs.remove_dir",    lsm_type_unknown());
    ADD_FUNC(NULL, "remove_dir_all","std.fs.remove_dir_all",lsm_type_unknown());
    ADD_FUNC(NULL, "create_dir",    "std.fs.create_dir",    lsm_type_unknown());
    ADD_FUNC(NULL, "create_dir_all","std.fs.create_dir_all",lsm_type_unknown());
    ADD_FUNC(NULL, "metadata",      "std.fs.metadata",      lsm_type_unknown());
    ADD_FUNC(NULL, "rename",        "std.fs.rename",        lsm_type_unknown());
    ADD_FUNC(NULL, "copy",          "std.fs.copy",          lsm_type_unknown());
    ADD_FUNC(NULL, "hard_link",     "std.fs.hard_link",     lsm_type_unknown());

    /* ── std::path ─────────────────────────────────────────────── */
    ADD_TYPE("std.path.Path",       "Path",       false);
    ADD_TYPE("std.path.PathBuf",    "PathBuf",    false);
    ADD_TYPE("std.path.Component",  "Component",  false);
    ADD_TYPE("std.path.Components", "Components", false);
    ADD_TYPE("std.path.Iter",       "Iter",       false);
    ADD_TYPE("std.path.Display",    "Display",    false);

    {
        const char* T = "std.path.Path";
        ADD_FUNC(T, "new",             "std.path.Path.new",             lsm_type_unknown());
        ADD_FUNC(T, "exists",          "std.path.Path.exists",          t_bool);
        ADD_FUNC(T, "try_exists",      "std.path.Path.try_exists",      lsm_type_unknown());
        ADD_FUNC(T, "is_file",         "std.path.Path.is_file",         t_bool);
        ADD_FUNC(T, "is_dir",          "std.path.Path.is_dir",          t_bool);
        ADD_FUNC(T, "is_symlink",      "std.path.Path.is_symlink",      t_bool);
        ADD_FUNC(T, "is_absolute",     "std.path.Path.is_absolute",     t_bool);
        ADD_FUNC(T, "is_relative",     "std.path.Path.is_relative",     t_bool);
        ADD_FUNC(T, "has_root",        "std.path.Path.has_root",        t_bool);
        ADD_FUNC(T, "ends_with",       "std.path.Path.ends_with",       t_bool);
        ADD_FUNC(T, "starts_with",     "std.path.Path.starts_with",     t_bool);
        ADD_FUNC(T, "extension",       "std.path.Path.extension",       lsm_type_unknown());
        ADD_FUNC(T, "file_name",       "std.path.Path.file_name",       lsm_type_unknown());
        ADD_FUNC(T, "file_stem",       "std.path.Path.file_stem",       lsm_type_unknown());
        ADD_FUNC(T, "parent",          "std.path.Path.parent",          lsm_type_unknown());
        ADD_FUNC(T, "components",      "std.path.Path.components",      lsm_type_unknown());
        ADD_FUNC(T, "iter",            "std.path.Path.iter",            lsm_type_unknown());
        ADD_FUNC(T, "join",            "std.path.Path.join",            lsm_type_unknown());
        ADD_FUNC(T, "display",         "std.path.Path.display",         lsm_type_unknown());
        ADD_FUNC(T, "canonicalize",    "std.path.Path.canonicalize",    lsm_type_unknown());
        ADD_FUNC(T, "to_path_buf",     "std.path.Path.to_path_buf",     lsm_type_unknown());
        ADD_FUNC(T, "to_str",          "std.path.Path.to_str",          lsm_type_unknown());
        ADD_FUNC(T, "to_string_lossy", "std.path.Path.to_string_lossy", lsm_type_unknown());
        ADD_FUNC(T, "metadata",        "std.path.Path.metadata",        lsm_type_unknown());
        ADD_FUNC(T, "read_dir",        "std.path.Path.read_dir",        lsm_type_unknown());
        ADD_FUNC(T, "with_extension",  "std.path.Path.with_extension",  lsm_type_unknown());
        ADD_FUNC(T, "with_file_name",  "std.path.Path.with_file_name",  lsm_type_unknown());
    }
    {
        const char* T = "std.path.PathBuf";
        ADD_FUNC(T, "new",             "std.path.PathBuf.new",          lsm_type_unknown());
        ADD_FUNC(T, "from",            "std.path.PathBuf.from",         lsm_type_unknown());
        ADD_FUNC(T, "with_capacity",   "std.path.PathBuf.with_capacity",lsm_type_unknown());
        ADD_FUNC(T, "push",            "std.path.PathBuf.push",         t_unit);
        ADD_FUNC(T, "pop",             "std.path.PathBuf.pop",          t_bool);
        ADD_FUNC(T, "set_file_name",   "std.path.PathBuf.set_file_name",t_unit);
        ADD_FUNC(T, "set_extension",   "std.path.PathBuf.set_extension",t_bool);
        ADD_FUNC(T, "as_path",         "std.path.PathBuf.as_path",      lsm_type_unknown());
        ADD_FUNC(T, "into_os_string",  "std.path.PathBuf.into_os_string",lsm_type_unknown());
        ADD_FUNC(T, "clear",           "std.path.PathBuf.clear",        t_unit);
    }

    /* ── std::env ──────────────────────────────────────────────── */
    ADD_TYPE("std.env.Args",         "Args",         false);
    ADD_TYPE("std.env.Vars",         "Vars",         false);
    ADD_TYPE("std.env.VarError",     "VarError",     false);
    ADD_FUNC(NULL, "args",           "std.env.args",           lsm_type_unknown());
    ADD_FUNC(NULL, "args_os",        "std.env.args_os",        lsm_type_unknown());
    ADD_FUNC(NULL, "vars",           "std.env.vars",           lsm_type_unknown());
    ADD_FUNC(NULL, "vars_os",        "std.env.vars_os",        lsm_type_unknown());
    ADD_FUNC(NULL, "var_os",         "std.env.var_os",         lsm_type_unknown());
    ADD_FUNC(NULL, "set_var",        "std.env.set_var",        t_unit);
    ADD_FUNC(NULL, "remove_var",     "std.env.remove_var",     t_unit);
    ADD_FUNC(NULL, "current_dir",    "std.env.current_dir",    lsm_type_unknown());
    ADD_FUNC(NULL, "set_current_dir","std.env.set_current_dir",lsm_type_unknown());
    ADD_FUNC(NULL, "current_exe",    "std.env.current_exe",    lsm_type_unknown());
    ADD_FUNC(NULL, "temp_dir",       "std.env.temp_dir",       lsm_type_unknown());
    ADD_FUNC(NULL, "home_dir",       "std.env.home_dir",       lsm_type_unknown());

    /* ── std::process ──────────────────────────────────────────── */
    ADD_TYPE("std.process.Command",     "Command",     false);
    ADD_TYPE("std.process.Output",      "Output",      false);
    ADD_TYPE("std.process.Child",       "Child",       false);
    ADD_TYPE("std.process.ExitStatus",  "ExitStatus",  false);
    ADD_TYPE("std.process.Stdio",       "Stdio",       false);
    ADD_TYPE("std.process.ChildStdin",  "ChildStdin",  false);
    ADD_TYPE("std.process.ChildStdout", "ChildStdout", false);
    ADD_TYPE("std.process.ChildStderr", "ChildStderr", false);
    {
        const char* T = "std.process.Command";
        ADD_FUNC(T, "new",       "std.process.Command.new",       lsm_type_unknown());
        ADD_FUNC(T, "arg",       "std.process.Command.arg",       lsm_type_unknown());
        ADD_FUNC(T, "args",      "std.process.Command.args",      lsm_type_unknown());
        ADD_FUNC(T, "env",       "std.process.Command.env",       lsm_type_unknown());
        ADD_FUNC(T, "envs",      "std.process.Command.envs",      lsm_type_unknown());
        ADD_FUNC(T, "env_remove","std.process.Command.env_remove",lsm_type_unknown());
        ADD_FUNC(T, "env_clear", "std.process.Command.env_clear", lsm_type_unknown());
        ADD_FUNC(T, "current_dir","std.process.Command.current_dir",lsm_type_unknown());
        ADD_FUNC(T, "stdin",     "std.process.Command.stdin",     lsm_type_unknown());
        ADD_FUNC(T, "stdout",    "std.process.Command.stdout",    lsm_type_unknown());
        ADD_FUNC(T, "stderr",    "std.process.Command.stderr",    lsm_type_unknown());
        ADD_FUNC(T, "spawn",     "std.process.Command.spawn",     lsm_type_unknown());
        ADD_FUNC(T, "output",    "std.process.Command.output",    lsm_type_unknown());
        ADD_FUNC(T, "status",    "std.process.Command.status",    lsm_type_unknown());
    }
    {
        const char* T = "std.process.Output";
        ADD_FUNC(T, "status",    "std.process.Output.status",    lsm_type_unknown());
        ADD_FUNC(T, "stdout",    "std.process.Output.stdout",    lsm_type_unknown());
        ADD_FUNC(T, "stderr",    "std.process.Output.stderr",    lsm_type_unknown());
    }
    {
        const char* T = "std.process.Child";
        ADD_FUNC(T, "wait",      "std.process.Child.wait",      lsm_type_unknown());
        ADD_FUNC(T, "wait_with_output","std.process.Child.wait_with_output", lsm_type_unknown());
        ADD_FUNC(T, "kill",      "std.process.Child.kill",      lsm_type_unknown());
        ADD_FUNC(T, "id",        "std.process.Child.id",        lsm_type_unknown());
        ADD_FUNC(T, "try_wait",  "std.process.Child.try_wait",  lsm_type_unknown());
    }
    {
        const char* T = "std.process.ExitStatus";
        ADD_FUNC(T, "success",   "std.process.ExitStatus.success", t_bool);
        ADD_FUNC(T, "code",      "std.process.ExitStatus.code",    lsm_type_unknown());
    }
    {
        const char* T = "std.process.Stdio";
        ADD_FUNC(T, "inherit",   "std.process.Stdio.inherit",  lsm_type_unknown());
        ADD_FUNC(T, "piped",     "std.process.Stdio.piped",    lsm_type_unknown());
        ADD_FUNC(T, "null",      "std.process.Stdio.null",     lsm_type_unknown());
    }
    ADD_FUNC(NULL, "exit",      "std.process.exit",      lsm_type_unknown());
    ADD_FUNC(NULL, "abort",     "std.process.abort",     lsm_type_unknown());
    ADD_FUNC(NULL, "id",        "std.process.id",        lsm_type_unknown());

    /* ── std::time ─────────────────────────────────────────────── */
    ADD_TYPE("std.time.Duration",   "Duration",   false);
    ADD_TYPE("std.time.Instant",    "Instant",    false);
    ADD_TYPE("std.time.SystemTime", "SystemTime", false);
    ADD_TYPE("std.time.UNIX_EPOCH", "UNIX_EPOCH", false);

    {
        const char* T = "std.time.Duration";
        ADD_FUNC(T, "new",            "std.time.Duration.new",            lsm_type_unknown());
        ADD_FUNC(T, "from_secs",      "std.time.Duration.from_secs",      lsm_type_unknown());
        ADD_FUNC(T, "from_millis",    "std.time.Duration.from_millis",    lsm_type_unknown());
        ADD_FUNC(T, "from_micros",    "std.time.Duration.from_micros",    lsm_type_unknown());
        ADD_FUNC(T, "from_nanos",     "std.time.Duration.from_nanos",     lsm_type_unknown());
        ADD_FUNC(T, "from_secs_f64",  "std.time.Duration.from_secs_f64",  lsm_type_unknown());
        ADD_FUNC(T, "as_secs",        "std.time.Duration.as_secs",        lsm_type_unknown());
        ADD_FUNC(T, "as_millis",      "std.time.Duration.as_millis",      lsm_type_unknown());
        ADD_FUNC(T, "as_micros",      "std.time.Duration.as_micros",      lsm_type_unknown());
        ADD_FUNC(T, "as_nanos",       "std.time.Duration.as_nanos",       lsm_type_unknown());
        ADD_FUNC(T, "as_secs_f64",    "std.time.Duration.as_secs_f64",    lsm_type_unknown());
        ADD_FUNC(T, "subsec_millis",  "std.time.Duration.subsec_millis",  lsm_type_unknown());
        ADD_FUNC(T, "subsec_micros",  "std.time.Duration.subsec_micros",  lsm_type_unknown());
        ADD_FUNC(T, "subsec_nanos",   "std.time.Duration.subsec_nanos",   lsm_type_unknown());
        ADD_FUNC(T, "is_zero",        "std.time.Duration.is_zero",        t_bool);
        ADD_FUNC(T, "saturating_add", "std.time.Duration.saturating_add", lsm_type_unknown());
        ADD_FUNC(T, "saturating_sub", "std.time.Duration.saturating_sub", lsm_type_unknown());
        ADD_FUNC(T, "checked_add",    "std.time.Duration.checked_add",    lsm_type_unknown());
        ADD_FUNC(T, "checked_sub",    "std.time.Duration.checked_sub",    lsm_type_unknown());
    }
    {
        const char* T = "std.time.Instant";
        ADD_FUNC(T, "now",            "std.time.Instant.now",             lsm_type_unknown());
        ADD_FUNC(T, "elapsed",        "std.time.Instant.elapsed",         lsm_type_unknown());
        ADD_FUNC(T, "duration_since", "std.time.Instant.duration_since",  lsm_type_unknown());
        ADD_FUNC(T, "saturating_duration_since","std.time.Instant.saturating_duration_since", lsm_type_unknown());
        ADD_FUNC(T, "checked_duration_since","std.time.Instant.checked_duration_since",   lsm_type_unknown());
    }
    {
        const char* T = "std.time.SystemTime";
        ADD_FUNC(T, "now",            "std.time.SystemTime.now",          lsm_type_unknown());
        ADD_FUNC(T, "duration_since", "std.time.SystemTime.duration_since",lsm_type_unknown());
        ADD_FUNC(T, "elapsed",        "std.time.SystemTime.elapsed",      lsm_type_unknown());
    }

    /* ── std::thread ───────────────────────────────────────────── */
    ADD_TYPE("std.thread.JoinHandle", "JoinHandle", false);
    ADD_TYPE("std.thread.Thread",     "Thread",     false);
    ADD_TYPE("std.thread.Builder",    "Builder",    false);

    {
        const char* T = "std.thread.JoinHandle";
        ADD_FUNC(T, "join",     "std.thread.JoinHandle.join",     lsm_type_unknown());
        ADD_FUNC(T, "thread",   "std.thread.JoinHandle.thread",   lsm_type_unknown());
        ADD_FUNC(T, "is_finished","std.thread.JoinHandle.is_finished", t_bool);
    }
    {
        const char* T = "std.thread.Builder";
        ADD_FUNC(T, "new",      "std.thread.Builder.new",      lsm_type_unknown());
        ADD_FUNC(T, "name",     "std.thread.Builder.name",     lsm_type_unknown());
        ADD_FUNC(T, "stack_size","std.thread.Builder.stack_size", lsm_type_unknown());
        ADD_FUNC(T, "spawn",    "std.thread.Builder.spawn",    lsm_type_unknown());
    }
    {
        const char* T = "std.thread.Thread";
        ADD_FUNC(T, "name",     "std.thread.Thread.name",     lsm_type_unknown());
        ADD_FUNC(T, "id",       "std.thread.Thread.id",       lsm_type_unknown());
        ADD_FUNC(T, "unpark",   "std.thread.Thread.unpark",   t_unit);
    }
    /* thread::spawn returns JoinHandle<T> — model loosely as NAMED so
     * `let h = thread::spawn(…); h.join()` resolves through the
     * registered methods on JoinHandle. */
    ADD_FUNC(NULL, "spawn",        "std.thread.spawn",
        lsm_type_named(arena, "std.thread.JoinHandle"));
    ADD_FUNC(NULL, "sleep",        "std.thread.sleep",        t_unit);
    ADD_FUNC(NULL, "sleep_ms",     "std.thread.sleep_ms",     t_unit);
    ADD_FUNC(NULL, "yield_now",    "std.thread.yield_now",    t_unit);
    ADD_FUNC(NULL, "current",      "std.thread.current",
        lsm_type_named(arena, "std.thread.Thread"));
    ADD_FUNC(NULL, "park",         "std.thread.park",         t_unit);
    ADD_FUNC(NULL, "available_parallelism", "std.thread.available_parallelism", lsm_type_unknown());

    /* ── std::sync ─────────────────────────────────────────────── */
    ADD_TYPE("std.sync.Mutex",      "Mutex",      false);
    ADD_TYPE("std.sync.RwLock",     "RwLock",     false);
    ADD_TYPE("std.sync.Once",       "Once",       false);
    ADD_TYPE("std.sync.OnceLock",   "OnceLock",   false);
    ADD_TYPE("std.sync.Barrier",    "Barrier",    false);
    ADD_TYPE("std.sync.Condvar",    "Condvar",    false);
    ADD_TYPE("std.sync.MutexGuard", "MutexGuard", false);
    ADD_TYPE("std.sync.RwLockReadGuard","RwLockReadGuard", false);
    ADD_TYPE("std.sync.RwLockWriteGuard","RwLockWriteGuard",false);
    ADD_TYPE("std.sync.atomic.AtomicBool","AtomicBool", false);
    ADD_TYPE("std.sync.atomic.AtomicI32","AtomicI32", false);
    ADD_TYPE("std.sync.atomic.AtomicI64","AtomicI64", false);
    ADD_TYPE("std.sync.atomic.AtomicU32","AtomicU32", false);
    ADD_TYPE("std.sync.atomic.AtomicU64","AtomicU64", false);
    ADD_TYPE("std.sync.atomic.AtomicUsize","AtomicUsize", false);
    ADD_TYPE("std.sync.atomic.Ordering","Ordering", false);
    ADD_TYPE("std.sync.mpsc.Sender","Sender",   false);
    ADD_TYPE("std.sync.mpsc.Receiver","Receiver",false);
    ADD_TYPE("std.sync.mpsc.SyncSender","SyncSender",false);

    {
        const char* T = "std.sync.Mutex";
        ADD_FUNC(T, "new",      "std.sync.Mutex.new",      lsm_type_unknown());
        ADD_FUNC(T, "lock",     "std.sync.Mutex.lock",     lsm_type_unknown());
        ADD_FUNC(T, "try_lock", "std.sync.Mutex.try_lock", lsm_type_unknown());
        ADD_FUNC(T, "into_inner","std.sync.Mutex.into_inner", lsm_type_unknown());
        ADD_FUNC(T, "get_mut",  "std.sync.Mutex.get_mut",  lsm_type_unknown());
        ADD_FUNC(T, "is_poisoned","std.sync.Mutex.is_poisoned", t_bool);
    }
    {
        const char* T = "std.sync.RwLock";
        ADD_FUNC(T, "new",      "std.sync.RwLock.new",     lsm_type_unknown());
        ADD_FUNC(T, "read",     "std.sync.RwLock.read",    lsm_type_unknown());
        ADD_FUNC(T, "write",    "std.sync.RwLock.write",   lsm_type_unknown());
        ADD_FUNC(T, "try_read", "std.sync.RwLock.try_read",lsm_type_unknown());
        ADD_FUNC(T, "try_write","std.sync.RwLock.try_write",lsm_type_unknown());
    }
    {
        const char* T = "std.sync.Once";
        ADD_FUNC(T, "new",      "std.sync.Once.new",       lsm_type_unknown());
        ADD_FUNC(T, "call_once","std.sync.Once.call_once", t_unit);
        ADD_FUNC(T, "is_completed","std.sync.Once.is_completed", t_bool);
    }
    {
        const char* T = "std.sync.OnceLock";
        ADD_FUNC(T, "new",      "std.sync.OnceLock.new",   lsm_type_unknown());
        ADD_FUNC(T, "get",      "std.sync.OnceLock.get",   lsm_type_unknown());
        ADD_FUNC(T, "set",      "std.sync.OnceLock.set",   lsm_type_unknown());
        ADD_FUNC(T, "get_or_init","std.sync.OnceLock.get_or_init", lsm_type_unknown());
    }
    {
        const char* T = "std.sync.mpsc.Sender";
        ADD_FUNC(T, "send",     "std.sync.mpsc.Sender.send", lsm_type_unknown());
        ADD_FUNC(T, "clone",    "std.sync.mpsc.Sender.clone",lsm_type_unknown());
    }
    {
        const char* T = "std.sync.mpsc.Receiver";
        ADD_FUNC(T, "recv",     "std.sync.mpsc.Receiver.recv",     lsm_type_unknown());
        ADD_FUNC(T, "try_recv", "std.sync.mpsc.Receiver.try_recv", lsm_type_unknown());
        ADD_FUNC(T, "recv_timeout","std.sync.mpsc.Receiver.recv_timeout", lsm_type_unknown());
        ADD_FUNC(T, "iter",     "std.sync.mpsc.Receiver.iter",     lsm_type_unknown());
    }
    /* Atomics — share the same method-name surface across all integer
     * widths, so we register the same set on each type QN. */
    {
        const char* atomic_types[] = {
            "std.sync.atomic.AtomicBool", "std.sync.atomic.AtomicI32",
            "std.sync.atomic.AtomicI64", "std.sync.atomic.AtomicU32",
            "std.sync.atomic.AtomicU64", "std.sync.atomic.AtomicUsize",
            NULL};
        for (const char** T = atomic_types; *T; T++) {
            ADD_FUNC(*T, "new",          lsm_arena_sprintf(arena, "%s.new", *T),          lsm_type_unknown());
            ADD_FUNC(*T, "load",         lsm_arena_sprintf(arena, "%s.load", *T),         lsm_type_unknown());
            ADD_FUNC(*T, "store",        lsm_arena_sprintf(arena, "%s.store", *T),        t_unit);
            ADD_FUNC(*T, "swap",         lsm_arena_sprintf(arena, "%s.swap", *T),         lsm_type_unknown());
            ADD_FUNC(*T, "compare_exchange",lsm_arena_sprintf(arena, "%s.compare_exchange", *T), lsm_type_unknown());
            ADD_FUNC(*T, "fetch_add",    lsm_arena_sprintf(arena, "%s.fetch_add", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "fetch_sub",    lsm_arena_sprintf(arena, "%s.fetch_sub", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "fetch_and",    lsm_arena_sprintf(arena, "%s.fetch_and", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "fetch_or",     lsm_arena_sprintf(arena, "%s.fetch_or", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "fetch_xor",    lsm_arena_sprintf(arena, "%s.fetch_xor", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "into_inner",   lsm_arena_sprintf(arena, "%s.into_inner", *T),   lsm_type_unknown());
        }
    }
    /* mpsc free helpers. */
    ADD_FUNC(NULL, "channel",      "std.sync.mpsc.channel",      lsm_type_unknown());
    ADD_FUNC(NULL, "sync_channel", "std.sync.mpsc.sync_channel", lsm_type_unknown());

    /* ── std::net ──────────────────────────────────────────────── */
    ADD_TYPE("std.net.TcpStream",       "TcpStream",       false);
    ADD_TYPE("std.net.TcpListener",     "TcpListener",     false);
    ADD_TYPE("std.net.UdpSocket",       "UdpSocket",       false);
    ADD_TYPE("std.net.SocketAddr",      "SocketAddr",      false);
    ADD_TYPE("std.net.SocketAddrV4",    "SocketAddrV4",    false);
    ADD_TYPE("std.net.SocketAddrV6",    "SocketAddrV6",    false);
    ADD_TYPE("std.net.IpAddr",          "IpAddr",          false);
    ADD_TYPE("std.net.Ipv4Addr",        "Ipv4Addr",        false);
    ADD_TYPE("std.net.Ipv6Addr",        "Ipv6Addr",        false);

    {
        const char* T = "std.net.TcpStream";
        ADD_FUNC(T, "connect",   "std.net.TcpStream.connect", lsm_type_unknown());
        ADD_FUNC(T, "connect_timeout","std.net.TcpStream.connect_timeout", lsm_type_unknown());
        ADD_FUNC(T, "peer_addr", "std.net.TcpStream.peer_addr",lsm_type_unknown());
        ADD_FUNC(T, "local_addr","std.net.TcpStream.local_addr",lsm_type_unknown());
        ADD_FUNC(T, "shutdown",  "std.net.TcpStream.shutdown",lsm_type_unknown());
        ADD_FUNC(T, "try_clone", "std.net.TcpStream.try_clone",lsm_type_unknown());
        ADD_FUNC(T, "set_nonblocking","std.net.TcpStream.set_nonblocking", lsm_type_unknown());
        ADD_FUNC(T, "set_read_timeout","std.net.TcpStream.set_read_timeout", lsm_type_unknown());
        ADD_FUNC(T, "set_write_timeout","std.net.TcpStream.set_write_timeout", lsm_type_unknown());
    }
    {
        const char* T = "std.net.TcpListener";
        ADD_FUNC(T, "bind",      "std.net.TcpListener.bind",     lsm_type_unknown());
        ADD_FUNC(T, "accept",    "std.net.TcpListener.accept",   lsm_type_unknown());
        ADD_FUNC(T, "incoming",  "std.net.TcpListener.incoming", lsm_type_unknown());
        ADD_FUNC(T, "local_addr","std.net.TcpListener.local_addr",lsm_type_unknown());
        ADD_FUNC(T, "set_nonblocking","std.net.TcpListener.set_nonblocking", lsm_type_unknown());
    }
    {
        const char* T = "std.net.UdpSocket";
        ADD_FUNC(T, "bind",      "std.net.UdpSocket.bind",       lsm_type_unknown());
        ADD_FUNC(T, "recv_from", "std.net.UdpSocket.recv_from",  lsm_type_unknown());
        ADD_FUNC(T, "send_to",   "std.net.UdpSocket.send_to",    lsm_type_unknown());
        ADD_FUNC(T, "connect",   "std.net.UdpSocket.connect",    lsm_type_unknown());
        ADD_FUNC(T, "recv",      "std.net.UdpSocket.recv",       lsm_type_unknown());
        ADD_FUNC(T, "send",      "std.net.UdpSocket.send",       lsm_type_unknown());
    }

    /* ── core::str / &str method surface ───────────────────────── */
    /* Most str methods already added above; round out parsing helpers. */
    {
        const char* T = "core.str";
        ADD_FUNC(T, "split_whitespace","core.str.split_whitespace", lsm_type_unknown());
        ADD_FUNC(T, "split_terminator","core.str.split_terminator", lsm_type_unknown());
        ADD_FUNC(T, "rsplit",          "core.str.rsplit",           lsm_type_unknown());
        ADD_FUNC(T, "rsplitn",         "core.str.rsplitn",          lsm_type_unknown());
        ADD_FUNC(T, "trim_start",      "core.str.trim_start",       t_str_ref);
        ADD_FUNC(T, "trim_end",        "core.str.trim_end",         t_str_ref);
        ADD_FUNC(T, "trim_start_matches","core.str.trim_start_matches", t_str_ref);
        ADD_FUNC(T, "trim_end_matches","core.str.trim_end_matches", t_str_ref);
        ADD_FUNC(T, "matches",         "core.str.matches",          lsm_type_unknown());
        ADD_FUNC(T, "match_indices",   "core.str.match_indices",    lsm_type_unknown());
        ADD_FUNC(T, "char_indices",    "core.str.char_indices",     lsm_type_unknown());
        ADD_FUNC(T, "encode_utf16",    "core.str.encode_utf16",     lsm_type_unknown());
        ADD_FUNC(T, "is_ascii",        "core.str.is_ascii",         t_bool);
        ADD_FUNC(T, "eq_ignore_ascii_case","core.str.eq_ignore_ascii_case", t_bool);
        ADD_FUNC(T, "make_ascii_uppercase","core.str.make_ascii_uppercase", t_unit);
        ADD_FUNC(T, "make_ascii_lowercase","core.str.make_ascii_lowercase", t_unit);
        ADD_FUNC(T, "to_ascii_uppercase","core.str.to_ascii_uppercase", t_self_named_string);
        ADD_FUNC(T, "to_ascii_lowercase","core.str.to_ascii_lowercase", t_self_named_string);
        ADD_FUNC(T, "char_at",         "core.str.char_at",          lsm_type_unknown());
        ADD_FUNC(T, "get",             "core.str.get",              lsm_type_unknown());
        ADD_FUNC(T, "repeat",          "core.str.repeat",           t_self_named_string);
        ADD_FUNC(T, "rfind",           "core.str.rfind",            lsm_type_unknown());
        ADD_FUNC(T, "split_at",        "core.str.split_at",         lsm_type_unknown());
        ADD_FUNC(T, "split_once",      "core.str.split_once",       lsm_type_unknown());
        ADD_FUNC(T, "rsplit_once",     "core.str.rsplit_once",      lsm_type_unknown());
    }

    /* ── Primitive integer / float method surfaces ────────────── */
    {
        /* Each numeric type shares ~50 methods. We register them under
         * each primitive's QN so `42i32.abs()` resolves to `i32.abs`. */
        const char* int_types[] = {
            "i8", "i16", "i32", "i64", "i128", "isize",
            "u8", "u16", "u32", "u64", "u128", "usize", NULL};
        for (const char** T = int_types; *T; T++) {
            ADD_FUNC(*T, "abs",            lsm_arena_sprintf(arena, "%s.abs", *T),            lsm_type_unknown());
            ADD_FUNC(*T, "signum",         lsm_arena_sprintf(arena, "%s.signum", *T),         lsm_type_unknown());
            ADD_FUNC(*T, "pow",            lsm_arena_sprintf(arena, "%s.pow", *T),            lsm_type_unknown());
            ADD_FUNC(*T, "checked_add",    lsm_arena_sprintf(arena, "%s.checked_add", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "checked_sub",    lsm_arena_sprintf(arena, "%s.checked_sub", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "checked_mul",    lsm_arena_sprintf(arena, "%s.checked_mul", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "checked_div",    lsm_arena_sprintf(arena, "%s.checked_div", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "checked_rem",    lsm_arena_sprintf(arena, "%s.checked_rem", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "saturating_add", lsm_arena_sprintf(arena, "%s.saturating_add", *T), lsm_type_unknown());
            ADD_FUNC(*T, "saturating_sub", lsm_arena_sprintf(arena, "%s.saturating_sub", *T), lsm_type_unknown());
            ADD_FUNC(*T, "saturating_mul", lsm_arena_sprintf(arena, "%s.saturating_mul", *T), lsm_type_unknown());
            ADD_FUNC(*T, "wrapping_add",   lsm_arena_sprintf(arena, "%s.wrapping_add", *T),   lsm_type_unknown());
            ADD_FUNC(*T, "wrapping_sub",   lsm_arena_sprintf(arena, "%s.wrapping_sub", *T),   lsm_type_unknown());
            ADD_FUNC(*T, "wrapping_mul",   lsm_arena_sprintf(arena, "%s.wrapping_mul", *T),   lsm_type_unknown());
            ADD_FUNC(*T, "overflowing_add",lsm_arena_sprintf(arena, "%s.overflowing_add", *T),lsm_type_unknown());
            ADD_FUNC(*T, "leading_zeros",  lsm_arena_sprintf(arena, "%s.leading_zeros", *T),  lsm_type_builtin(arena, "u32"));
            ADD_FUNC(*T, "trailing_zeros", lsm_arena_sprintf(arena, "%s.trailing_zeros", *T), lsm_type_builtin(arena, "u32"));
            ADD_FUNC(*T, "count_ones",     lsm_arena_sprintf(arena, "%s.count_ones", *T),     lsm_type_builtin(arena, "u32"));
            ADD_FUNC(*T, "count_zeros",    lsm_arena_sprintf(arena, "%s.count_zeros", *T),    lsm_type_builtin(arena, "u32"));
            ADD_FUNC(*T, "swap_bytes",     lsm_arena_sprintf(arena, "%s.swap_bytes", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "rotate_left",    lsm_arena_sprintf(arena, "%s.rotate_left", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "rotate_right",   lsm_arena_sprintf(arena, "%s.rotate_right", *T),   lsm_type_unknown());
            ADD_FUNC(*T, "to_string",      lsm_arena_sprintf(arena, "%s.to_string", *T),      t_self_named_string);
            ADD_FUNC(*T, "to_be",          lsm_arena_sprintf(arena, "%s.to_be", *T),          lsm_type_unknown());
            ADD_FUNC(*T, "to_le",          lsm_arena_sprintf(arena, "%s.to_le", *T),          lsm_type_unknown());
            ADD_FUNC(*T, "to_be_bytes",    lsm_arena_sprintf(arena, "%s.to_be_bytes", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "to_le_bytes",    lsm_arena_sprintf(arena, "%s.to_le_bytes", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "from_be_bytes",  lsm_arena_sprintf(arena, "%s.from_be_bytes", *T),  lsm_type_unknown());
            ADD_FUNC(*T, "from_le_bytes",  lsm_arena_sprintf(arena, "%s.from_le_bytes", *T),  lsm_type_unknown());
            ADD_FUNC(*T, "min",            lsm_arena_sprintf(arena, "%s.min", *T),            lsm_type_unknown());
            ADD_FUNC(*T, "max",            lsm_arena_sprintf(arena, "%s.max", *T),            lsm_type_unknown());
            ADD_FUNC(*T, "clamp",          lsm_arena_sprintf(arena, "%s.clamp", *T),          lsm_type_unknown());
            ADD_FUNC(*T, "isqrt",          lsm_arena_sprintf(arena, "%s.isqrt", *T),          lsm_type_unknown());
            ADD_FUNC(*T, "is_power_of_two",lsm_arena_sprintf(arena, "%s.is_power_of_two", *T),t_bool);
            ADD_FUNC(*T, "next_power_of_two",lsm_arena_sprintf(arena, "%s.next_power_of_two", *T), lsm_type_unknown());
            ADD_FUNC(*T, "div_euclid",     lsm_arena_sprintf(arena, "%s.div_euclid", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "rem_euclid",     lsm_arena_sprintf(arena, "%s.rem_euclid", *T),     lsm_type_unknown());
        }

        /* Float types share a different surface. */
        const char* float_types[] = {"f32", "f64", NULL};
        for (const char** T = float_types; *T; T++) {
            ADD_FUNC(*T, "abs",      lsm_arena_sprintf(arena, "%s.abs", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "sqrt",     lsm_arena_sprintf(arena, "%s.sqrt", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "cbrt",     lsm_arena_sprintf(arena, "%s.cbrt", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "floor",    lsm_arena_sprintf(arena, "%s.floor", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "ceil",     lsm_arena_sprintf(arena, "%s.ceil", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "round",    lsm_arena_sprintf(arena, "%s.round", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "trunc",    lsm_arena_sprintf(arena, "%s.trunc", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "fract",    lsm_arena_sprintf(arena, "%s.fract", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "powi",     lsm_arena_sprintf(arena, "%s.powi", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "powf",     lsm_arena_sprintf(arena, "%s.powf", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "exp",      lsm_arena_sprintf(arena, "%s.exp", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "exp2",     lsm_arena_sprintf(arena, "%s.exp2", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "ln",       lsm_arena_sprintf(arena, "%s.ln", *T),       lsm_type_unknown());
            ADD_FUNC(*T, "log",      lsm_arena_sprintf(arena, "%s.log", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "log2",     lsm_arena_sprintf(arena, "%s.log2", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "log10",    lsm_arena_sprintf(arena, "%s.log10", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "sin",      lsm_arena_sprintf(arena, "%s.sin", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "cos",      lsm_arena_sprintf(arena, "%s.cos", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "tan",      lsm_arena_sprintf(arena, "%s.tan", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "asin",     lsm_arena_sprintf(arena, "%s.asin", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "acos",     lsm_arena_sprintf(arena, "%s.acos", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "atan",     lsm_arena_sprintf(arena, "%s.atan", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "atan2",    lsm_arena_sprintf(arena, "%s.atan2", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "sinh",     lsm_arena_sprintf(arena, "%s.sinh", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "cosh",     lsm_arena_sprintf(arena, "%s.cosh", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "tanh",     lsm_arena_sprintf(arena, "%s.tanh", *T),     lsm_type_unknown());
            ADD_FUNC(*T, "to_radians",lsm_arena_sprintf(arena, "%s.to_radians", *T), lsm_type_unknown());
            ADD_FUNC(*T, "to_degrees",lsm_arena_sprintf(arena, "%s.to_degrees", *T), lsm_type_unknown());
            ADD_FUNC(*T, "is_nan",   lsm_arena_sprintf(arena, "%s.is_nan", *T),   t_bool);
            ADD_FUNC(*T, "is_infinite",lsm_arena_sprintf(arena, "%s.is_infinite", *T), t_bool);
            ADD_FUNC(*T, "is_finite",lsm_arena_sprintf(arena, "%s.is_finite", *T), t_bool);
            ADD_FUNC(*T, "is_sign_positive",lsm_arena_sprintf(arena, "%s.is_sign_positive", *T), t_bool);
            ADD_FUNC(*T, "is_sign_negative",lsm_arena_sprintf(arena, "%s.is_sign_negative", *T), t_bool);
            ADD_FUNC(*T, "min",      lsm_arena_sprintf(arena, "%s.min", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "max",      lsm_arena_sprintf(arena, "%s.max", *T),      lsm_type_unknown());
            ADD_FUNC(*T, "clamp",    lsm_arena_sprintf(arena, "%s.clamp", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "hypot",    lsm_arena_sprintf(arena, "%s.hypot", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "to_string",lsm_arena_sprintf(arena, "%s.to_string", *T),t_self_named_string);
            ADD_FUNC(*T, "to_bits",  lsm_arena_sprintf(arena, "%s.to_bits", *T),  lsm_type_unknown());
            ADD_FUNC(*T, "from_bits",lsm_arena_sprintf(arena, "%s.from_bits", *T),lsm_type_unknown());
            ADD_FUNC(*T, "copysign", lsm_arena_sprintf(arena, "%s.copysign", *T), lsm_type_unknown());
            ADD_FUNC(*T, "mul_add",  lsm_arena_sprintf(arena, "%s.mul_add", *T),  lsm_type_unknown());
            ADD_FUNC(*T, "recip",    lsm_arena_sprintf(arena, "%s.recip", *T),    lsm_type_unknown());
            ADD_FUNC(*T, "signum",   lsm_arena_sprintf(arena, "%s.signum", *T),   lsm_type_unknown());
        }

        /* char methods. */
        ADD_FUNC("char", "is_alphabetic",  "char.is_alphabetic",  t_bool);
        ADD_FUNC("char", "is_alphanumeric","char.is_alphanumeric",t_bool);
        ADD_FUNC("char", "is_numeric",     "char.is_numeric",     t_bool);
        ADD_FUNC("char", "is_ascii",       "char.is_ascii",       t_bool);
        ADD_FUNC("char", "is_ascii_alphabetic","char.is_ascii_alphabetic", t_bool);
        ADD_FUNC("char", "is_ascii_alphanumeric","char.is_ascii_alphanumeric", t_bool);
        ADD_FUNC("char", "is_digit",       "char.is_digit",       t_bool);
        ADD_FUNC("char", "is_uppercase",   "char.is_uppercase",   t_bool);
        ADD_FUNC("char", "is_lowercase",   "char.is_lowercase",   t_bool);
        ADD_FUNC("char", "is_whitespace",  "char.is_whitespace",  t_bool);
        ADD_FUNC("char", "is_control",     "char.is_control",     t_bool);
        ADD_FUNC("char", "to_lowercase",   "char.to_lowercase",   lsm_type_unknown());
        ADD_FUNC("char", "to_uppercase",   "char.to_uppercase",   lsm_type_unknown());
        ADD_FUNC("char", "to_ascii_lowercase","char.to_ascii_lowercase", lsm_type_builtin(arena, "char"));
        ADD_FUNC("char", "to_ascii_uppercase","char.to_ascii_uppercase", lsm_type_builtin(arena, "char"));
        ADD_FUNC("char", "to_digit",       "char.to_digit",       lsm_type_unknown());
        ADD_FUNC("char", "len_utf8",       "char.len_utf8",       t_usize);
        ADD_FUNC("char", "len_utf16",      "char.len_utf16",      t_usize);
        ADD_FUNC("char", "to_string",      "char.to_string",      t_self_named_string);
    }

    /* ── core::slice / [T] methods ─────────────────────────────── */
    ADD_TYPE("core.slice", "slice", false);
    {
        const char* T = "core.slice";
        ADD_FUNC(T, "len",            "core.slice.len",            t_usize);
        ADD_FUNC(T, "is_empty",       "core.slice.is_empty",       t_bool);
        ADD_FUNC(T, "first",          "core.slice.first",          lsm_type_unknown());
        ADD_FUNC(T, "last",           "core.slice.last",           lsm_type_unknown());
        ADD_FUNC(T, "get",            "core.slice.get",            lsm_type_unknown());
        ADD_FUNC(T, "iter",           "core.slice.iter",           lsm_type_unknown());
        ADD_FUNC(T, "iter_mut",       "core.slice.iter_mut",       lsm_type_unknown());
        ADD_FUNC(T, "windows",        "core.slice.windows",        lsm_type_unknown());
        ADD_FUNC(T, "chunks",         "core.slice.chunks",         lsm_type_unknown());
        ADD_FUNC(T, "chunks_exact",   "core.slice.chunks_exact",   lsm_type_unknown());
        ADD_FUNC(T, "split",          "core.slice.split",          lsm_type_unknown());
        ADD_FUNC(T, "split_at",       "core.slice.split_at",       lsm_type_unknown());
        ADD_FUNC(T, "split_first",    "core.slice.split_first",    lsm_type_unknown());
        ADD_FUNC(T, "split_last",     "core.slice.split_last",     lsm_type_unknown());
        ADD_FUNC(T, "binary_search",  "core.slice.binary_search",  lsm_type_unknown());
        ADD_FUNC(T, "sort",           "core.slice.sort",           t_unit);
        ADD_FUNC(T, "sort_by",        "core.slice.sort_by",        t_unit);
        ADD_FUNC(T, "sort_by_key",    "core.slice.sort_by_key",    t_unit);
        ADD_FUNC(T, "sort_unstable",  "core.slice.sort_unstable",  t_unit);
        ADD_FUNC(T, "reverse",        "core.slice.reverse",        t_unit);
        ADD_FUNC(T, "fill",           "core.slice.fill",           t_unit);
        ADD_FUNC(T, "swap",           "core.slice.swap",           t_unit);
        ADD_FUNC(T, "contains",       "core.slice.contains",       t_bool);
        ADD_FUNC(T, "starts_with",    "core.slice.starts_with",    t_bool);
        ADD_FUNC(T, "ends_with",      "core.slice.ends_with",      t_bool);
        ADD_FUNC(T, "join",           "core.slice.join",           lsm_type_unknown());
        ADD_FUNC(T, "concat",         "core.slice.concat",         lsm_type_unknown());
        ADD_FUNC(T, "to_vec",         "core.slice.to_vec",         lsm_type_unknown());
        ADD_FUNC(T, "as_ptr",         "core.slice.as_ptr",         lsm_type_unknown());
        ADD_FUNC(T, "as_mut_ptr",     "core.slice.as_mut_ptr",     lsm_type_unknown());
        ADD_FUNC(T, "copy_from_slice","core.slice.copy_from_slice",t_unit);
        ADD_FUNC(T, "clone_from_slice","core.slice.clone_from_slice", t_unit);
        ADD_FUNC(T, "rotate_left",    "core.slice.rotate_left",    t_unit);
        ADD_FUNC(T, "rotate_right",   "core.slice.rotate_right",   t_unit);
    }

    /* ── core::cmp helpers ─────────────────────────────────────── */
    ADD_FUNC(NULL, "min",         "core.cmp.min",         lsm_type_unknown());
    ADD_FUNC(NULL, "max",         "core.cmp.max",         lsm_type_unknown());
    ADD_FUNC(NULL, "min_by",      "core.cmp.min_by",      lsm_type_unknown());
    ADD_FUNC(NULL, "max_by",      "core.cmp.max_by",      lsm_type_unknown());
    ADD_FUNC(NULL, "min_by_key",  "core.cmp.min_by_key",  lsm_type_unknown());
    ADD_FUNC(NULL, "max_by_key",  "core.cmp.max_by_key",  lsm_type_unknown());

    /* ── core::error::Error trait ──────────────────────────────── */
    ADD_TYPE("core.error.Error", "Error", true);
    {
        const char* T = "core.error.Error";
        ADD_FUNC(T, "source",      "core.error.Error.source",     lsm_type_unknown());
        ADD_FUNC(T, "description", "core.error.Error.description",t_str_ref);
        ADD_FUNC(T, "cause",       "core.error.Error.cause",      lsm_type_unknown());
    }

    /* ── std::error re-export ──────────────────────────────────── */
    {
        LSMRegisteredType rt;
        memset(&rt, 0, sizeof(rt));
        rt.qualified_name = "std.error.Error";
        rt.short_name = "Error";
        rt.alias_of = "core.error.Error";
        rt.is_interface = true;
        lsm_registry_add_type(reg, rt);
    }

    /* ── Range / RangeInclusive / RangeFull ────────────────────── */
    ADD_TYPE("core.ops.Range",          "Range",         false);
    ADD_TYPE("core.ops.RangeInclusive", "RangeInclusive",false);
    ADD_TYPE("core.ops.RangeFrom",      "RangeFrom",     false);
    ADD_TYPE("core.ops.RangeTo",        "RangeTo",       false);
    ADD_TYPE("core.ops.RangeToInclusive","RangeToInclusive", false);
    ADD_TYPE("core.ops.RangeFull",      "RangeFull",     false);
    {
        const char* T = "core.ops.Range";
        ADD_FUNC(T, "contains", "core.ops.Range.contains", t_bool);
        ADD_FUNC(T, "is_empty", "core.ops.Range.is_empty", t_bool);
        ADD_FUNC(T, "start",    "core.ops.Range.start",    lsm_type_unknown());
        ADD_FUNC(T, "end",      "core.ops.Range.end",      lsm_type_unknown());
        ADD_FUNC(T, "len",      "core.ops.Range.len",      t_usize);
    }

    /* ── core::iter additions (sum/product/zip etc. already added) ─ */
    {
        const char* T = "core.iter.Iterator";
        ADD_FUNC(T, "product",   "core.iter.Iterator.product",   lsm_type_unknown());
        ADD_FUNC(T, "last",      "core.iter.Iterator.last",      lsm_type_unknown());
        ADD_FUNC(T, "nth",       "core.iter.Iterator.nth",       lsm_type_unknown());
        ADD_FUNC(T, "size_hint", "core.iter.Iterator.size_hint", lsm_type_unknown());
        ADD_FUNC(T, "by_ref",    "core.iter.Iterator.by_ref",    lsm_type_unknown());
        ADD_FUNC(T, "max_by",    "core.iter.Iterator.max_by",    lsm_type_unknown());
        ADD_FUNC(T, "min_by",    "core.iter.Iterator.min_by",    lsm_type_unknown());
        ADD_FUNC(T, "max_by_key","core.iter.Iterator.max_by_key",lsm_type_unknown());
        ADD_FUNC(T, "min_by_key","core.iter.Iterator.min_by_key",lsm_type_unknown());
        ADD_FUNC(T, "scan",      "core.iter.Iterator.scan",      lsm_type_unknown());
        ADD_FUNC(T, "inspect",   "core.iter.Iterator.inspect",   lsm_type_unknown());
        ADD_FUNC(T, "fuse",      "core.iter.Iterator.fuse",      lsm_type_unknown());
        ADD_FUNC(T, "cycle",     "core.iter.Iterator.cycle",     lsm_type_unknown());
        ADD_FUNC(T, "take_while","core.iter.Iterator.take_while",lsm_type_unknown());
        ADD_FUNC(T, "skip_while","core.iter.Iterator.skip_while",lsm_type_unknown());
        ADD_FUNC(T, "try_fold",  "core.iter.Iterator.try_fold",  lsm_type_unknown());
        ADD_FUNC(T, "try_for_each","core.iter.Iterator.try_for_each", lsm_type_unknown());
        ADD_FUNC(T, "reduce",    "core.iter.Iterator.reduce",    lsm_type_unknown());
        ADD_FUNC(T, "unzip",     "core.iter.Iterator.unzip",     lsm_type_unknown());
        ADD_FUNC(T, "partition", "core.iter.Iterator.partition", lsm_type_unknown());
        ADD_FUNC(T, "into_iter", "core.iter.Iterator.into_iter", lsm_type_unknown());
    }
    {
        const char* T = "core.iter.IntoIterator";
        ADD_FUNC(T, "into_iter", "core.iter.IntoIterator.into_iter", lsm_type_unknown());
    }
    /* Free helpers in core::iter. */
    ADD_FUNC(NULL, "once",          "core.iter.once",          lsm_type_unknown());
    ADD_FUNC(NULL, "empty",         "core.iter.empty",         lsm_type_unknown());
    ADD_FUNC(NULL, "repeat",        "core.iter.repeat",        lsm_type_unknown());
    ADD_FUNC(NULL, "from_fn",       "core.iter.from_fn",       lsm_type_unknown());
    ADD_FUNC(NULL, "successors",    "core.iter.successors",    lsm_type_unknown());
    ADD_FUNC(NULL, "zip",           "core.iter.zip",           lsm_type_unknown());

    /* ── Pin and Future helpers ───────────────────────────────── */
    ADD_TYPE("core.pin.Pin", "Pin", false);
    {
        const char* T = "core.pin.Pin";
        ADD_FUNC(T, "new",          "core.pin.Pin.new",          lsm_type_unknown());
        ADD_FUNC(T, "as_ref",       "core.pin.Pin.as_ref",       lsm_type_unknown());
        ADD_FUNC(T, "as_mut",       "core.pin.Pin.as_mut",       lsm_type_unknown());
        ADD_FUNC(T, "into_inner",   "core.pin.Pin.into_inner",   lsm_type_unknown());
    }
    {
        const char* T = "core.future.Future";
        ADD_FUNC(T, "poll",     "core.future.Future.poll",     lsm_type_unknown());
    }

    /* ── std::collections additions ────────────────────────────── */
    ADD_TYPE("alloc.collections.LinkedList", "LinkedList", false);
    ADD_TYPE("alloc.collections.BinaryHeap", "BinaryHeap", false);
    {
        const char* T = "alloc.collections.LinkedList";
        ADD_FUNC(T, "new",       "alloc.collections.LinkedList.new",       lsm_type_unknown());
        ADD_FUNC(T, "push_back", "alloc.collections.LinkedList.push_back", t_unit);
        ADD_FUNC(T, "push_front","alloc.collections.LinkedList.push_front",t_unit);
        ADD_FUNC(T, "pop_back",  "alloc.collections.LinkedList.pop_back",  lsm_type_unknown());
        ADD_FUNC(T, "pop_front", "alloc.collections.LinkedList.pop_front", lsm_type_unknown());
        ADD_FUNC(T, "len",       "alloc.collections.LinkedList.len",       t_usize);
        ADD_FUNC(T, "is_empty",  "alloc.collections.LinkedList.is_empty",  t_bool);
        ADD_FUNC(T, "iter",      "alloc.collections.LinkedList.iter",      lsm_type_unknown());
    }
    {
        const char* T = "alloc.collections.BinaryHeap";
        ADD_FUNC(T, "new",       "alloc.collections.BinaryHeap.new",       lsm_type_unknown());
        ADD_FUNC(T, "push",      "alloc.collections.BinaryHeap.push",      t_unit);
        ADD_FUNC(T, "pop",       "alloc.collections.BinaryHeap.pop",       lsm_type_unknown());
        ADD_FUNC(T, "peek",      "alloc.collections.BinaryHeap.peek",      lsm_type_unknown());
        ADD_FUNC(T, "len",       "alloc.collections.BinaryHeap.len",       t_usize);
        ADD_FUNC(T, "into_sorted_vec","alloc.collections.BinaryHeap.into_sorted_vec", lsm_type_unknown());
    }
    {
        const char* T = "alloc.collections.HashSet";
        ADD_FUNC(T, "new",        "alloc.collections.HashSet.new",        lsm_type_unknown());
        ADD_FUNC(T, "insert",     "alloc.collections.HashSet.insert",     t_bool);
        ADD_FUNC(T, "remove",     "alloc.collections.HashSet.remove",     t_bool);
        ADD_FUNC(T, "contains",   "alloc.collections.HashSet.contains",   t_bool);
        ADD_FUNC(T, "len",        "alloc.collections.HashSet.len",        t_usize);
        ADD_FUNC(T, "is_empty",   "alloc.collections.HashSet.is_empty",   t_bool);
        ADD_FUNC(T, "clear",      "alloc.collections.HashSet.clear",      t_unit);
        ADD_FUNC(T, "iter",       "alloc.collections.HashSet.iter",       lsm_type_unknown());
        ADD_FUNC(T, "intersection","alloc.collections.HashSet.intersection",lsm_type_unknown());
        ADD_FUNC(T, "union",      "alloc.collections.HashSet.union",      lsm_type_unknown());
        ADD_FUNC(T, "difference", "alloc.collections.HashSet.difference", lsm_type_unknown());
    }
    {
        const char* T = "alloc.collections.VecDeque";
        ADD_FUNC(T, "new",       "alloc.collections.VecDeque.new",       lsm_type_unknown());
        ADD_FUNC(T, "push_back", "alloc.collections.VecDeque.push_back", t_unit);
        ADD_FUNC(T, "push_front","alloc.collections.VecDeque.push_front",t_unit);
        ADD_FUNC(T, "pop_back",  "alloc.collections.VecDeque.pop_back",  lsm_type_unknown());
        ADD_FUNC(T, "pop_front", "alloc.collections.VecDeque.pop_front", lsm_type_unknown());
        ADD_FUNC(T, "len",       "alloc.collections.VecDeque.len",       t_usize);
        ADD_FUNC(T, "is_empty",  "alloc.collections.VecDeque.is_empty",  t_bool);
        ADD_FUNC(T, "front",     "alloc.collections.VecDeque.front",     lsm_type_unknown());
        ADD_FUNC(T, "back",      "alloc.collections.VecDeque.back",      lsm_type_unknown());
        ADD_FUNC(T, "iter",      "alloc.collections.VecDeque.iter",      lsm_type_unknown());
    }

    /* ════════════════════════════════════════════════════════════════
     * EXTRA stdlib breadth — push from ~700 to ~2500+ methods so the
     * gap-class tests that exercise less-common surfaces find hits.
     *
     * Sections below register: ffi (CStr/CString/OsStr/OsString),
     * std::ptr, std::mem extras, additional Range-family methods,
     * std::str functions (parse, from_utf8), additional collection
     * methods (drain/range/extend), HashSet operations, BinaryHeap,
     * additional Box/Rc/Arc/Pin methods, additional Mutex/RwLock
     * guards, additional Atomic types and variants, additional thread
     * Builder, std::process extras, std::time extras, more std::sync
     * (Once/Barrier/Condvar), additional std::path methods, raw ptr
     * arithmetic, Cell methods, plus per-numeric-type extras.
     * ════════════════════════════════════════════════════════════════ */

    /* ── std::ffi ─────────────────────────────────────────────── */
    ADD_TYPE("std.ffi.CStr",      "CStr",      false);
    ADD_TYPE("std.ffi.CString",   "CString",   false);
    ADD_TYPE("std.ffi.OsStr",     "OsStr",     false);
    ADD_TYPE("std.ffi.OsString",  "OsString",  false);
    ADD_TYPE("std.ffi.NulError",  "NulError",  false);
    ADD_TYPE("std.ffi.IntoStringError","IntoStringError", false);
    ADD_TYPE("std.ffi.FromBytesWithNulError","FromBytesWithNulError", false);
    {
        const char* T = "std.ffi.CString";
        ADD_FUNC(T, "new",          "std.ffi.CString.new",          lsm_type_unknown());
        ADD_FUNC(T, "as_c_str",     "std.ffi.CString.as_c_str",     lsm_type_unknown());
        ADD_FUNC(T, "as_bytes",     "std.ffi.CString.as_bytes",     lsm_type_unknown());
        ADD_FUNC(T, "as_bytes_with_nul","std.ffi.CString.as_bytes_with_nul", lsm_type_unknown());
        ADD_FUNC(T, "into_bytes",   "std.ffi.CString.into_bytes",   lsm_type_unknown());
        ADD_FUNC(T, "into_bytes_with_nul","std.ffi.CString.into_bytes_with_nul", lsm_type_unknown());
        ADD_FUNC(T, "into_string",  "std.ffi.CString.into_string",  lsm_type_unknown());
        ADD_FUNC(T, "into_raw",     "std.ffi.CString.into_raw",     lsm_type_unknown());
        ADD_FUNC(T, "from_raw",     "std.ffi.CString.from_raw",     lsm_type_unknown());
        ADD_FUNC(T, "from_vec_unchecked","std.ffi.CString.from_vec_unchecked", lsm_type_unknown());
    }
    {
        const char* T = "std.ffi.CStr";
        ADD_FUNC(T, "from_ptr",     "std.ffi.CStr.from_ptr",        lsm_type_unknown());
        ADD_FUNC(T, "from_bytes_with_nul","std.ffi.CStr.from_bytes_with_nul", lsm_type_unknown());
        ADD_FUNC(T, "to_str",       "std.ffi.CStr.to_str",          lsm_type_unknown());
        ADD_FUNC(T, "to_string_lossy","std.ffi.CStr.to_string_lossy", lsm_type_unknown());
        ADD_FUNC(T, "to_bytes",     "std.ffi.CStr.to_bytes",        lsm_type_unknown());
        ADD_FUNC(T, "to_bytes_with_nul","std.ffi.CStr.to_bytes_with_nul", lsm_type_unknown());
        ADD_FUNC(T, "as_ptr",       "std.ffi.CStr.as_ptr",          lsm_type_unknown());
    }
    {
        const char* T = "std.ffi.OsString";
        ADD_FUNC(T, "new",          "std.ffi.OsString.new",         lsm_type_unknown());
        ADD_FUNC(T, "from",         "std.ffi.OsString.from",        lsm_type_unknown());
        ADD_FUNC(T, "with_capacity","std.ffi.OsString.with_capacity",lsm_type_unknown());
        ADD_FUNC(T, "as_os_str",    "std.ffi.OsString.as_os_str",   lsm_type_unknown());
        ADD_FUNC(T, "into_string",  "std.ffi.OsString.into_string", lsm_type_unknown());
        ADD_FUNC(T, "push",         "std.ffi.OsString.push",        t_unit);
        ADD_FUNC(T, "len",          "std.ffi.OsString.len",         t_usize);
        ADD_FUNC(T, "clear",        "std.ffi.OsString.clear",       t_unit);
        ADD_FUNC(T, "capacity",     "std.ffi.OsString.capacity",    t_usize);
    }
    {
        const char* T = "std.ffi.OsStr";
        ADD_FUNC(T, "new",          "std.ffi.OsStr.new",            lsm_type_unknown());
        ADD_FUNC(T, "to_str",       "std.ffi.OsStr.to_str",         lsm_type_unknown());
        ADD_FUNC(T, "to_string_lossy","std.ffi.OsStr.to_string_lossy", lsm_type_unknown());
        ADD_FUNC(T, "to_os_string", "std.ffi.OsStr.to_os_string",   lsm_type_unknown());
        ADD_FUNC(T, "is_empty",     "std.ffi.OsStr.is_empty",       t_bool);
        ADD_FUNC(T, "len",          "std.ffi.OsStr.len",            t_usize);
    }

    /* ── std::ptr / core::ptr ─────────────────────────────────── */
    ADD_TYPE("core.ptr.NonNull",  "NonNull",  false);
    ADD_FUNC(NULL, "null",         "core.ptr.null",         lsm_type_unknown());
    ADD_FUNC(NULL, "null_mut",     "core.ptr.null_mut",     lsm_type_unknown());
    ADD_FUNC(NULL, "addr_of",      "core.ptr.addr_of",      lsm_type_unknown());
    ADD_FUNC(NULL, "addr_of_mut",  "core.ptr.addr_of_mut",  lsm_type_unknown());
    ADD_FUNC(NULL, "read",         "core.ptr.read",         lsm_type_unknown());
    ADD_FUNC(NULL, "write",        "core.ptr.write",        t_unit);
    ADD_FUNC(NULL, "copy",         "core.ptr.copy",         t_unit);
    ADD_FUNC(NULL, "copy_nonoverlapping","core.ptr.copy_nonoverlapping", t_unit);
    ADD_FUNC(NULL, "swap",         "core.ptr.swap",         t_unit);
    ADD_FUNC(NULL, "drop_in_place","core.ptr.drop_in_place",t_unit);
    /* std::ptr re-export aliases for free functions don't fit easily —
     * the resolver finds them via use map already. */
    {
        const char* T = "core.ptr.NonNull";
        ADD_FUNC(T, "new",          "core.ptr.NonNull.new",         lsm_type_unknown());
        ADD_FUNC(T, "new_unchecked","core.ptr.NonNull.new_unchecked", lsm_type_unknown());
        ADD_FUNC(T, "as_ptr",       "core.ptr.NonNull.as_ptr",      lsm_type_unknown());
        ADD_FUNC(T, "as_ref",       "core.ptr.NonNull.as_ref",      lsm_type_unknown());
        ADD_FUNC(T, "as_mut",       "core.ptr.NonNull.as_mut",      lsm_type_unknown());
        ADD_FUNC(T, "cast",         "core.ptr.NonNull.cast",        lsm_type_unknown());
    }

    /* ── std::mem extras ──────────────────────────────────────── */
    ADD_FUNC(NULL, "align_of",     "std.mem.align_of",     t_usize);
    ADD_FUNC(NULL, "align_of_val", "std.mem.align_of_val", t_usize);
    ADD_FUNC(NULL, "size_of_val",  "std.mem.size_of_val",  t_usize);
    ADD_FUNC(NULL, "transmute",    "std.mem.transmute",    lsm_type_unknown());
    ADD_FUNC(NULL, "transmute_copy","std.mem.transmute_copy", lsm_type_unknown());
    ADD_FUNC(NULL, "uninitialized","std.mem.uninitialized",lsm_type_unknown());
    ADD_FUNC(NULL, "zeroed",       "std.mem.zeroed",       lsm_type_unknown());
    ADD_FUNC(NULL, "forget",       "std.mem.forget",       t_unit);
    ADD_FUNC(NULL, "discriminant", "std.mem.discriminant", lsm_type_unknown());
    ADD_FUNC(NULL, "needs_drop",   "std.mem.needs_drop",   t_bool);

    /* ── core::slice extras (already partial) ────────────────── */
    {
        const char* T = "core.slice";
        ADD_FUNC(T, "split_at_mut", "core.slice.split_at_mut", lsm_type_unknown());
        ADD_FUNC(T, "split_first_mut","core.slice.split_first_mut", lsm_type_unknown());
        ADD_FUNC(T, "split_last_mut","core.slice.split_last_mut", lsm_type_unknown());
        ADD_FUNC(T, "split_at_unchecked","core.slice.split_at_unchecked", lsm_type_unknown());
        ADD_FUNC(T, "windows_mut",  "core.slice.windows_mut",  lsm_type_unknown());
        ADD_FUNC(T, "chunks_mut",   "core.slice.chunks_mut",   lsm_type_unknown());
        ADD_FUNC(T, "chunks_exact_mut","core.slice.chunks_exact_mut", lsm_type_unknown());
        ADD_FUNC(T, "rchunks",      "core.slice.rchunks",      lsm_type_unknown());
        ADD_FUNC(T, "rchunks_mut",  "core.slice.rchunks_mut",  lsm_type_unknown());
        ADD_FUNC(T, "binary_search_by","core.slice.binary_search_by", lsm_type_unknown());
        ADD_FUNC(T, "binary_search_by_key","core.slice.binary_search_by_key", lsm_type_unknown());
        ADD_FUNC(T, "select_nth_unstable","core.slice.select_nth_unstable", lsm_type_unknown());
        ADD_FUNC(T, "partition_point","core.slice.partition_point", t_usize);
        ADD_FUNC(T, "get_mut",      "core.slice.get_mut",      lsm_type_unknown());
        ADD_FUNC(T, "first_mut",    "core.slice.first_mut",    lsm_type_unknown());
        ADD_FUNC(T, "last_mut",     "core.slice.last_mut",     lsm_type_unknown());
        ADD_FUNC(T, "iter_chunks",  "core.slice.iter_chunks",  lsm_type_unknown());
        ADD_FUNC(T, "concat_str",   "core.slice.concat_str",   lsm_type_unknown());
        ADD_FUNC(T, "is_sorted",    "core.slice.is_sorted",    t_bool);
        ADD_FUNC(T, "is_sorted_by", "core.slice.is_sorted_by", t_bool);
        ADD_FUNC(T, "split_off",    "core.slice.split_off",    lsm_type_unknown());
        ADD_FUNC(T, "as_chunks",    "core.slice.as_chunks",    lsm_type_unknown());
    }

    /* ── core::str extras ─────────────────────────────────────── */
    {
        const char* T = "core.str";
        ADD_FUNC(T, "as_ptr",       "core.str.as_ptr",       lsm_type_unknown());
        ADD_FUNC(T, "as_str",       "core.str.as_str",       t_str_ref);
        ADD_FUNC(T, "from_utf8",    "core.str.from_utf8",    lsm_type_unknown());
        ADD_FUNC(T, "from_utf8_unchecked","core.str.from_utf8_unchecked", lsm_type_unknown());
        ADD_FUNC(T, "encode_utf16_lossy","core.str.encode_utf16_lossy", lsm_type_unknown());
        ADD_FUNC(T, "into_string",  "core.str.into_string",  t_string);
    }

    /* ── alloc::string extras ─────────────────────────────────── */
    {
        const char* T = "alloc.string.String";
        ADD_FUNC(T, "from_utf8",    "alloc.string.String.from_utf8",     lsm_type_unknown());
        ADD_FUNC(T, "from_utf8_lossy","alloc.string.String.from_utf8_lossy", lsm_type_unknown());
        ADD_FUNC(T, "from_utf16",   "alloc.string.String.from_utf16",    lsm_type_unknown());
        ADD_FUNC(T, "from_utf16_lossy","alloc.string.String.from_utf16_lossy", lsm_type_unknown());
        ADD_FUNC(T, "into_boxed_str","alloc.string.String.into_boxed_str", lsm_type_unknown());
        ADD_FUNC(T, "leak",         "alloc.string.String.leak",          lsm_type_unknown());
        ADD_FUNC(T, "shrink_to_fit","alloc.string.String.shrink_to_fit", t_unit);
        ADD_FUNC(T, "shrink_to",    "alloc.string.String.shrink_to",     t_unit);
        ADD_FUNC(T, "reserve",      "alloc.string.String.reserve",       t_unit);
        ADD_FUNC(T, "reserve_exact","alloc.string.String.reserve_exact", t_unit);
        ADD_FUNC(T, "drain",        "alloc.string.String.drain",         lsm_type_unknown());
        ADD_FUNC(T, "insert",       "alloc.string.String.insert",        t_unit);
        ADD_FUNC(T, "insert_str",   "alloc.string.String.insert_str",    t_unit);
        ADD_FUNC(T, "remove",       "alloc.string.String.remove",        lsm_type_unknown());
        ADD_FUNC(T, "retain",       "alloc.string.String.retain",        t_unit);
        ADD_FUNC(T, "truncate",     "alloc.string.String.truncate",      t_unit);
        ADD_FUNC(T, "split_off",    "alloc.string.String.split_off",     t_string);
        ADD_FUNC(T, "as_mut_str",   "alloc.string.String.as_mut_str",    lsm_type_unknown());
    }

    /* ── alloc::vec extras ────────────────────────────────────── */
    {
        const char* T = "alloc.vec.Vec";
        ADD_FUNC(T, "shrink_to_fit","alloc.vec.Vec.shrink_to_fit",   t_unit);
        ADD_FUNC(T, "shrink_to",    "alloc.vec.Vec.shrink_to",       t_unit);
        ADD_FUNC(T, "truncate",     "alloc.vec.Vec.truncate",        t_unit);
        ADD_FUNC(T, "resize",       "alloc.vec.Vec.resize",          t_unit);
        ADD_FUNC(T, "resize_with",  "alloc.vec.Vec.resize_with",     t_unit);
        ADD_FUNC(T, "retain",       "alloc.vec.Vec.retain",          t_unit);
        ADD_FUNC(T, "retain_mut",   "alloc.vec.Vec.retain_mut",      t_unit);
        ADD_FUNC(T, "dedup",        "alloc.vec.Vec.dedup",           t_unit);
        ADD_FUNC(T, "dedup_by",     "alloc.vec.Vec.dedup_by",        t_unit);
        ADD_FUNC(T, "dedup_by_key", "alloc.vec.Vec.dedup_by_key",    t_unit);
        ADD_FUNC(T, "split_off",    "alloc.vec.Vec.split_off",       lsm_type_unknown());
        ADD_FUNC(T, "append",       "alloc.vec.Vec.append",          t_unit);
        ADD_FUNC(T, "leak",         "alloc.vec.Vec.leak",            lsm_type_unknown());
        ADD_FUNC(T, "spare_capacity_mut","alloc.vec.Vec.spare_capacity_mut", lsm_type_unknown());
        ADD_FUNC(T, "into_boxed_slice","alloc.vec.Vec.into_boxed_slice", lsm_type_unknown());
        ADD_FUNC(T, "set_len",      "alloc.vec.Vec.set_len",         t_unit);
        ADD_FUNC(T, "split_at_mut", "alloc.vec.Vec.split_at_mut",    lsm_type_unknown());
        ADD_FUNC(T, "swap",         "alloc.vec.Vec.swap",            t_unit);
        ADD_FUNC(T, "rotate_left",  "alloc.vec.Vec.rotate_left",     t_unit);
        ADD_FUNC(T, "rotate_right", "alloc.vec.Vec.rotate_right",    t_unit);
        ADD_FUNC(T, "fill",         "alloc.vec.Vec.fill",            t_unit);
        ADD_FUNC(T, "fill_with",    "alloc.vec.Vec.fill_with",       t_unit);
    }

    /* ── alloc::collections::HashMap extras ───────────────────── */
    {
        const char* T = "alloc.collections.HashMap";
        ADD_FUNC(T, "drain",        "alloc.collections.HashMap.drain",        lsm_type_unknown());
        ADD_FUNC(T, "retain",       "alloc.collections.HashMap.retain",       t_unit);
        ADD_FUNC(T, "extend",       "alloc.collections.HashMap.extend",       t_unit);
        ADD_FUNC(T, "values_mut",   "alloc.collections.HashMap.values_mut",   lsm_type_unknown());
        ADD_FUNC(T, "iter_mut",     "alloc.collections.HashMap.iter_mut",     lsm_type_unknown());
        ADD_FUNC(T, "into_iter",    "alloc.collections.HashMap.into_iter",    lsm_type_unknown());
        ADD_FUNC(T, "into_keys",    "alloc.collections.HashMap.into_keys",    lsm_type_unknown());
        ADD_FUNC(T, "into_values",  "alloc.collections.HashMap.into_values",  lsm_type_unknown());
        ADD_FUNC(T, "shrink_to_fit","alloc.collections.HashMap.shrink_to_fit",t_unit);
        ADD_FUNC(T, "reserve",      "alloc.collections.HashMap.reserve",      t_unit);
        ADD_FUNC(T, "capacity",     "alloc.collections.HashMap.capacity",     t_usize);
        ADD_FUNC(T, "hasher",       "alloc.collections.HashMap.hasher",       lsm_type_unknown());
        ADD_FUNC(T, "raw_entry",    "alloc.collections.HashMap.raw_entry",    lsm_type_unknown());
    }

    /* ── std::sync extras ─────────────────────────────────────── */
    ADD_TYPE("std.sync.PoisonError", "PoisonError", false);
    ADD_TYPE("std.sync.WaitTimeoutResult","WaitTimeoutResult", false);
    {
        const char* T = "std.sync.Condvar";
        ADD_FUNC(T, "new",         "std.sync.Condvar.new",         lsm_type_unknown());
        ADD_FUNC(T, "wait",        "std.sync.Condvar.wait",        lsm_type_unknown());
        ADD_FUNC(T, "wait_timeout","std.sync.Condvar.wait_timeout",lsm_type_unknown());
        ADD_FUNC(T, "wait_while",  "std.sync.Condvar.wait_while",  lsm_type_unknown());
        ADD_FUNC(T, "notify_one",  "std.sync.Condvar.notify_one",  t_unit);
        ADD_FUNC(T, "notify_all",  "std.sync.Condvar.notify_all",  t_unit);
    }
    {
        const char* T = "std.sync.Barrier";
        ADD_FUNC(T, "new",         "std.sync.Barrier.new",         lsm_type_unknown());
        ADD_FUNC(T, "wait",        "std.sync.Barrier.wait",        lsm_type_unknown());
    }
    {
        const char* T = "std.sync.MutexGuard";
        ADD_FUNC(T, "deref",       "std.sync.MutexGuard.deref",       lsm_type_unknown());
        ADD_FUNC(T, "deref_mut",   "std.sync.MutexGuard.deref_mut",   lsm_type_unknown());
    }
    {
        const char* T = "std.sync.RwLockReadGuard";
        ADD_FUNC(T, "deref",       "std.sync.RwLockReadGuard.deref",  lsm_type_unknown());
    }
    {
        const char* T = "std.sync.RwLockWriteGuard";
        ADD_FUNC(T, "deref",       "std.sync.RwLockWriteGuard.deref", lsm_type_unknown());
        ADD_FUNC(T, "deref_mut",   "std.sync.RwLockWriteGuard.deref_mut", lsm_type_unknown());
    }

    /* ── core::cell extras ────────────────────────────────────── */
    {
        const char* T = "core.cell.Cell";
        ADD_FUNC(T, "get",         "core.cell.Cell.get",          lsm_type_unknown());
        ADD_FUNC(T, "set",         "core.cell.Cell.set",          t_unit);
        ADD_FUNC(T, "replace",     "core.cell.Cell.replace",      lsm_type_unknown());
        ADD_FUNC(T, "swap",        "core.cell.Cell.swap",         t_unit);
        ADD_FUNC(T, "into_inner",  "core.cell.Cell.into_inner",   lsm_type_unknown());
        ADD_FUNC(T, "as_ptr",      "core.cell.Cell.as_ptr",       lsm_type_unknown());
        ADD_FUNC(T, "take",        "core.cell.Cell.take",         lsm_type_unknown());
        ADD_FUNC(T, "update",      "core.cell.Cell.update",       lsm_type_unknown());
    }
    {
        const char* T = "core.cell.RefCell";
        ADD_FUNC(T, "try_borrow",  "core.cell.RefCell.try_borrow",lsm_type_unknown());
        ADD_FUNC(T, "try_borrow_mut","core.cell.RefCell.try_borrow_mut", lsm_type_unknown());
        ADD_FUNC(T, "into_inner",  "core.cell.RefCell.into_inner",lsm_type_unknown());
        ADD_FUNC(T, "replace",     "core.cell.RefCell.replace",   lsm_type_unknown());
        ADD_FUNC(T, "replace_with","core.cell.RefCell.replace_with", lsm_type_unknown());
        ADD_FUNC(T, "swap",        "core.cell.RefCell.swap",      t_unit);
        ADD_FUNC(T, "take",        "core.cell.RefCell.take",      lsm_type_unknown());
        ADD_FUNC(T, "as_ptr",      "core.cell.RefCell.as_ptr",    lsm_type_unknown());
    }

    /* ── core::ops::Range additions ──────────────────────────── */
    {
        const char* T = "core.ops.Range";
        ADD_FUNC(T, "into_iter",   "core.ops.Range.into_iter",    lsm_type_unknown());
        ADD_FUNC(T, "step_by",     "core.ops.Range.step_by",      lsm_type_unknown());
        ADD_FUNC(T, "rev",         "core.ops.Range.rev",          lsm_type_unknown());
        ADD_FUNC(T, "map",         "core.ops.Range.map",          lsm_type_unknown());
        ADD_FUNC(T, "filter",      "core.ops.Range.filter",       lsm_type_unknown());
        ADD_FUNC(T, "fold",        "core.ops.Range.fold",         lsm_type_unknown());
        ADD_FUNC(T, "for_each",    "core.ops.Range.for_each",     t_unit);
        ADD_FUNC(T, "count",       "core.ops.Range.count",        t_usize);
        ADD_FUNC(T, "sum",         "core.ops.Range.sum",          lsm_type_unknown());
        ADD_FUNC(T, "product",     "core.ops.Range.product",      lsm_type_unknown());
        ADD_FUNC(T, "collect",     "core.ops.Range.collect",      lsm_type_unknown());
        ADD_FUNC(T, "next",        "core.ops.Range.next",         lsm_type_unknown());
    }
    {
        const char* T = "core.ops.RangeInclusive";
        ADD_FUNC(T, "new",         "core.ops.RangeInclusive.new", lsm_type_unknown());
        ADD_FUNC(T, "contains",    "core.ops.RangeInclusive.contains", t_bool);
        ADD_FUNC(T, "start",       "core.ops.RangeInclusive.start", lsm_type_unknown());
        ADD_FUNC(T, "end",         "core.ops.RangeInclusive.end", lsm_type_unknown());
        ADD_FUNC(T, "is_empty",    "core.ops.RangeInclusive.is_empty", t_bool);
    }

    /* ── std::process extras ──────────────────────────────────── */
    {
        const char* T = "std.process.Command";
        ADD_FUNC(T, "args_owned",  "std.process.Command.args_owned",  lsm_type_unknown());
        ADD_FUNC(T, "get_args",    "std.process.Command.get_args",    lsm_type_unknown());
        ADD_FUNC(T, "get_envs",    "std.process.Command.get_envs",    lsm_type_unknown());
        ADD_FUNC(T, "get_current_dir","std.process.Command.get_current_dir", lsm_type_unknown());
        ADD_FUNC(T, "get_program", "std.process.Command.get_program", lsm_type_unknown());
    }

    /* ── std::time extras ─────────────────────────────────────── */
    {
        const char* T = "std.time.Duration";
        ADD_FUNC(T, "as_secs_f32", "std.time.Duration.as_secs_f32",   lsm_type_unknown());
        ADD_FUNC(T, "from_secs_f32","std.time.Duration.from_secs_f32",lsm_type_unknown());
        ADD_FUNC(T, "div_duration_f32","std.time.Duration.div_duration_f32", lsm_type_unknown());
        ADD_FUNC(T, "div_duration_f64","std.time.Duration.div_duration_f64", lsm_type_unknown());
        ADD_FUNC(T, "mul_f32",     "std.time.Duration.mul_f32",       lsm_type_unknown());
        ADD_FUNC(T, "mul_f64",     "std.time.Duration.mul_f64",       lsm_type_unknown());
        ADD_FUNC(T, "div_f32",     "std.time.Duration.div_f32",       lsm_type_unknown());
        ADD_FUNC(T, "div_f64",     "std.time.Duration.div_f64",       lsm_type_unknown());
        ADD_FUNC(T, "max",         "std.time.Duration.max",           lsm_type_unknown());
        ADD_FUNC(T, "min",         "std.time.Duration.min",           lsm_type_unknown());
        ADD_FUNC(T, "abs_diff",    "std.time.Duration.abs_diff",      lsm_type_unknown());
    }

    /* ── std::thread extras ──────────────────────────────────── */
    {
        const char* T = "std.thread.Builder";
        ADD_FUNC(T, "spawn_scoped","std.thread.Builder.spawn_scoped", lsm_type_unknown());
    }
    ADD_TYPE("std.thread.ThreadId", "ThreadId", false);
    ADD_TYPE("std.thread.LocalKey", "LocalKey", false);
    ADD_TYPE("std.thread.AccessError","AccessError", false);
    {
        const char* T = "std.thread.LocalKey";
        ADD_FUNC(T, "with",        "std.thread.LocalKey.with",        lsm_type_unknown());
        ADD_FUNC(T, "try_with",    "std.thread.LocalKey.try_with",    lsm_type_unknown());
    }

    /* ── std::collections::BinaryHeap extras ─────────────────── */
    {
        const char* T = "alloc.collections.BinaryHeap";
        ADD_FUNC(T, "is_empty",    "alloc.collections.BinaryHeap.is_empty",   t_bool);
        ADD_FUNC(T, "iter",        "alloc.collections.BinaryHeap.iter",       lsm_type_unknown());
        ADD_FUNC(T, "drain",       "alloc.collections.BinaryHeap.drain",      lsm_type_unknown());
        ADD_FUNC(T, "drain_sorted","alloc.collections.BinaryHeap.drain_sorted", lsm_type_unknown());
        ADD_FUNC(T, "clear",       "alloc.collections.BinaryHeap.clear",      t_unit);
        ADD_FUNC(T, "capacity",    "alloc.collections.BinaryHeap.capacity",   t_usize);
        ADD_FUNC(T, "shrink_to_fit","alloc.collections.BinaryHeap.shrink_to_fit", t_unit);
        ADD_FUNC(T, "reserve",     "alloc.collections.BinaryHeap.reserve",    t_unit);
    }

    /* ── core::iter free helpers + Iterator extras ──────────── */
    ADD_FUNC(NULL, "from_iter",    "core.iter.FromIterator.from_iter", lsm_type_unknown());
    {
        const char* T = "core.iter.Iterator";
        ADD_FUNC(T, "size_hint",   "core.iter.Iterator.size_hint",    lsm_type_unknown());
        ADD_FUNC(T, "advance_by",  "core.iter.Iterator.advance_by",   lsm_type_unknown());
        ADD_FUNC(T, "intersperse", "core.iter.Iterator.intersperse",  lsm_type_unknown());
        ADD_FUNC(T, "tuple_windows","core.iter.Iterator.tuple_windows", lsm_type_unknown());
        ADD_FUNC(T, "windows",     "core.iter.Iterator.windows",      lsm_type_unknown());
        ADD_FUNC(T, "is_partitioned","core.iter.Iterator.is_partitioned", t_bool);
        ADD_FUNC(T, "cmp_by",      "core.iter.Iterator.cmp_by",       lsm_type_unknown());
        ADD_FUNC(T, "partial_cmp", "core.iter.Iterator.partial_cmp",  lsm_type_unknown());
        ADD_FUNC(T, "eq",          "core.iter.Iterator.eq",           t_bool);
        ADD_FUNC(T, "ne",          "core.iter.Iterator.ne",           t_bool);
        ADD_FUNC(T, "lt",          "core.iter.Iterator.lt",           t_bool);
    }

    /* ── alloc::boxed::Box extras ────────────────────────────── */
    {
        const char* T = "alloc.boxed.Box";
        ADD_FUNC(T, "leak",        "alloc.boxed.Box.leak",            lsm_type_unknown());
        ADD_FUNC(T, "into_pin",    "alloc.boxed.Box.into_pin",        lsm_type_unknown());
        ADD_FUNC(T, "from_raw",    "alloc.boxed.Box.from_raw",        lsm_type_unknown());
        ADD_FUNC(T, "into_raw",    "alloc.boxed.Box.into_raw",        lsm_type_unknown());
        ADD_FUNC(T, "downcast",    "alloc.boxed.Box.downcast",        lsm_type_unknown());
    }

    /* ── alloc::rc::Rc / sync::Arc extras ────────────────────── */
    {
        const char* T = "alloc.rc.Rc";
        ADD_FUNC(T, "weak_count",  "alloc.rc.Rc.weak_count",          t_usize);
        ADD_FUNC(T, "downgrade",   "alloc.rc.Rc.downgrade",           lsm_type_unknown());
        ADD_FUNC(T, "get_mut",     "alloc.rc.Rc.get_mut",             lsm_type_unknown());
        ADD_FUNC(T, "into_raw",    "alloc.rc.Rc.into_raw",            lsm_type_unknown());
        ADD_FUNC(T, "from_raw",    "alloc.rc.Rc.from_raw",            lsm_type_unknown());
        ADD_FUNC(T, "ptr_eq",      "alloc.rc.Rc.ptr_eq",              t_bool);
        ADD_FUNC(T, "make_mut",    "alloc.rc.Rc.make_mut",            lsm_type_unknown());
    }
    {
        const char* T = "alloc.sync.Arc";
        ADD_FUNC(T, "weak_count",  "alloc.sync.Arc.weak_count",       t_usize);
        ADD_FUNC(T, "downgrade",   "alloc.sync.Arc.downgrade",        lsm_type_unknown());
        ADD_FUNC(T, "get_mut",     "alloc.sync.Arc.get_mut",          lsm_type_unknown());
        ADD_FUNC(T, "into_raw",    "alloc.sync.Arc.into_raw",         lsm_type_unknown());
        ADD_FUNC(T, "from_raw",    "alloc.sync.Arc.from_raw",         lsm_type_unknown());
        ADD_FUNC(T, "ptr_eq",      "alloc.sync.Arc.ptr_eq",           t_bool);
        ADD_FUNC(T, "make_mut",    "alloc.sync.Arc.make_mut",         lsm_type_unknown());
        ADD_FUNC(T, "try_unwrap",  "alloc.sync.Arc.try_unwrap",       lsm_type_unknown());
    }

    /* ── HashSet extras ───────────────────────────────────────── */
    {
        const char* T = "alloc.collections.HashSet";
        ADD_FUNC(T, "extend",         "alloc.collections.HashSet.extend",         t_unit);
        ADD_FUNC(T, "drain",          "alloc.collections.HashSet.drain",          lsm_type_unknown());
        ADD_FUNC(T, "into_iter",      "alloc.collections.HashSet.into_iter",      lsm_type_unknown());
        ADD_FUNC(T, "is_subset",      "alloc.collections.HashSet.is_subset",      t_bool);
        ADD_FUNC(T, "is_superset",    "alloc.collections.HashSet.is_superset",    t_bool);
        ADD_FUNC(T, "is_disjoint",    "alloc.collections.HashSet.is_disjoint",    t_bool);
        ADD_FUNC(T, "symmetric_difference","alloc.collections.HashSet.symmetric_difference", lsm_type_unknown());
        ADD_FUNC(T, "shrink_to_fit",  "alloc.collections.HashSet.shrink_to_fit",  t_unit);
        ADD_FUNC(T, "reserve",        "alloc.collections.HashSet.reserve",        t_unit);
        ADD_FUNC(T, "capacity",       "alloc.collections.HashSet.capacity",       t_usize);
        ADD_FUNC(T, "hasher",         "alloc.collections.HashSet.hasher",         lsm_type_unknown());
    }

    /* ── BTreeSet extras ──────────────────────────────────────── */
    {
        const char* T = "alloc.collections.BTreeSet";
        ADD_FUNC(T, "new",            "alloc.collections.BTreeSet.new",            lsm_type_unknown());
        ADD_FUNC(T, "insert",         "alloc.collections.BTreeSet.insert",         t_bool);
        ADD_FUNC(T, "remove",         "alloc.collections.BTreeSet.remove",         t_bool);
        ADD_FUNC(T, "contains",       "alloc.collections.BTreeSet.contains",       t_bool);
        ADD_FUNC(T, "len",            "alloc.collections.BTreeSet.len",            t_usize);
        ADD_FUNC(T, "is_empty",       "alloc.collections.BTreeSet.is_empty",       t_bool);
        ADD_FUNC(T, "iter",           "alloc.collections.BTreeSet.iter",           lsm_type_unknown());
        ADD_FUNC(T, "range",          "alloc.collections.BTreeSet.range",          lsm_type_unknown());
        ADD_FUNC(T, "first",          "alloc.collections.BTreeSet.first",          lsm_type_unknown());
        ADD_FUNC(T, "last",           "alloc.collections.BTreeSet.last",           lsm_type_unknown());
    }

    /* ── std::path extras ─────────────────────────────────────── */
    {
        const char* T = "std.path.PathBuf";
        ADD_FUNC(T, "with_capacity", "std.path.PathBuf.with_capacity", lsm_type_unknown());
        ADD_FUNC(T, "capacity",      "std.path.PathBuf.capacity",      t_usize);
        ADD_FUNC(T, "as_mut_os_string","std.path.PathBuf.as_mut_os_string", lsm_type_unknown());
    }
    {
        const char* T = "std.path.Path";
        ADD_FUNC(T, "as_os_str",     "std.path.Path.as_os_str",        lsm_type_unknown());
        ADD_FUNC(T, "ancestors",     "std.path.Path.ancestors",        lsm_type_unknown());
        ADD_FUNC(T, "strip_prefix",  "std.path.Path.strip_prefix",     lsm_type_unknown());
        ADD_FUNC(T, "is_normalized", "std.path.Path.is_normalized",    t_bool);
    }

    /* ── std::io extras ───────────────────────────────────────── */
    {
        const char* T = "std.io.BufReader";
        ADD_FUNC(T, "fill_buf",     "std.io.BufReader.fill_buf",      lsm_type_unknown());
        ADD_FUNC(T, "consume",      "std.io.BufReader.consume",       t_unit);
        ADD_FUNC(T, "lines",        "std.io.BufReader.lines",         lsm_type_unknown());
        ADD_FUNC(T, "read_line",    "std.io.BufReader.read_line",     lsm_type_unknown());
    }
    {
        const char* T = "std.io.Stdin";
        ADD_FUNC(T, "lock",         "std.io.Stdin.lock",              lsm_type_unknown());
        ADD_FUNC(T, "lines",        "std.io.Stdin.lines",             lsm_type_unknown());
        ADD_FUNC(T, "read_line",    "std.io.Stdin.read_line",         lsm_type_unknown());
    }
    {
        const char* T = "std.io.Stdout";
        ADD_FUNC(T, "lock",         "std.io.Stdout.lock",             lsm_type_unknown());
        ADD_FUNC(T, "flush",        "std.io.Stdout.flush",            lsm_type_unknown());
    }
    {
        const char* T = "std.io.StdoutLock";
        ADD_FUNC(T, "write",        "std.io.StdoutLock.write",        lsm_type_unknown());
        ADD_FUNC(T, "write_all",    "std.io.StdoutLock.write_all",    lsm_type_unknown());
    }

    /* ── core::fmt::Formatter ─────────────────────────────────── */
    ADD_TYPE("core.fmt.Formatter",   "Formatter",   false);
    ADD_TYPE("core.fmt.Result",      "Result",      false);
    ADD_TYPE("core.fmt.Error",       "Error",       false);
    ADD_TYPE("core.fmt.Arguments",   "Arguments",   false);
    {
        const char* T = "core.fmt.Formatter";
        ADD_FUNC(T, "write_str",    "core.fmt.Formatter.write_str",   lsm_type_unknown());
        ADD_FUNC(T, "write_fmt",    "core.fmt.Formatter.write_fmt",   lsm_type_unknown());
        ADD_FUNC(T, "pad",          "core.fmt.Formatter.pad",         lsm_type_unknown());
        ADD_FUNC(T, "fill",         "core.fmt.Formatter.fill",        lsm_type_unknown());
        ADD_FUNC(T, "align",        "core.fmt.Formatter.align",       lsm_type_unknown());
        ADD_FUNC(T, "width",        "core.fmt.Formatter.width",       lsm_type_unknown());
        ADD_FUNC(T, "precision",    "core.fmt.Formatter.precision",   lsm_type_unknown());
        ADD_FUNC(T, "debug_struct", "core.fmt.Formatter.debug_struct",lsm_type_unknown());
        ADD_FUNC(T, "debug_tuple",  "core.fmt.Formatter.debug_tuple", lsm_type_unknown());
        ADD_FUNC(T, "debug_list",   "core.fmt.Formatter.debug_list",  lsm_type_unknown());
        ADD_FUNC(T, "debug_set",    "core.fmt.Formatter.debug_set",   lsm_type_unknown());
        ADD_FUNC(T, "debug_map",    "core.fmt.Formatter.debug_map",   lsm_type_unknown());
    }

    /* Common-crate seeds (serde/tokio/anyhow/clap/regex/log/futures/
     * parking_lot/once_cell/chrono/uuid/reqwest/rayon). */
    lsm_rust_crates_register(reg, arena);

    /* Finalise: build hash buckets so per-file lookups are O(1)
     * instead of O(n) over the ~2.5k registered entries. */
    lsm_registry_finalize(reg);
}
