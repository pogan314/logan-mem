#include "go_lsp.h"
#include "lsp_node_iter.h"
#include "../helpers.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations
static void resolve_calls_in_node_inner(GoLSPContext* ctx, TSNode node);

/* Depth-guarded entry for the AST call-resolution walk. The walk recurses once
 * per nesting level; a deeply-nested or cyclic file can overflow the native
 * stack (SIGSEGV) and take down the whole index. Past the cap the subtree is
 * skipped — its calls stay unresolved, which is graceful degradation, not a
 * crash. The cap is LSM_LSP_MAX_WALK_DEPTH, env-overridable via the same name.
 * The walk_depth-- runs after the inner returns, so early returns in the body
 * never leak the counter. */
static void resolve_calls_in_node(GoLSPContext* ctx, TSNode node) {
    if (ctx->walk_depth >= lsm_lsp_max_walk_depth())
        return;
    ctx->walk_depth++;
    resolve_calls_in_node_inner(ctx, node);
    ctx->walk_depth--;
}
static void emit_resolved_call(GoLSPContext *ctx, const char *callee_qn, const char *strategy,
                               float confidence, TSNode site);
static const char *go_exact_callable_target(GoLSPContext *ctx, TSNode node);
static const LSMType* go_lookup_field(GoLSPContext* ctx, const char* type_qn, const char* field_name, int depth);
static void extract_type_params_from_ast(LSMArena* arena, LSMTypeRegistry* reg,
    TSNode root, const char* source, const char* module_qn);

// --- Initialization ---

void go_lsp_init(GoLSPContext* ctx, LSMArena* arena, const char* source, int source_len,
    const LSMTypeRegistry* registry, const char* package_qn, LSMResolvedCallArray* out) {
    memset(ctx, 0, sizeof(GoLSPContext));
    ctx->arena = arena;
    ctx->source = source;
    ctx->source_len = source_len;
    ctx->registry = registry;
    ctx->package_qn = package_qn;
    ctx->resolved_calls = out;
    ctx->current_scope = lsm_scope_push(arena, NULL); // root scope

    {
        const char* debug_env = getenv("LSM_LSP_DEBUG");
        ctx->debug = (debug_env && debug_env[0]);
    }
}

void go_lsp_add_import(GoLSPContext* ctx, const char* local_name, const char* pkg_qn) {
    // Store in parallel arrays (arena-allocated, grow by doubling)
    if (ctx->import_count % 32 == 0) {
        int new_cap = ctx->import_count + 32;
        const char** new_names = (const char**)lsm_arena_alloc(ctx->arena, (new_cap + 1) * sizeof(const char*));
        const char** new_qns = (const char**)lsm_arena_alloc(ctx->arena, (new_cap + 1) * sizeof(const char*));
        if (!new_names || !new_qns) return;
        if (ctx->import_local_names && ctx->import_count > 0) {
            memcpy(new_names, ctx->import_local_names, ctx->import_count * sizeof(const char*));
            memcpy(new_qns, ctx->import_package_qns, ctx->import_count * sizeof(const char*));
        }
        ctx->import_local_names = new_names;
        ctx->import_package_qns = new_qns;
    }
    ctx->import_local_names[ctx->import_count] = lsm_arena_strdup(ctx->arena, local_name);
    ctx->import_package_qns[ctx->import_count] = lsm_arena_strdup(ctx->arena, pkg_qn);
    ctx->import_count++;
}

// --- Helper: get node text ---

static char* lsp_node_text(GoLSPContext* ctx, TSNode node) {
    return lsm_node_text(ctx->arena, node, ctx->source);
}

// --- Helper: resolve import alias to package QN ---

static const char* resolve_import(GoLSPContext* ctx, const char* local_name) {
    for (int i = 0; i < ctx->import_count; i++) {
        if (strcmp(ctx->import_local_names[i], local_name) == 0) {
            return ctx->import_package_qns[i];
        }
    }
    return NULL;
}

// --- Helper: check if name is a Go builtin ---

static bool is_go_builtin_func(const char* name) {
    static const char* builtins[] = {
        "make", "new", "append", "len", "cap", "delete",
        "close", "copy", "panic", "recover", "print", "println",
        "complex", "real", "imag", "min", "max", "clear",
        NULL
    };
    for (const char** b = builtins; *b; b++) {
        if (strcmp(name, *b) == 0) return true;
    }
    return false;
}

// --- Helper: check if name is a Go builtin type ---

static const LSMType* resolve_builtin_type(GoLSPContext* ctx, const char* name) {
    static const char* builtin_types[] = {
        "int", "int8", "int16", "int32", "int64",
        "uint", "uint8", "uint16", "uint32", "uint64",
        "float32", "float64", "complex64", "complex128",
        "string", "bool", "byte", "rune", "error",
        "uintptr", "any",
        NULL
    };
    for (const char** b = builtin_types; *b; b++) {
        if (strcmp(name, *b) == 0) {
            return lsm_type_builtin(ctx->arena, name);
        }
    }
    return NULL;
}

// --- go_parse_type_node: AST type node -> LSMType ---

const LSMType* go_parse_type_node(GoLSPContext* ctx, TSNode node) {
    if (ts_node_is_null(node)) return lsm_type_unknown();

    const char* kind = ts_node_type(node);

    // type_identifier: simple named type
    if (strcmp(kind, "type_identifier") == 0) {
        char* name = lsp_node_text(ctx, node);
        if (!name) return lsm_type_unknown();
        const LSMType* builtin = resolve_builtin_type(ctx, name);
        if (builtin) return builtin;
        // Resolve as local type: package_qn.TypeName
        return lsm_type_named(ctx->arena,
            lsm_arena_sprintf(ctx->arena, "%s.%s", ctx->package_qn, name));
    }

    // qualified_type: pkg.Type
    if (strcmp(kind, "qualified_type") == 0) {
        TSNode pkg_node = ts_node_child_by_field_name(node, "package", 7);
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(pkg_node) && !ts_node_is_null(name_node)) {
            char* pkg = lsp_node_text(ctx, pkg_node);
            char* name = lsp_node_text(ctx, name_node);
            const char* pkg_qn = resolve_import(ctx, pkg);
            if (pkg_qn) {
                return lsm_type_named(ctx->arena,
                    lsm_arena_sprintf(ctx->arena, "%s.%s", pkg_qn, name));
            }
        }
        return lsm_type_unknown();
    }

    // pointer_type: *T
    if (strcmp(kind, "pointer_type") == 0) {
        uint32_t nc = ts_node_named_child_count(node);
        if (nc > 0) {
            return lsm_type_pointer(ctx->arena,
                go_parse_type_node(ctx, ts_node_named_child(node, nc - 1)));
        }
        return lsm_type_unknown();
    }

    // slice_type: []T
    if (strcmp(kind, "slice_type") == 0) {
        TSNode elem = ts_node_child_by_field_name(node, "element", 7);
        if (ts_node_is_null(elem) && ts_node_named_child_count(node) > 0) {
            elem = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        }
        return lsm_type_slice(ctx->arena, go_parse_type_node(ctx, elem));
    }

    // array_type: [N]T — treat as slice for our purposes
    if (strcmp(kind, "array_type") == 0) {
        TSNode elem = ts_node_child_by_field_name(node, "element", 7);
        if (ts_node_is_null(elem) && ts_node_named_child_count(node) > 0) {
            elem = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        }
        return lsm_type_slice(ctx->arena, go_parse_type_node(ctx, elem));
    }

    // map_type: map[K]V
    if (strcmp(kind, "map_type") == 0) {
        TSNode key = ts_node_child_by_field_name(node, "key", 3);
        TSNode value = ts_node_child_by_field_name(node, "value", 5);
        return lsm_type_map(ctx->arena,
            go_parse_type_node(ctx, key),
            go_parse_type_node(ctx, value));
    }

    // channel_type: chan T
    if (strcmp(kind, "channel_type") == 0) {
        TSNode value = ts_node_child_by_field_name(node, "value", 5);
        if (ts_node_is_null(value) && ts_node_named_child_count(node) > 0) {
            value = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        }
        // Determine direction from text
        char* text = lsp_node_text(ctx, node);
        int dir = 0;
        if (text) {
            if (strncmp(text, "chan<-", 6) == 0 || strncmp(text, "chan <-", 7) == 0) dir = 1;
            else if (strncmp(text, "<-chan", 6) == 0 || strncmp(text, "<- chan", 7) == 0) dir = 2;
        }
        return lsm_type_channel(ctx->arena, go_parse_type_node(ctx, value), dir);
    }

    // function_type: func(...)...
    if (strcmp(kind, "function_type") == 0) {
        return lsm_type_func(ctx->arena, NULL, NULL, NULL); // simplified
    }

    // interface_type
    if (strcmp(kind, "interface_type") == 0) {
        LSMType* t = (LSMType*)lsm_arena_alloc(ctx->arena, sizeof(LSMType));
        memset(t, 0, sizeof(LSMType));
        t->kind = LSM_TYPE_INTERFACE;
        return t;
    }

    // struct_type
    if (strcmp(kind, "struct_type") == 0) {
        LSMType* t = (LSMType*)lsm_arena_alloc(ctx->arena, sizeof(LSMType));
        memset(t, 0, sizeof(LSMType));
        t->kind = LSM_TYPE_STRUCT;
        return t;
    }

    // parenthesized_type: (T)
    if (strcmp(kind, "parenthesized_type") == 0 && ts_node_named_child_count(node) > 0) {
        return go_parse_type_node(ctx, ts_node_named_child(node, 0));
    }

    // type_elem: wrapper in type_arguments, unwrap to inner type
    if (strcmp(kind, "type_elem") == 0 && ts_node_named_child_count(node) > 0) {
        return go_parse_type_node(ctx, ts_node_named_child(node, 0));
    }

    // generic_type: Type[T1, T2] — return as named without generic args for now
    if (strcmp(kind, "generic_type") == 0) {
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        if (!ts_node_is_null(type_node)) {
            return go_parse_type_node(ctx, type_node);
        }
    }

    // parameter_list used as result type (multi-return)
    if (strcmp(kind, "parameter_list") == 0) {
        int count = 0;
        const LSMType* elems[16];
        uint32_t nc = ts_node_child_count(node);
        for (uint32_t i = 0; i < nc && count < 16; i++) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
            const char* ck = ts_node_type(child);
            if (strcmp(ck, "parameter_declaration") == 0) {
                TSNode tn = ts_node_child_by_field_name(child, "type", 4);
                if (!ts_node_is_null(tn)) {
                    elems[count++] = go_parse_type_node(ctx, tn);
                }
            } else {
                elems[count++] = go_parse_type_node(ctx, child);
            }
        }
        if (count == 1) return elems[0];
        if (count > 1) return lsm_type_tuple(ctx->arena, elems, count);
    }

    return lsm_type_unknown();
}

// --- Implicit generics: type unification ---

// Unify a parameter type pattern (containing TYPE_PARAM) against a concrete argument type.
// Fills inferred[i] with the concrete type bound to type_param_names[i].
static void go_unify_type(const LSMType* param_type, const LSMType* arg_type,
    const char** type_param_names, const LSMType** inferred, int param_count) {
    if (!param_type || !arg_type || lsm_type_is_unknown(arg_type)) return;

    if (param_type->kind == LSM_TYPE_TYPE_PARAM) {
        for (int i = 0; i < param_count; i++) {
            if (strcmp(param_type->data.type_param.name, type_param_names[i]) == 0) {
                if (!inferred[i]) inferred[i] = arg_type;
                break;
            }
        }
        return;
    }

    // Structural matching — recurse into composite types
    if (param_type->kind == LSM_TYPE_SLICE && arg_type->kind == LSM_TYPE_SLICE) {
        go_unify_type(param_type->data.slice.elem, arg_type->data.slice.elem,
            type_param_names, inferred, param_count);
    }
    if (param_type->kind == LSM_TYPE_POINTER && arg_type->kind == LSM_TYPE_POINTER) {
        go_unify_type(param_type->data.pointer.elem, arg_type->data.pointer.elem,
            type_param_names, inferred, param_count);
    }
    if (param_type->kind == LSM_TYPE_MAP && arg_type->kind == LSM_TYPE_MAP) {
        go_unify_type(param_type->data.map.key, arg_type->data.map.key,
            type_param_names, inferred, param_count);
        go_unify_type(param_type->data.map.value, arg_type->data.map.value,
            type_param_names, inferred, param_count);
    }
    if (param_type->kind == LSM_TYPE_CHANNEL && arg_type->kind == LSM_TYPE_CHANNEL) {
        go_unify_type(param_type->data.channel.elem, arg_type->data.channel.elem,
            type_param_names, inferred, param_count);
    }
    if (param_type->kind == LSM_TYPE_FUNC && arg_type->kind == LSM_TYPE_FUNC) {
        // Match param types
        if (param_type->data.func.param_types && arg_type->data.func.param_types) {
            for (int i = 0; param_type->data.func.param_types[i] && arg_type->data.func.param_types[i]; i++) {
                go_unify_type(param_type->data.func.param_types[i], arg_type->data.func.param_types[i],
                    type_param_names, inferred, param_count);
            }
        }
        // Match return types
        if (param_type->data.func.return_types && arg_type->data.func.return_types) {
            for (int i = 0; param_type->data.func.return_types[i] && arg_type->data.func.return_types[i]; i++) {
                go_unify_type(param_type->data.func.return_types[i], arg_type->data.func.return_types[i],
                    type_param_names, inferred, param_count);
            }
        }
    }
}

// --- go_eval_expr_type: recursive expression type evaluator ---

const LSMType* go_eval_expr_type(GoLSPContext* ctx, TSNode node) {
    if (ts_node_is_null(node)) return lsm_type_unknown();

    const char* kind = ts_node_type(node);

    // --- Identifier: scope lookup ---
    if (strcmp(kind, "identifier") == 0) {
        char* name = lsp_node_text(ctx, node);
        if (!name) return lsm_type_unknown();

        // Check scope first
        const LSMType* t = lsm_scope_lookup(ctx->current_scope, name);
        if (lsm_scope_contains(ctx->current_scope, name))
            return t ? t : lsm_type_unknown();

        // Check if it's a package-level function
        const LSMRegisteredFunc* f = lsm_registry_lookup_symbol(ctx->registry, ctx->package_qn, name);
        if (f && f->signature) return f->signature;

        // Check if it's a builtin type (for type conversions like string(x), int(x))
        const LSMType* bt = resolve_builtin_type(ctx, name);
        if (bt) return lsm_type_named(ctx->arena, name);

        // Check if it's a registered type (for type conversions like MyType(x))
        const char* type_qn = lsm_arena_sprintf(ctx->arena, "%s.%s", ctx->package_qn, name);
        const LSMRegisteredType* rt = lsm_registry_lookup_type(ctx->registry, type_qn);
        if (rt) return lsm_type_named(ctx->arena, type_qn);

        return lsm_type_unknown();
    }

    // --- Selector expression: a.B ---
    if (strcmp(kind, "selector_expression") == 0) {
        TSNode operand = ts_node_child_by_field_name(node, "operand", 7);
        TSNode field = ts_node_child_by_field_name(node, "field", 5);
        if (ts_node_is_null(operand) || ts_node_is_null(field)) return lsm_type_unknown();

        char* field_name = lsp_node_text(ctx, field);
        if (!field_name) return lsm_type_unknown();

        // Check if operand is an import alias (pkg.Symbol)
        if (strcmp(ts_node_type(operand), "identifier") == 0) {
            char* pkg_name = lsp_node_text(ctx, operand);
            if (pkg_name) {
                const char* pkg_qn = resolve_import(ctx, pkg_name);
                if (pkg_qn) {
                    // Look up pkg.Symbol as a function or type
                    const LSMRegisteredFunc* f = lsm_registry_lookup_symbol(ctx->registry, pkg_qn, field_name);
                    if (f && f->signature) return f->signature;
                    // Check if it's a type
                    char* type_qn = lsm_arena_sprintf(ctx->arena, "%s.%s", pkg_qn, field_name);
                    const LSMRegisteredType* rt = lsm_registry_lookup_type(ctx->registry, type_qn);
                    if (rt) return lsm_type_named(ctx->arena, type_qn);
                    return lsm_type_unknown();
                }
            }
        }

        // Evaluate operand type
        const LSMType* recv_type = go_eval_expr_type(ctx, operand);
        if (lsm_type_is_unknown(recv_type)) return lsm_type_unknown();

        // Auto-deref pointers for method calls
        const LSMType* base_type = recv_type;
        if (base_type->kind == LSM_TYPE_POINTER) {
            base_type = lsm_type_deref(base_type);
        }

        if (base_type->kind == LSM_TYPE_NAMED) {
            const char* type_qn = base_type->data.named.qualified_name;

            // Look up method/field (methods recurse through embeddings)
            const LSMRegisteredFunc* method = go_lookup_field_or_method(ctx, type_qn, field_name);
            if (method && method->signature) return method->signature;

            // Check struct fields (with embedded field promotion)
            const LSMType* field_type = go_lookup_field(ctx, type_qn, field_name, 0);
            if (field_type && !lsm_type_is_unknown(field_type)) return field_type;
        }

        return lsm_type_unknown();
    }

    // --- Call expression: f(...) ---
    if (strcmp(kind, "call_expression") == 0) {
        TSNode func_node = ts_node_child_by_field_name(node, "function", 8);
        TSNode args_node = ts_node_child_by_field_name(node, "arguments", 9);
        if (ts_node_is_null(func_node)) return lsm_type_unknown();

        // Check for builtin calls
        if (strcmp(ts_node_type(func_node), "identifier") == 0) {
            char* name = lsp_node_text(ctx, func_node);
            if (name && is_go_builtin_func(name)) {
                return go_eval_builtin_call(ctx, name, args_node);
            }
        }

        // Evaluate function type
        const LSMType* func_type = go_eval_expr_type(ctx, func_node);

        // If it's a FUNC type, return its return type
        if (func_type && func_type->kind == LSM_TYPE_FUNC &&
            func_type->data.func.return_types && func_type->data.func.return_types[0]) {

            // Check for explicit type arguments: call_expression has type_arguments field
            // Go tree-sitter: Func[T1, T2](args) → call_expression { function, type_arguments, arguments }
            TSNode targs_node = ts_node_child_by_field_name(node, "type_arguments", 14);
            if (!ts_node_is_null(targs_node)) {
                // Look up the registered function to get type_param_names
                const LSMRegisteredFunc* rfunc = NULL;
                char* func_name = lsp_node_text(ctx, func_node);
                if (func_name) {
                    const char* func_qn = lsm_arena_sprintf(ctx->arena, "%s.%s",
                        ctx->package_qn, func_name);
                    rfunc = lsm_registry_lookup_func(ctx->registry, func_qn);
                    // If not found as local, try via import (selector_expression: pkg.Func)
                    if (!rfunc && strcmp(ts_node_type(func_node), "selector_expression") == 0) {
                        TSNode operand = ts_node_child_by_field_name(func_node, "operand", 7);
                        TSNode field = ts_node_child_by_field_name(func_node, "field", 5);
                        if (!ts_node_is_null(operand) && !ts_node_is_null(field)) {
                            char* pkg = lsp_node_text(ctx, operand);
                            char* fn = lsp_node_text(ctx, field);
                            if (pkg && fn) {
                                const char* pkg_qn = resolve_import(ctx, pkg);
                                if (pkg_qn) {
                                    rfunc = lsm_registry_lookup_symbol(ctx->registry, pkg_qn, fn);
                                }
                            }
                        }
                    }
                }

                if (rfunc && rfunc->type_param_names) {
                    // Parse type arguments from AST
                    const LSMType* type_args[16];
                    int targ_count = 0;
                    uint32_t ta_nc = ts_node_child_count(targs_node);
                    for (uint32_t ti = 0; ti < ta_nc && targ_count < 15; ti++) {
                        TSNode targ = ts_node_child(targs_node, ti);
                        if (ts_node_is_null(targ) || !ts_node_is_named(targ)) continue;
                        type_args[targ_count++] = go_parse_type_node(ctx, targ);
                    }

                    // Count type params
                    int param_count = 0;
                    while (rfunc->type_param_names[param_count]) param_count++;

                    if (targ_count > 0 && targ_count == param_count) {
                        const LSMType** targ_arr = (const LSMType**)lsm_arena_alloc(
                            ctx->arena, (targ_count + 1) * sizeof(const LSMType*));
                        for (int ti = 0; ti < targ_count; ti++) targ_arr[ti] = type_args[ti];
                        targ_arr[targ_count] = NULL;

                        // Substitute type params in return type(s)
                        int ret_count = 0;
                        while (func_type->data.func.return_types[ret_count]) ret_count++;

                        if (ret_count == 1) {
                            return lsm_type_substitute(ctx->arena,
                                func_type->data.func.return_types[0],
                                rfunc->type_param_names, targ_arr);
                        }
                        // Multi-return: substitute all
                        const LSMType** new_rets = (const LSMType**)lsm_arena_alloc(
                            ctx->arena, (ret_count + 1) * sizeof(const LSMType*));
                        for (int ri = 0; ri < ret_count; ri++) {
                            new_rets[ri] = lsm_type_substitute(ctx->arena,
                                func_type->data.func.return_types[ri],
                                rfunc->type_param_names, targ_arr);
                        }
                        new_rets[ret_count] = NULL;
                        return lsm_type_tuple(ctx->arena, new_rets, ret_count);
                    }
                }
            }

            // Implicit generics: infer type args from argument types
            if (ts_node_is_null(targs_node)) {
                // Look up registered function to check for type_param_names
                const LSMRegisteredFunc* rfunc = NULL;
                char* func_name = lsp_node_text(ctx, func_node);
                if (func_name) {
                    const char* func_qn = lsm_arena_sprintf(ctx->arena, "%s.%s",
                        ctx->package_qn, func_name);
                    rfunc = lsm_registry_lookup_func(ctx->registry, func_qn);
                    if (!rfunc && strcmp(ts_node_type(func_node), "selector_expression") == 0) {
                        TSNode operand = ts_node_child_by_field_name(func_node, "operand", 7);
                        TSNode field = ts_node_child_by_field_name(func_node, "field", 5);
                        if (!ts_node_is_null(operand) && !ts_node_is_null(field)) {
                            char* pkg = lsp_node_text(ctx, operand);
                            char* fn = lsp_node_text(ctx, field);
                            if (pkg && fn) {
                                const char* pkg_qn = resolve_import(ctx, pkg);
                                if (pkg_qn)
                                    rfunc = lsm_registry_lookup_symbol(ctx->registry, pkg_qn, fn);
                            }
                        }
                    }
                }

                if (rfunc && rfunc->type_param_names && func_type->data.func.param_types) {
                    int tpc = 0;
                    while (rfunc->type_param_names[tpc]) tpc++;

                    // Check if any return type uses a type param
                    bool has_type_param = false;
                    int ret_count = 0;
                    while (func_type->data.func.return_types[ret_count]) ret_count++;
                    for (int ri = 0; ri < ret_count && !has_type_param; ri++) {
                        // Quick check: walk return type tree for TYPE_PARAM nodes
                        const LSMType* rt = func_type->data.func.return_types[ri];
                        if (rt->kind == LSM_TYPE_TYPE_PARAM) has_type_param = true;
                        else if (rt->kind == LSM_TYPE_SLICE && rt->data.slice.elem &&
                                 rt->data.slice.elem->kind == LSM_TYPE_TYPE_PARAM) has_type_param = true;
                        else if (rt->kind == LSM_TYPE_POINTER && rt->data.pointer.elem &&
                                 rt->data.pointer.elem->kind == LSM_TYPE_TYPE_PARAM) has_type_param = true;
                        else if (rt->kind == LSM_TYPE_MAP) {
                            if (rt->data.map.key && rt->data.map.key->kind == LSM_TYPE_TYPE_PARAM) has_type_param = true;
                            if (rt->data.map.value && rt->data.map.value->kind == LSM_TYPE_TYPE_PARAM) has_type_param = true;
                        }
                    }

                    if (has_type_param && tpc > 0 && tpc <= 16) {
                        // Evaluate argument types and unify
                        const LSMType* inferred[16] = {0};

                        if (!ts_node_is_null(args_node)) {
                            uint32_t argc = ts_node_named_child_count(args_node);
                            int pi = 0;
                            for (uint32_t ai = 0; ai < argc && func_type->data.func.param_types[pi]; ai++) {
                                TSNode arg = ts_node_named_child(args_node, ai);
                                if (ts_node_is_null(arg)) continue;
                                const LSMType* arg_type = go_eval_expr_type(ctx, arg);
                                go_unify_type(func_type->data.func.param_types[pi],
                                    arg_type, rfunc->type_param_names, inferred, tpc);
                                pi++;
                            }
                        }

                        // Check if all type params were inferred
                        bool all_inferred = true;
                        for (int i = 0; i < tpc; i++) {
                            if (!inferred[i]) { all_inferred = false; break; }
                        }

                        if (all_inferred) {
                            const LSMType** targ_arr = (const LSMType**)lsm_arena_alloc(
                                ctx->arena, (tpc + 1) * sizeof(const LSMType*));
                            for (int i = 0; i < tpc; i++) targ_arr[i] = inferred[i];
                            targ_arr[tpc] = NULL;

                            if (ret_count == 1) {
                                return lsm_type_substitute(ctx->arena,
                                    func_type->data.func.return_types[0],
                                    rfunc->type_param_names, targ_arr);
                            }
                            const LSMType** new_rets = (const LSMType**)lsm_arena_alloc(
                                ctx->arena, (ret_count + 1) * sizeof(const LSMType*));
                            for (int ri = 0; ri < ret_count; ri++) {
                                new_rets[ri] = lsm_type_substitute(ctx->arena,
                                    func_type->data.func.return_types[ri],
                                    rfunc->type_param_names, targ_arr);
                            }
                            new_rets[ret_count] = NULL;
                            return lsm_type_tuple(ctx->arena, new_rets, ret_count);
                        }
                    }
                }
            }

            // No type arguments or no substitution needed — return as-is
            if (!func_type->data.func.return_types[1]) {
                return func_type->data.func.return_types[0];
            }
            int count = 0;
            while (func_type->data.func.return_types[count]) count++;
            if (count > 1) {
                return lsm_type_tuple(ctx->arena, func_type->data.func.return_types, count);
            }
        }

        // Type conversion: Type(expr) — if func_node resolves to a named type
        if (func_type && func_type->kind == LSM_TYPE_NAMED) {
            return func_type;
        }

        // Type conversion with composite type syntax: []byte(s), map[K]V(x), etc.
        // The function node is a type node (slice_type, map_type, etc.)
        {
            const char* fk = ts_node_type(func_node);
            if (strcmp(fk, "slice_type") == 0 || strcmp(fk, "array_type") == 0 ||
                strcmp(fk, "map_type") == 0 || strcmp(fk, "pointer_type") == 0 ||
                strcmp(fk, "channel_type") == 0) {
                return go_parse_type_node(ctx, func_node);
            }
        }

        return lsm_type_unknown();
    }

    // --- Composite literal: Type{...} ---
    if (strcmp(kind, "composite_literal") == 0) {
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        if (!ts_node_is_null(type_node)) {
            return go_parse_type_node(ctx, type_node);
        }
        return lsm_type_unknown();
    }

    // --- Unary expression: &x, *x, <-ch, !x ---
    if (strcmp(kind, "unary_expression") == 0) {
        TSNode operand = ts_node_child_by_field_name(node, "operand", 7);
        if (ts_node_is_null(operand)) return lsm_type_unknown();

        // Get operator
        for (uint32_t i = 0; i < ts_node_child_count(node); i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child)) {
                char* op = lsp_node_text(ctx, child);
                if (!op) continue;
                if (strcmp(op, "&") == 0) {
                    return lsm_type_pointer(ctx->arena, go_eval_expr_type(ctx, operand));
                }
                if (strcmp(op, "*") == 0) {
                    return lsm_type_deref(go_eval_expr_type(ctx, operand));
                }
                if (strcmp(op, "<-") == 0) {
                    const LSMType* ch_type = go_eval_expr_type(ctx, operand);
                    if (ch_type && ch_type->kind == LSM_TYPE_CHANNEL) {
                        return ch_type->data.channel.elem;
                    }
                    return lsm_type_unknown();
                }
                if (strcmp(op, "!") == 0) {
                    return lsm_type_builtin(ctx->arena, "bool");
                }
                break;
            }
        }
        return lsm_type_unknown();
    }

    // --- Index expression: a[i] ---
    if (strcmp(kind, "index_expression") == 0) {
        TSNode operand = ts_node_child_by_field_name(node, "operand", 7);
        if (ts_node_is_null(operand)) return lsm_type_unknown();
        const LSMType* op_type = go_eval_expr_type(ctx, operand);
        if (!op_type) return lsm_type_unknown();
        if (op_type->kind == LSM_TYPE_MAP) return op_type->data.map.value;
        if (op_type->kind == LSM_TYPE_SLICE) return op_type->data.slice.elem;
        return lsm_type_unknown();
    }

    // --- Type assertion: x.(Type) ---
    if (strcmp(kind, "type_assertion_expression") == 0) {
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        if (!ts_node_is_null(type_node)) {
            return go_parse_type_node(ctx, type_node);
        }
        return lsm_type_unknown();
    }

    // --- Parenthesized expression: (x) ---
    if (strcmp(kind, "parenthesized_expression") == 0 && ts_node_named_child_count(node) > 0) {
        return go_eval_expr_type(ctx, ts_node_named_child(node, 0));
    }

    // --- Binary expression ---
    if (strcmp(kind, "binary_expression") == 0) {
        // For comparisons, return bool
        // For arithmetic, return left operand type
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        for (uint32_t i = 0; i < ts_node_child_count(node); i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child)) {
                char* op = lsp_node_text(ctx, child);
                if (op && (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
                           strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                           strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                           strcmp(op, "&&") == 0 || strcmp(op, "||") == 0)) {
                    return lsm_type_builtin(ctx->arena, "bool");
                }
                break;
            }
        }
        if (!ts_node_is_null(left)) return go_eval_expr_type(ctx, left);
        return lsm_type_unknown();
    }

    // --- Slice expression: a[low:high] ---
    if (strcmp(kind, "slice_expression") == 0) {
        TSNode operand = ts_node_child_by_field_name(node, "operand", 7);
        if (!ts_node_is_null(operand)) return go_eval_expr_type(ctx, operand);
        return lsm_type_unknown();
    }

    // --- Literals ---
    if (strcmp(kind, "interpreted_string_literal") == 0 ||
        strcmp(kind, "raw_string_literal") == 0) {
        return lsm_type_builtin(ctx->arena, "string");
    }
    if (strcmp(kind, "int_literal") == 0) {
        return lsm_type_builtin(ctx->arena, "int");
    }
    if (strcmp(kind, "float_literal") == 0) {
        return lsm_type_builtin(ctx->arena, "float64");
    }
    if (strcmp(kind, "true") == 0 || strcmp(kind, "false") == 0) {
        return lsm_type_builtin(ctx->arena, "bool");
    }
    if (strcmp(kind, "nil") == 0) {
        return lsm_type_unknown(); // nil has no concrete type
    }

    // --- Func literal (closure) ---
    if (strcmp(kind, "func_literal") == 0) {
        // Process the closure body to resolve calls with captured scope
        TSNode body = ts_node_child_by_field_name(node, "body", 4);
        if (!ts_node_is_null(body)) {
            // Push child scope (inherits all outer bindings via parent chain)
            LSMScope* saved = ctx->current_scope;
            ctx->current_scope = lsm_scope_push(ctx->arena, ctx->current_scope);

            // Bind closure parameters
            TSNode params = ts_node_child_by_field_name(node, "parameters", 10);
            if (!ts_node_is_null(params)) {
                uint32_t nc = ts_node_child_count(params);
                for (uint32_t i = 0; i < nc; i++) {
                    TSNode param = ts_node_child(params, i);
                    if (ts_node_is_null(param) || !ts_node_is_named(param)) continue;
                    if (strcmp(ts_node_type(param), "parameter_declaration") != 0) continue;
                    TSNode type_node = ts_node_child_by_field_name(param, "type", 4);
                    const LSMType* pt = go_parse_type_node(ctx, type_node);
                    uint32_t pnc = ts_node_child_count(param);
                    for (uint32_t j = 0; j < pnc; j++) {
                        TSNode ch = ts_node_child(param, j);
                        if (!ts_node_is_null(ch) && ts_node_is_named(ch) &&
                            strcmp(ts_node_type(ch), "identifier") == 0) {
                            char* pname = lsp_node_text(ctx, ch);
                            if (pname && strcmp(pname, "_") != 0)
                                lsm_scope_bind(ctx->current_scope, pname, pt);
                        }
                    }
                }
            }

            // Walk closure body to resolve calls
            resolve_calls_in_node(ctx, body);

            ctx->current_scope = saved;
        }

        // Build full FUNC type with param/return types from AST
        const LSMType* pt_arr[16];
        int pt_count = 0;
        TSNode params2 = ts_node_child_by_field_name(node, "parameters", 10);
        if (!ts_node_is_null(params2)) {
            uint32_t nc2 = ts_node_child_count(params2);
            for (uint32_t i = 0; i < nc2 && pt_count < 15; i++) {
                TSNode p = ts_node_child(params2, i);
                if (ts_node_is_null(p) || !ts_node_is_named(p)) continue;
                if (strcmp(ts_node_type(p), "parameter_declaration") != 0) continue;
                TSNode pt = ts_node_child_by_field_name(p, "type", 4);
                if (!ts_node_is_null(pt))
                    pt_arr[pt_count++] = go_parse_type_node(ctx, pt);
            }
        }
        pt_arr[pt_count] = NULL;

        const LSMType* rt_arr[16];
        int rt_count = 0;
        TSNode result = ts_node_child_by_field_name(node, "result", 6);
        if (!ts_node_is_null(result)) {
            if (strcmp(ts_node_type(result), "parameter_list") == 0) {
                uint32_t rnc = ts_node_child_count(result);
                for (uint32_t i = 0; i < rnc && rt_count < 15; i++) {
                    TSNode rc = ts_node_child(result, i);
                    if (ts_node_is_null(rc) || !ts_node_is_named(rc)) continue;
                    TSNode rt = ts_node_child_by_field_name(rc, "type", 4);
                    if (ts_node_is_null(rt)) rt = rc;
                    rt_arr[rt_count++] = go_parse_type_node(ctx, rt);
                }
            } else {
                rt_arr[rt_count++] = go_parse_type_node(ctx, result);
            }
        }
        rt_arr[rt_count] = NULL;

        return lsm_type_func(ctx->arena, NULL,
            pt_count > 0 ? (const LSMType**)pt_arr : NULL,
            rt_count > 0 ? (const LSMType**)rt_arr : NULL);
    }

    return lsm_type_unknown();
}

// --- go_eval_builtin_call ---

const LSMType* go_eval_builtin_call(GoLSPContext* ctx, const char* name, TSNode args) {
    // make(Type, ...) -> Type
    if (strcmp(name, "make") == 0 && !ts_node_is_null(args)) {
        uint32_t nc = ts_node_named_child_count(args);
        if (nc > 0) {
            TSNode first_arg = ts_node_named_child(args, 0);
            return go_parse_type_node(ctx, first_arg);
        }
    }

    // new(Type) -> *Type
    if (strcmp(name, "new") == 0 && !ts_node_is_null(args)) {
        uint32_t nc = ts_node_named_child_count(args);
        if (nc > 0) {
            TSNode first_arg = ts_node_named_child(args, 0);
            return lsm_type_pointer(ctx->arena, go_parse_type_node(ctx, first_arg));
        }
    }

    // append(slice, ...) -> same slice type
    if (strcmp(name, "append") == 0 && !ts_node_is_null(args)) {
        uint32_t nc = ts_node_named_child_count(args);
        if (nc > 0) {
            return go_eval_expr_type(ctx, ts_node_named_child(args, 0));
        }
    }

    // len, cap -> int
    if (strcmp(name, "len") == 0 || strcmp(name, "cap") == 0) {
        return lsm_type_builtin(ctx->arena, "int");
    }

    // delete -> void (no return)
    if (strcmp(name, "delete") == 0) {
        return lsm_type_unknown();
    }

    return lsm_type_unknown();
}

// --- go_lookup_field: struct field lookup with embedding recursion ---

static const LSMType* go_lookup_field(GoLSPContext* ctx,
    const char* type_qn, const char* field_name, int depth) {
    if (!type_qn || !field_name || depth > 5) return NULL;

    const LSMRegisteredType* rt = lsm_registry_lookup_type(ctx->registry, type_qn);
    if (!rt) return NULL;

    // Follow alias chain
    if (rt->alias_of) return go_lookup_field(ctx, rt->alias_of, field_name, depth + 1);

    // Direct field lookup
    if (rt->field_names) {
        for (int i = 0; rt->field_names[i]; i++) {
            if (strcmp(rt->field_names[i], field_name) == 0 && rt->field_types && rt->field_types[i]) {
                return rt->field_types[i];
            }
        }
    }

    // Promoted fields from embedded types
    if (rt->embedded_types) {
        for (int i = 0; rt->embedded_types[i]; i++) {
            const LSMType* f = go_lookup_field(ctx, rt->embedded_types[i], field_name, depth + 1);
            if (f) return f;
        }
    }

    return NULL;
}

// --- go_lookup_field_or_method: method sets + embedding ---

static const LSMRegisteredFunc* go_lookup_field_or_method_depth(GoLSPContext* ctx,
    const char* type_qn, const char* member_name, int depth) {
    if (!type_qn || !member_name) return NULL;
    if (depth > LSM_LSP_MAX_LOOKUP_DEPTH) return NULL;

    // Direct method lookup
    const LSMRegisteredFunc* f = lsm_registry_lookup_method(ctx->registry, type_qn, member_name);
    if (f) return f;

    const LSMRegisteredType* rt = lsm_registry_lookup_type(ctx->registry, type_qn);
    if (rt) {
        // Follow type alias chain
        if (rt->alias_of) {
            f = go_lookup_field_or_method_depth(ctx, rt->alias_of, member_name, depth + 1);
            if (f) return f;
        }

        // Check embedded types (promoted methods)
        if (rt->embedded_types) {
            for (int i = 0; rt->embedded_types[i]; i++) {
                f = go_lookup_field_or_method_depth(ctx, rt->embedded_types[i], member_name, depth + 1);
                if (f) return f;
            }
        }
    }

    return NULL;
}

const LSMRegisteredFunc* go_lookup_field_or_method(GoLSPContext* ctx,
    const char* type_qn, const char* member_name) {
    return go_lookup_field_or_method_depth(ctx, type_qn, member_name, 0);
}

/* Resolve only source forms that prove one callable identity.  This is
 * intentionally narrower than go_eval_expr_type: conditional expressions,
 * indexed dispatch tables, interfaces with several implementers, and unknown
 * locals remain ordinary values. */
static const char *go_exact_callable_target(GoLSPContext *ctx, TSNode node) {
    if (!ctx || ts_node_is_null(node))
        return NULL;
    const char *kind = ts_node_type(node);

    if (strcmp(kind, "parenthesized_expression") == 0 && ts_node_named_child_count(node) == 1) {
        return go_exact_callable_target(ctx, ts_node_named_child(node, 0));
    }

    if (strcmp(kind, "identifier") == 0) {
        char *name = lsp_node_text(ctx, node);
        if (!name)
            return NULL;
        /* A local value shadows the package declaration even when its type is
         * UNKNOWN.  Only an explicitly tracked callable alias may pass. */
        if (lsm_scope_contains(ctx->current_scope, name)) {
            return lsm_scope_lookup_callable(ctx->current_scope, name);
        }
        const LSMRegisteredFunc *f =
            lsm_registry_lookup_symbol(ctx->registry, ctx->package_qn, name);
        return f ? f->qualified_name : NULL;
    }

    if (strcmp(kind, "selector_expression") == 0) {
        TSNode operand = ts_node_child_by_field_name(node, "operand", 7);
        TSNode field = ts_node_child_by_field_name(node, "field", 5);
        if (ts_node_is_null(operand) || ts_node_is_null(field))
            return NULL;
        char *member = lsp_node_text(ctx, field);
        if (!member)
            return NULL;

        if (strcmp(ts_node_type(operand), "identifier") == 0) {
            char *local = lsp_node_text(ctx, operand);
            const char *package_qn = local ? resolve_import(ctx, local) : NULL;
            if (package_qn) {
                const LSMRegisteredFunc *f =
                    lsm_registry_lookup_symbol(ctx->registry, package_qn, member);
                return f ? f->qualified_name : NULL;
            }
        }

        const LSMType *recv = go_eval_expr_type(ctx, operand);
        if (recv && recv->kind == LSM_TYPE_POINTER)
            recv = lsm_type_deref(recv);
        if (recv && recv->kind == LSM_TYPE_NAMED) {
            /* A direct method name is unique in a valid Go method set.
             * Promoted lookup through embedded types can be ambiguous; without
             * full selector-depth resolution, fail closed and retain USAGE. */
            const LSMRegisteredFunc *method =
                lsm_registry_lookup_method(ctx->registry, recv->data.named.qualified_name, member);
            return method ? method->qualified_name : NULL;
        }
    }
    return NULL;
}

static void go_scope_bind_value(GoLSPContext *ctx, const char *name, const LSMType *type,
                                const char *callable_qn) {
    if (callable_qn) {
        lsm_scope_bind_callable(ctx->current_scope, name, type, callable_qn);
    } else {
        lsm_scope_bind(ctx->current_scope, name, type);
    }
}

// --- go_process_statement: bind variables from statements ---

void go_process_statement(GoLSPContext* ctx, TSNode node) {
    if (ts_node_is_null(node)) return;
    const char* kind = ts_node_type(node);

    // short_var_declaration: a, b := expr
    if (strcmp(kind, "short_var_declaration") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        TSNode right = ts_node_child_by_field_name(node, "right", 5);
        if (ts_node_is_null(left) || ts_node_is_null(right)) return;

        const LSMType* rhs_type = NULL;
        TSNode single_rhs = {0};

        // Check if RHS is an expression_list (multiple values)
        if (strcmp(ts_node_type(right), "expression_list") == 0) {
            uint32_t rhs_count = ts_node_named_child_count(right);
            if (rhs_count == 1) {
                // Single expression that might return a tuple (multi-return)
                single_rhs = ts_node_named_child(right, 0);
                rhs_type = go_eval_expr_type(ctx, single_rhs);
            }
        } else {
            single_rhs = right;
            rhs_type = go_eval_expr_type(ctx, right);
        }
        const char *rhs_callable =
            ts_node_is_null(single_rhs) ? NULL : go_exact_callable_target(ctx, single_rhs);

        // Bind left-hand side variables
        if (strcmp(ts_node_type(left), "expression_list") == 0) {
            uint32_t lhs_count = ts_node_named_child_count(left);
            for (uint32_t i = 0; i < lhs_count; i++) {
                TSNode lhs_var = ts_node_named_child(left, i);
                if (strcmp(ts_node_type(lhs_var), "identifier") != 0) continue;
                char* var_name = lsp_node_text(ctx, lhs_var);
                if (!var_name || strcmp(var_name, "_") == 0) continue;

                const LSMType* var_type = lsm_type_unknown();
                if (rhs_type) {
                    if (rhs_type->kind == LSM_TYPE_TUPLE && (int)i < rhs_type->data.tuple.count) {
                        var_type = rhs_type->data.tuple.elems[i];
                    } else if (i == 0) {
                        var_type = rhs_type;
                    }
                }
                go_scope_bind_value(ctx, var_name, var_type, i == 0 ? rhs_callable : NULL);
            }
        } else if (strcmp(ts_node_type(left), "identifier") == 0) {
            char* var_name = lsp_node_text(ctx, left);
            if (var_name && strcmp(var_name, "_") != 0 && rhs_type) {
                go_scope_bind_value(ctx, var_name, rhs_type, rhs_callable);
            }
        }
        return;
    }

    /* Reassignment must update (or clear) callable identity just like :=.
     * Without this, `alias := target; alias = dynamic; alias()` would retain a
     * stale exact target and fabricate a call. */
    if (strcmp(kind, "assignment_statement") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        TSNode right = ts_node_child_by_field_name(node, "right", 5);
        if (ts_node_is_null(left) || ts_node_is_null(right))
            return;
        uint32_t lhs_count = strcmp(ts_node_type(left), "expression_list") == 0
                                 ? ts_node_named_child_count(left)
                                 : 1;
        uint32_t rhs_count = strcmp(ts_node_type(right), "expression_list") == 0
                                 ? ts_node_named_child_count(right)
                                 : 1;
        for (uint32_t i = 0; i < lhs_count; i++) {
            TSNode lhs = lhs_count == 1 && strcmp(ts_node_type(left), "expression_list") != 0
                             ? left
                             : ts_node_named_child(left, i);
            if (ts_node_is_null(lhs) || strcmp(ts_node_type(lhs), "identifier") != 0)
                continue;
            char *name = lsp_node_text(ctx, lhs);
            if (!name || strcmp(name, "_") == 0)
                continue;
            TSNode rhs = {0};
            if (rhs_count == lhs_count) {
                rhs = rhs_count == 1 && strcmp(ts_node_type(right), "expression_list") != 0
                          ? right
                          : ts_node_named_child(right, i);
            }
            const LSMType *type =
                ts_node_is_null(rhs) ? lsm_type_unknown() : go_eval_expr_type(ctx, rhs);
            const char *callable = ts_node_is_null(rhs) ? NULL : go_exact_callable_target(ctx, rhs);
            go_scope_bind_value(ctx, name, type, callable);
        }
        return;
    }

    // var_spec: var x Type = expr  OR  var a, b, c Type = expr
    if (strcmp(kind, "var_spec") == 0) {
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        TSNode value_node = ts_node_child_by_field_name(node, "value", 5);

        const LSMType* var_type = lsm_type_unknown();
        TSNode callable_value = value_node;
        if (!ts_node_is_null(value_node) &&
            strcmp(ts_node_type(value_node), "expression_list") == 0) {
            callable_value = ts_node_named_child_count(value_node) == 1
                                 ? ts_node_named_child(value_node, 0)
                                 : (TSNode){0};
        }
        if (!ts_node_is_null(type_node)) {
            var_type = go_parse_type_node(ctx, type_node);
        } else if (!ts_node_is_null(value_node)) {
            if (strcmp(ts_node_type(value_node), "expression_list") == 0 &&
                ts_node_named_child_count(value_node) > 0) {
                var_type = go_eval_expr_type(ctx, ts_node_named_child(value_node, 0));
            } else {
                var_type = go_eval_expr_type(ctx, value_node);
            }
        }
        const char *callable_qn =
            ts_node_is_null(callable_value) ? NULL : go_exact_callable_target(ctx, callable_value);

        // Bind all name identifiers (handles: var a, b, c int)
        uint32_t vnc = ts_node_child_count(node);
        for (uint32_t vi = 0; vi < vnc; vi++) {
            TSNode ch = ts_node_child(node, vi);
            if (ts_node_is_null(ch) || !ts_node_is_named(ch)) continue;
            if (strcmp(ts_node_type(ch), "identifier") == 0) {
                char* var_name = lsp_node_text(ctx, ch);
                if (var_name && strcmp(var_name, "_") != 0) {
                    go_scope_bind_value(ctx, var_name, var_type, callable_qn);
                }
            }
        }
        return;
    }

    // const_spec: const x Type = expr  OR  const x = expr
    if (strcmp(kind, "const_spec") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        TSNode value_node = ts_node_child_by_field_name(node, "value", 5);
        if (ts_node_is_null(name_node)) return;

        const LSMType* const_type = lsm_type_unknown();
        TSNode callable_value = value_node;
        if (!ts_node_is_null(value_node) &&
            strcmp(ts_node_type(value_node), "expression_list") == 0) {
            callable_value = ts_node_named_child_count(value_node) == 1
                                 ? ts_node_named_child(value_node, 0)
                                 : (TSNode){0};
        }
        if (!ts_node_is_null(type_node)) {
            const_type = go_parse_type_node(ctx, type_node);
        } else if (!ts_node_is_null(value_node)) {
            // For expression_list values, evaluate first element
            if (strcmp(ts_node_type(value_node), "expression_list") == 0 &&
                ts_node_named_child_count(value_node) > 0) {
                const_type = go_eval_expr_type(ctx, ts_node_named_child(value_node, 0));
            } else {
                const_type = go_eval_expr_type(ctx, value_node);
            }
        }
        const char *callable_qn =
            ts_node_is_null(callable_value) ? NULL : go_exact_callable_target(ctx, callable_value);

        if (strcmp(ts_node_type(name_node), "identifier") == 0) {
            char* name = lsp_node_text(ctx, name_node);
            if (name && strcmp(name, "_") != 0)
                go_scope_bind_value(ctx, name, const_type, callable_qn);
        }
        return;
    }

    // range_clause: for k, v := range container
    if (strcmp(kind, "range_clause") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        TSNode right = ts_node_child_by_field_name(node, "right", 5);
        if (ts_node_is_null(right)) return;

        const LSMType* container_type = go_eval_expr_type(ctx, right);

        // Determine key and value types based on container type
        const LSMType* key_type = lsm_type_unknown();
        const LSMType* val_type = lsm_type_unknown();

        if (container_type) {
            switch (container_type->kind) {
            case LSM_TYPE_SLICE:
                key_type = lsm_type_builtin(ctx->arena, "int");
                val_type = container_type->data.slice.elem;
                break;
            case LSM_TYPE_MAP:
                key_type = container_type->data.map.key;
                val_type = container_type->data.map.value;
                break;
            case LSM_TYPE_CHANNEL:
                val_type = container_type->data.channel.elem;
                break;
            default:
                if (container_type->kind == LSM_TYPE_BUILTIN &&
                    strcmp(container_type->data.builtin.name, "string") == 0) {
                    key_type = lsm_type_builtin(ctx->arena, "int");
                    val_type = lsm_type_builtin(ctx->arena, "rune");
                }
                break;
            }
        }

        if (!ts_node_is_null(left) && strcmp(ts_node_type(left), "expression_list") == 0) {
            uint32_t lhs_count = ts_node_named_child_count(left);
            for (uint32_t i = 0; i < lhs_count; i++) {
                TSNode var_node = ts_node_named_child(left, i);
                if (strcmp(ts_node_type(var_node), "identifier") != 0) continue;
                char* var_name = lsp_node_text(ctx, var_node);
                if (!var_name || strcmp(var_name, "_") == 0) continue;
                lsm_scope_bind(ctx->current_scope, var_name, i == 0 ? key_type : val_type);
            }
        }
        return;
    }

    // type_switch_statement is handled in resolve_calls_in_node, not here
    // (needs per-case scope narrowing, not just variable binding)
}

// --- Emit a resolved call/reference ---

static void go_emit_resolved_kind(GoLSPContext *ctx, const char *callee_qn, const char *strategy,
                                  float confidence, const char *source_name, LSMResolvedKind kind,
                                  TSNode site) {
    if (!ctx->resolved_calls || !callee_qn || !ctx->enclosing_func_qn) return;

    LSMResolvedCall rc = {0};
    rc.caller_qn = ctx->enclosing_func_qn;
    rc.callee_qn = callee_qn;
    rc.strategy = strategy;
    rc.confidence = confidence;
    /* When a callable alias has a different textual leaf, retain that source
     * name for the exact-site join. */
    rc.reason = source_name;
    rc.kind = kind;
    if (!ts_node_is_null(site)) {
        rc.site_start_byte = ts_node_start_byte(site);
        rc.site_end_byte = ts_node_end_byte(site);
    }
    lsm_resolvedcall_push(ctx->resolved_calls, ctx->arena, rc);
}

static void emit_resolved_call(GoLSPContext *ctx, const char *callee_qn, const char *strategy,
                               float confidence, TSNode site) {
    go_emit_resolved_kind(ctx, callee_qn, strategy, confidence, NULL, LSM_RESOLVED_INVOCATION,
                          site);
}

static void emit_resolved_alias_call(GoLSPContext *ctx, const char *callee_qn,
                                     const char *source_name, TSNode site) {
    go_emit_resolved_kind(ctx, callee_qn, "lsp_callable_alias", 0.97f, source_name,
                          LSM_RESOLVED_INVOCATION, site);
}

static void emit_resolved_reference(GoLSPContext *ctx, const char *callee_qn,
                                    const char *source_name, TSNode site) {
    const char *leaf = strrchr(callee_qn, '.');
    leaf = leaf ? leaf + 1 : callee_qn;
    const char *join_name = source_name && strcmp(source_name, leaf) != 0 ? source_name : NULL;
    go_emit_resolved_kind(ctx, callee_qn, "lsp_callable_value_reference", 0.97f, join_name,
                          LSM_RESOLVED_CALL_REFERENCE, site);
}

static void emit_unresolved_reference(GoLSPContext *ctx, const char *source_name, TSNode site) {
    if (!ctx || !ctx->resolved_calls || !ctx->enclosing_func_qn || !source_name ||
        ts_node_is_null(site)) {
        return;
    }
    LSMResolvedCall rc = {0};
    rc.caller_qn = ctx->enclosing_func_qn;
    rc.callee_qn = source_name;
    rc.strategy = "lsp_unresolved";
    rc.confidence = 0.0f;
    rc.reason = "callable_value_not_in_registry";
    rc.kind = LSM_RESOLVED_CALL_REFERENCE;
    rc.site_start_byte = ts_node_start_byte(site);
    rc.site_end_byte = ts_node_end_byte(site);
    lsm_resolvedcall_push(ctx->resolved_calls, ctx->arena, rc);
}

// Emit a diagnostic for an unresolved call (confidence 0.0).
static void emit_unresolved_call(GoLSPContext *ctx, const char *expr_text, const char *reason,
                                 TSNode site) {
    if (!ctx->resolved_calls || !ctx->enclosing_func_qn) return;

    LSMResolvedCall rc = {0};
    rc.caller_qn = ctx->enclosing_func_qn;
    rc.callee_qn = expr_text ? expr_text : "?";
    rc.strategy = "lsp_unresolved";
    rc.confidence = 0.0f;
    rc.reason = reason;
    rc.kind = LSM_RESOLVED_INVOCATION;
    if (!ts_node_is_null(site)) {
        rc.site_start_byte = ts_node_start_byte(site);
        rc.site_end_byte = ts_node_end_byte(site);
    }
    lsm_resolvedcall_push(ctx->resolved_calls, ctx->arena, rc);
}

/* Only direct argument expressions are candidates. Descending into a
 * conditional/map/index expression would promote its constituent names even
 * though the runtime callable is not statically unique. */
static void go_resolve_value_references_at(GoLSPContext *ctx, TSNode call) {
    TSNode args = ts_node_child_by_field_name(call, "arguments", 9);
    if (ts_node_is_null(args))
        return;
    uint32_t count = ts_node_named_child_count(args);
    for (uint32_t i = 0; i < count; i++) {
        TSNode arg = ts_node_named_child(args, i);
        const char *kind = ts_node_type(arg);
        if (strcmp(kind, "identifier") != 0 && strcmp(kind, "selector_expression") != 0 &&
            strcmp(kind, "parenthesized_expression") != 0)
            continue;
        const char *target = go_exact_callable_target(ctx, arg);
        char *source_name = lsp_node_text(ctx, arg);
        if (target) {
            emit_resolved_reference(ctx, target, source_name, arg);
        } else if (strcmp(kind, "identifier") == 0 && source_name &&
                   !lsm_scope_contains(ctx->current_scope, source_name)) {
            /* The per-file registry cannot see an unqualified function from
             * another file in the same Go package. Preserve the exact source
             * occurrence for the metadata-only project pass. */
            emit_unresolved_reference(ctx, source_name, arg);
        }
    }
}

/* Any assignment that is guarded by a branch/loop makes the post-construct
 * callable identity a join of at least two paths. Clear the outer exact
 * identity before walking the construct; assignments inside its temporary
 * scope may still be exact locally, but cannot leak past the join. */
static void go_invalidate_control_flow_aliases(GoLSPContext *ctx, TSNode node, int depth) {
    if (!ctx || ts_node_is_null(node) || depth > 64)
        return;
    const char *kind = ts_node_type(node);
    if (strcmp(kind, "func_literal") == 0)
        return;
    if (strcmp(kind, "assignment_statement") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        if (!ts_node_is_null(left)) {
            uint32_t count = strcmp(ts_node_type(left), "expression_list") == 0
                                 ? ts_node_named_child_count(left)
                                 : 1;
            for (uint32_t i = 0; i < count; i++) {
                TSNode target = count == 1 && strcmp(ts_node_type(left), "expression_list") != 0
                                    ? left
                                    : ts_node_named_child(left, i);
                if (ts_node_is_null(target) || strcmp(ts_node_type(target), "identifier") != 0) {
                    continue;
                }
                char *name = lsp_node_text(ctx, target);
                if (name && lsm_scope_lookup_callable(ctx->current_scope, name)) {
                    lsm_scope_update_callable(ctx->current_scope, name, NULL);
                }
            }
        }
        return;
    }
    uint32_t count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        go_invalidate_control_flow_aliases(ctx, ts_node_named_child(node, i), depth + 1);
    }
}

// --- Walk call expressions and resolve them ---

static void resolve_calls_in_node_inner(GoLSPContext* ctx, TSNode node) {
    if (ts_node_is_null(node)) return;
    const char* kind = ts_node_type(node);

    // Process statements to build scope
    go_process_statement(ctx, node);

    if (strcmp(kind, "if_statement") == 0 || strcmp(kind, "for_statement") == 0 ||
        strcmp(kind, "expression_switch_statement") == 0 ||
        strcmp(kind, "type_switch_statement") == 0 || strcmp(kind, "select_statement") == 0) {
        go_invalidate_control_flow_aliases(ctx, node, 0);
    }

    // Resolve call expressions
    if (strcmp(kind, "call_expression") == 0) {
        go_resolve_value_references_at(ctx, node);
        TSNode func_node = ts_node_child_by_field_name(node, "function", 8);
        if (!ts_node_is_null(func_node)) {
            const char* fk = ts_node_type(func_node);

            // selector_expression: obj.Method() or pkg.Func()
            if (strcmp(fk, "selector_expression") == 0) {
                TSNode operand = ts_node_child_by_field_name(func_node, "operand", 7);
                TSNode field = ts_node_child_by_field_name(func_node, "field", 5);
                if (!ts_node_is_null(operand) && !ts_node_is_null(field)) {
                    char* field_name = lsp_node_text(ctx, field);

                    // Check if operand is a package import
                    if (strcmp(ts_node_type(operand), "identifier") == 0) {
                        char* pkg_name = lsp_node_text(ctx, operand);
                        if (pkg_name) {
                            const char* pkg_qn = resolve_import(ctx, pkg_name);
                            if (pkg_qn && field_name) {
                                const LSMRegisteredFunc* f = lsm_registry_lookup_symbol(ctx->registry, pkg_qn, field_name);
                                if (f) {
                                    emit_resolved_call(ctx, f->qualified_name, "lsp_direct", 0.95f,
                                                       node);
                                    goto recurse;
                                }
                                // Package found but symbol not in registry
                                emit_unresolved_call(
                                    ctx,
                                    lsm_arena_sprintf(ctx->arena, "%s.%s", pkg_name, field_name),
                                    "symbol_not_in_registry", node);
                                goto recurse;
                            }
                        }
                    }

                    // Type-based method dispatch
                    if (field_name) {
                        const LSMType* recv_type = go_eval_expr_type(ctx, operand);
                        const LSMType* base = recv_type;
                        if (base && base->kind == LSM_TYPE_POINTER) base = lsm_type_deref(base);

                        if (base && base->kind == LSM_TYPE_NAMED) {
                            const LSMRegisteredType *receiver_type = lsm_registry_lookup_type(
                                ctx->registry, base->data.named.qualified_name);
                            /* Registered interface receivers must reach the
                             * interface-resolution branch below. Their semantic
                             * method registrations are signatures, not concrete
                             * dispatch targets. */
                            if (!receiver_type || !receiver_type->is_interface) {
                                const LSMRegisteredFunc *method = go_lookup_field_or_method(
                                    ctx, base->data.named.qualified_name, field_name);
                                if (method) {
                                    const char *strategy = "lsp_type_dispatch";
                                    if (method->receiver_type &&
                                        strcmp(method->receiver_type,
                                               base->data.named.qualified_name) != 0) {
                                        strategy = "lsp_embed_dispatch";
                                    }
                                    emit_resolved_call(ctx, method->qualified_name, strategy, 0.95f,
                                                       node);
                                    goto recurse;
                                }
                            }
                        }

                        // Interface dispatch: NAMED type that is an interface, or bare INTERFACE type
                        if (base && field_name) {
                            bool is_iface = (base->kind == LSM_TYPE_INTERFACE);
                            const char* iface_qn = NULL;
                            if (!is_iface && base->kind == LSM_TYPE_NAMED) {
                                const LSMRegisteredType* rt = lsm_registry_lookup_type(ctx->registry,
                                    base->data.named.qualified_name);
                                if (rt && rt->is_interface) {
                                    is_iface = true;
                                    iface_qn = base->data.named.qualified_name;
                                }
                            }
                            if (is_iface) {
                                // Try interface satisfaction: find concrete types implementing this interface
                                const LSMRegisteredType* iface_rt = iface_qn ?
                                    lsm_registry_lookup_type(ctx->registry, iface_qn) : NULL;
                                if (iface_rt && iface_rt->method_names && iface_rt->method_names[0]) {
                                    // Count interface methods
                                    int iface_mcount = 0;
                                    while (iface_rt->method_names[iface_mcount]) iface_mcount++;

                                    // Scan all registered types for satisfaction
                                    const char* sole_impl_qn = NULL;
                                    int impl_count = 0;
                                    // Skip stdlib types when interface is from a project package
                                    bool iface_is_project = iface_qn && strchr(iface_qn, '/') != NULL;
                                    for (int ti = 0; ti < ctx->registry->type_count && impl_count < 2; ti++) {
                                        const LSMRegisteredType* cand = &ctx->registry->types[ti];
                                        if (cand->is_interface) continue;
                                        if (!cand->qualified_name) continue;
                                        if (cand->alias_of) continue;
                                        // For project interfaces, skip stdlib candidates (no '/' in QN)
                                        if (iface_is_project && !strchr(cand->qualified_name, '/')) continue;

                                        // Check if candidate has all interface methods
                                        bool satisfies = true;
                                        for (int mi = 0; mi < iface_mcount; mi++) {
                                            if (!lsm_registry_lookup_method(ctx->registry,
                                                    cand->qualified_name, iface_rt->method_names[mi])) {
                                                satisfies = false;
                                                break;
                                            }
                                        }
                                        if (satisfies) {
                                            sole_impl_qn = cand->qualified_name;
                                            impl_count++;
                                        }
                                    }

                                    if (impl_count == 1 && sole_impl_qn) {
                                        // Single implementer: resolve to concrete method
                                        const LSMRegisteredFunc* concrete_method =
                                            lsm_registry_lookup_method(ctx->registry, sole_impl_qn, field_name);
                                        if (concrete_method) {
                                            // Sole-implementer interface dispatch is an unambiguous
                                            // resolution (exactly one concrete method); rank it at least
                                            // as high as a direct type dispatch (0.95) so the concrete
                                            // `Type.method` wins over the interface-method type_dispatch
                                            // for the same call site.
                                            emit_resolved_call(ctx, concrete_method->qualified_name,
                                                               "lsp_interface_resolve", 0.95f,
                                                               node);
                                            goto recurse;
                                        }
                                    }
                                }

                                // Fallback: generic interface dispatch
                                emit_resolved_call(
                                    ctx,
                                    lsm_arena_sprintf(ctx->arena, "%s.%s",
                                                      iface_qn ? iface_qn : "interface",
                                                      field_name),
                                    "lsp_interface_dispatch", 0.85f, node);
                                goto recurse;
                            }
                        }

                        // Type resolved to NAMED but neither method nor interface matched
                        if (base && base->kind == LSM_TYPE_NAMED) {
                            emit_unresolved_call(ctx,
                                                 lsm_arena_sprintf(ctx->arena, "%s.%s",
                                                                   base->data.named.qualified_name,
                                                                   field_name),
                                                 "method_not_found", node);
                        } else if (lsm_type_is_unknown(recv_type)) {
                            char* operand_text = lsp_node_text(ctx, operand);
                            emit_unresolved_call(
                                ctx,
                                lsm_arena_sprintf(ctx->arena, "%s.%s",
                                                  operand_text ? operand_text : "?", field_name),
                                "unknown_receiver_type", node);
                        }
                    }
                }
            }

            // Direct function call: FuncName()
            if (strcmp(fk, "identifier") == 0) {
                char* name = lsp_node_text(ctx, func_node);
                if (name && !is_go_builtin_func(name)) {
                    if (lsm_scope_contains(ctx->current_scope, name)) {
                        const char *alias_target =
                            lsm_scope_lookup_callable(ctx->current_scope, name);
                        if (alias_target) {
                            emit_resolved_alias_call(ctx, alias_target, name, node);
                        } else {
                            emit_unresolved_call(ctx, name, "lexical_value_not_exact_callable",
                                                 node);
                        }
                        goto recurse;
                    }
                    // Package-local function
                    const LSMRegisteredFunc* f = lsm_registry_lookup_symbol(ctx->registry, ctx->package_qn, name);
                    if (f) {
                        emit_resolved_call(ctx, f->qualified_name, "lsp_direct", 0.95f, node);
                    } else {
                        emit_unresolved_call(ctx, name, "function_not_in_registry", node);
                    }
                }
            }
        }
    }

recurse:;
    // Push scope for blocks and statements that introduce variables
    bool push_scope = (strcmp(kind, "block") == 0 ||
                       strcmp(kind, "if_statement") == 0 ||
                       strcmp(kind, "for_statement") == 0 ||
                       strcmp(kind, "expression_switch_statement") == 0);
    if (push_scope) {
        ctx->current_scope = lsm_scope_push(ctx->arena, ctx->current_scope);
    }

    // Process initializer field for if/for/switch statements.
    // Go patterns: if err := f(); ..., for i := 0; ..., switch v := x; v { ... }
    if (strcmp(kind, "if_statement") == 0 ||
        strcmp(kind, "for_statement") == 0 ||
        strcmp(kind, "expression_switch_statement") == 0) {
        TSNode init = ts_node_child_by_field_name(node, "initializer", 11);
        if (!ts_node_is_null(init)) {
            go_process_statement(ctx, init);
        }
    }

    // Process for_statement range_clause before recursing into body
    if (strcmp(kind, "for_statement") == 0) {
        for (uint32_t i = 0; i < ts_node_child_count(node); i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_null(child) && ts_node_is_named(child) &&
                strcmp(ts_node_type(child), "range_clause") == 0) {
                go_process_statement(ctx, child);
                break;
            }
        }
    }

    // Type switch: switch a := expr.(type) { case *T: a.Method() }
    // Go tree-sitter structure: type_switch_statement has flat children:
    //   expression_list (contains var name "a")
    //   identifier (operand "animal")
    //   type_case (case clause, may repeat)
    if (strcmp(kind, "type_switch_statement") == 0) {
        const char* switch_var = NULL;
        // Pass 1: find switch variable and operand type
        uint32_t nc2 = ts_node_child_count(node);
        bool found_assign = false;
        for (uint32_t i = 0; i < nc2; i++) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_null(child)) continue;
            const char* ck = ts_node_type(child);

            // expression_list before := is the LHS (variable name)
            if (!found_assign && strcmp(ck, "expression_list") == 0) {
                TSNode var_node = ts_node_named_child(child, 0);
                if (!ts_node_is_null(var_node) && strcmp(ts_node_type(var_node), "identifier") == 0)
                    switch_var = lsp_node_text(ctx, var_node);
            }
            // := operator marks the assignment
            if (!ts_node_is_named(child)) {
                char* tok = lsp_node_text(ctx, child);
                if (tok && strcmp(tok, ":=") == 0) found_assign = true;
            }
            // identifier after := is the operand being type-switched
            if (found_assign && strcmp(ck, "identifier") == 0) {
                (void)go_eval_expr_type(ctx, child);
            }
        }

        // Pass 2: process each type_case with narrowed scope
        for (uint32_t i = 0; i < nc2; i++) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
            if (strcmp(ts_node_type(child), "type_case") != 0) continue;

            LSMScope* saved = ctx->current_scope;
            ctx->current_scope = lsm_scope_push(ctx->arena, ctx->current_scope);

            // Find case type and bind switch variable
            uint32_t cc_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < cc_count; j++) {
                TSNode cc_child = ts_node_child(child, j);
                if (ts_node_is_null(cc_child) || !ts_node_is_named(cc_child)) continue;
                const char* cc_kind = ts_node_type(cc_child);
                if (strcmp(cc_kind, "type_identifier") == 0 ||
                    strcmp(cc_kind, "qualified_type") == 0 ||
                    strcmp(cc_kind, "pointer_type") == 0 ||
                    strcmp(cc_kind, "slice_type") == 0) {
                    if (switch_var) {
                        lsm_scope_bind(ctx->current_scope, switch_var,
                            go_parse_type_node(ctx, cc_child));
                    }
                    break;
                }
            }

            // Recurse into case body statements
            for (uint32_t j = 0; j < cc_count; j++) {
                TSNode cc_child = ts_node_child(child, j);
                if (ts_node_is_null(cc_child) || !ts_node_is_named(cc_child)) continue;
                const char* cc_kind = ts_node_type(cc_child);
                // Skip type nodes, process everything else (expression_statement, etc.)
                if (strcmp(cc_kind, "type_identifier") == 0 ||
                    strcmp(cc_kind, "qualified_type") == 0 ||
                    strcmp(cc_kind, "pointer_type") == 0 ||
                    strcmp(cc_kind, "slice_type") == 0) continue;
                resolve_calls_in_node(ctx, cc_child);
            }

            ctx->current_scope = saved;
        }

        if (push_scope) ctx->current_scope = lsm_scope_pop(ctx->current_scope);
        return;
    }

    // Select statement: each communication_case gets its own scope
    // case msg := <-ch:  →  receive_statement has left (vars) + unary_expression (<-ch)
    if (strcmp(kind, "select_statement") == 0) {
        uint32_t nc3 = ts_node_child_count(node);
        for (uint32_t i = 0; i < nc3; i++) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
            const char* ck3 = ts_node_type(child);

            if (strcmp(ck3, "communication_case") == 0 || strcmp(ck3, "default_case") == 0) {
                LSMScope* saved = ctx->current_scope;
                ctx->current_scope = lsm_scope_push(ctx->arena, ctx->current_scope);

                // Process children: receive_statement binds vars, then recurse body
                uint32_t cc_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < cc_count; j++) {
                    TSNode cc_child = ts_node_child(child, j);
                    if (ts_node_is_null(cc_child) || !ts_node_is_named(cc_child)) continue;
                    const char* cc_kind = ts_node_type(cc_child);

                    if (strcmp(cc_kind, "receive_statement") == 0) {
                        // receive_statement: left := <-right
                        // tree-sitter Go: right field is the channel expression (after <-)
                        // The receive value type is the channel's element type
                        TSNode left = ts_node_child_by_field_name(cc_child, "left", 4);
                        TSNode right = ts_node_child_by_field_name(cc_child, "right", 5);
                        if (!ts_node_is_null(right)) {
                            const LSMType* right_type = go_eval_expr_type(ctx, right);
                            // right_type might be a channel (if right is the channel)
                            // or already the elem type (if right is a <-ch unary expr)
                            const LSMType* recv_type = right_type;
                            if (right_type && right_type->kind == LSM_TYPE_CHANNEL) {
                                recv_type = right_type->data.channel.elem;
                            }
                            if (!ts_node_is_null(left)) {
                                if (strcmp(ts_node_type(left), "expression_list") == 0) {
                                    uint32_t lhs_count = ts_node_named_child_count(left);
                                    for (uint32_t k = 0; k < lhs_count; k++) {
                                        TSNode var_node = ts_node_named_child(left, k);
                                        if (strcmp(ts_node_type(var_node), "identifier") != 0) continue;
                                        char* var_name = lsp_node_text(ctx, var_node);
                                        if (!var_name || strcmp(var_name, "_") == 0) continue;
                                        if (k == 0) {
                                            lsm_scope_bind(ctx->current_scope, var_name, recv_type);
                                        } else {
                                            // second var is the ok bool
                                            lsm_scope_bind(ctx->current_scope, var_name,
                                                lsm_type_builtin(ctx->arena, "bool"));
                                        }
                                    }
                                } else if (strcmp(ts_node_type(left), "identifier") == 0) {
                                    char* var_name = lsp_node_text(ctx, left);
                                    if (var_name && strcmp(var_name, "_") != 0) {
                                        lsm_scope_bind(ctx->current_scope, var_name, recv_type);
                                    }
                                }
                            }
                        }
                    } else {
                        // Body statements
                        resolve_calls_in_node(ctx, cc_child);
                    }
                }

                ctx->current_scope = saved;
            }
        }
        if (push_scope) ctx->current_scope = lsm_scope_pop(ctx->current_scope);
        return;
    }

    // Recurse into children via a cursor (O(n)); ts_node_child(node,i) is O(i)
    // → O(n²) on a wide node.
    {
        TSTreeCursor cursor = ts_tree_cursor_new(node);
        if (ts_tree_cursor_goto_first_child(&cursor)) {
            do {
                resolve_calls_in_node(ctx, ts_tree_cursor_current_node(&cursor));
            } while (ts_tree_cursor_goto_next_sibling(&cursor));
        }
        ts_tree_cursor_delete(&cursor);
    }

    if (push_scope) {
        ctx->current_scope = lsm_scope_pop(ctx->current_scope);
    }
}

// --- Process a single function body ---

static void process_function(GoLSPContext* ctx, TSNode func_node) {
    // Set enclosing function QN
    TSNode name_node = ts_node_child_by_field_name(func_node, "name", 4);
    if (ts_node_is_null(name_node)) return;

    char* func_name = lsp_node_text(ctx, name_node);
    if (!func_name || !func_name[0]) return;

    // For methods, the enclosing-function QN must include the receiver type
    // (package.Type.Method), matching how the textual extractor and the
    // registry qualify the method. Building it as package.Method (no receiver)
    // here made the LSP-resolved call's caller_qn disagree with the textual
    // call's enclosing_func_qn, so lsm_pipeline_find_lsp_resolution never
    // joined them — every call inside a method body silently lost its
    // type-aware LSP strategy. Derive the bare receiver type name the same way
    // the receiver binding below does.
    char* recv_type_name = NULL;
    {
        TSNode recv0 = ts_node_child_by_field_name(func_node, "receiver", 8);
        if (!ts_node_is_null(recv0)) {
            uint32_t rnc0 = ts_node_child_count(recv0);
            for (uint32_t i = 0; i < rnc0 && !recv_type_name; i++) {
                TSNode rp = ts_node_child(recv0, i);
                if (ts_node_is_null(rp) || !ts_node_is_named(rp)) continue;
                if (strcmp(ts_node_type(rp), "parameter_declaration") != 0) continue;
                TSNode rtype = ts_node_child_by_field_name(rp, "type", 4);
                if (ts_node_is_null(rtype)) continue;
                // Unwrap a pointer receiver (*Type) to the bare type identifier.
                const char* rtk = ts_node_type(rtype);
                if (strcmp(rtk, "pointer_type") == 0 && ts_node_named_child_count(rtype) > 0) {
                    rtype = ts_node_named_child(rtype, 0);
                }
                char* tn = lsp_node_text(ctx, rtype);
                if (tn && tn[0]) recv_type_name = tn;
            }
        }
    }

    if (recv_type_name) {
        ctx->enclosing_func_qn =
            lsm_arena_sprintf(ctx->arena, "%s.%s.%s", ctx->package_qn, recv_type_name, func_name);
    } else {
        ctx->enclosing_func_qn = lsm_arena_sprintf(ctx->arena, "%s.%s", ctx->package_qn, func_name);
    }

    // Push function scope
    LSMScope* saved_scope = ctx->current_scope;
    ctx->current_scope = lsm_scope_push(ctx->arena, ctx->current_scope);

    // Bind parameters into scope (including variadic)
    TSNode params = ts_node_child_by_field_name(func_node, "parameters", 10);
    if (!ts_node_is_null(params)) {
        uint32_t nc = ts_node_child_count(params);
        for (uint32_t i = 0; i < nc; i++) {
            TSNode param = ts_node_child(params, i);
            if (ts_node_is_null(param) || !ts_node_is_named(param)) continue;
            const char* pk = ts_node_type(param);

            bool is_variadic = (strcmp(pk, "variadic_parameter_declaration") == 0);
            if (!is_variadic && strcmp(pk, "parameter_declaration") != 0) continue;

            // Get type
            TSNode type_node = ts_node_child_by_field_name(param, "type", 4);
            const LSMType* param_type = go_parse_type_node(ctx, type_node);

            // Variadic: ...T is []T in the function body
            if (is_variadic && param_type) {
                param_type = lsm_type_slice(ctx->arena, param_type);
            }

            // Get name(s) — Go allows multiple names per declaration: a, b int
            uint32_t pnc = ts_node_child_count(param);
            for (uint32_t j = 0; j < pnc; j++) {
                TSNode child = ts_node_child(param, j);
                if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
                if (strcmp(ts_node_type(child), "identifier") == 0) {
                    char* pname = lsp_node_text(ctx, child);
                    if (pname && strcmp(pname, "_") != 0) {
                        lsm_scope_bind(ctx->current_scope, pname, param_type);
                    }
                }
            }
        }
    }

    // Bind named return values into scope
    TSNode result_node = ts_node_child_by_field_name(func_node, "result", 6);
    if (!ts_node_is_null(result_node)) {
        // result can be a parameter_list (named returns) or a simple type
        const char* rk = ts_node_type(result_node);
        if (strcmp(rk, "parameter_list") == 0) {
            uint32_t rnc = ts_node_child_count(result_node);
            for (uint32_t i = 0; i < rnc; i++) {
                TSNode rparam = ts_node_child(result_node, i);
                if (ts_node_is_null(rparam) || !ts_node_is_named(rparam)) continue;
                if (strcmp(ts_node_type(rparam), "parameter_declaration") != 0) continue;

                TSNode rtype = ts_node_child_by_field_name(rparam, "type", 4);
                const LSMType* ret_type = go_parse_type_node(ctx, rtype);

                uint32_t rpnc = ts_node_child_count(rparam);
                for (uint32_t j = 0; j < rpnc; j++) {
                    TSNode child = ts_node_child(rparam, j);
                    if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
                    if (strcmp(ts_node_type(child), "identifier") == 0) {
                        char* rname = lsp_node_text(ctx, child);
                        if (rname && strcmp(rname, "_") != 0) {
                            lsm_scope_bind(ctx->current_scope, rname, ret_type);
                        }
                    }
                }
            }
        }
    }

    // Bind receiver for methods
    TSNode recv = ts_node_child_by_field_name(func_node, "receiver", 8);
    if (!ts_node_is_null(recv)) {
        // receiver is a parameter_list with one parameter_declaration
        uint32_t rnc = ts_node_child_count(recv);
        for (uint32_t i = 0; i < rnc; i++) {
            TSNode rp = ts_node_child(recv, i);
            if (ts_node_is_null(rp) || !ts_node_is_named(rp)) continue;
            if (strcmp(ts_node_type(rp), "parameter_declaration") != 0) continue;

            TSNode rtype = ts_node_child_by_field_name(rp, "type", 4);
            const LSMType* recv_type = go_parse_type_node(ctx, rtype);

            // Find receiver name
            uint32_t rpnc = ts_node_child_count(rp);
            for (uint32_t j = 0; j < rpnc; j++) {
                TSNode rc = ts_node_child(rp, j);
                if (!ts_node_is_null(rc) && ts_node_is_named(rc) &&
                    strcmp(ts_node_type(rc), "identifier") == 0) {
                    char* rname = lsp_node_text(ctx, rc);
                    if (rname && strcmp(rname, "_") != 0) {
                        lsm_scope_bind(ctx->current_scope, rname, recv_type);
                    }
                    break;
                }
            }
        }
    }

    // Walk function body
    TSNode body = ts_node_child_by_field_name(func_node, "body", 4);
    if (!ts_node_is_null(body)) {
        resolve_calls_in_node(ctx, body);
    }

    // Restore scope
    ctx->current_scope = saved_scope;
}

// --- Process entire file ---

void go_lsp_process_file(GoLSPContext* ctx, TSNode root) {
    if (ts_node_is_null(root)) return;

    // Collect top-level children once (O(n)); ts_node_child(root,i) is O(i) → O(n²).
    uint32_t kn = 0;
    TSNode* kids = lsm_lsp_collect_children(ctx->arena, root, &kn);

    // Pass 1: Bind package-level var/const declarations into root scope.
    // Must happen before processing functions so all globals are visible.
    for (uint32_t i = 0; i < kn; i++) {
        TSNode child = kids[i];
        const char* kind = ts_node_type(child);

        if (strcmp(kind, "var_declaration") == 0 || strcmp(kind, "const_declaration") == 0) {
            uint32_t vnc = ts_node_child_count(child);
            for (uint32_t j = 0; j < vnc; j++) {
                TSNode spec = ts_node_child(child, j);
                if (ts_node_is_null(spec) || !ts_node_is_named(spec)) continue;
                go_process_statement(ctx, spec);
            }
        }
    }

    // Pass 2: Process function/method bodies
    for (uint32_t i = 0; i < kn; i++) {
        TSNode child = kids[i];
        const char* kind = ts_node_type(child);

        if (strcmp(kind, "function_declaration") == 0 ||
            strcmp(kind, "method_declaration") == 0) {
            process_function(ctx, child);
        }
    }
}

// --- Helper: parse Go return type text into LSMType ---

const LSMType* lsm_parse_return_type_text(LSMArena* a, const char* text, const char* module_qn) {
    if (!text || !text[0]) return lsm_type_unknown();

    // Pointer: *Foo
    if (text[0] == '*') {
        return lsm_type_pointer(a, lsm_parse_return_type_text(a, text + 1, module_qn));
    }

    // Slice: []Foo
    if (text[0] == '[' && text[1] == ']') {
        return lsm_type_slice(a, lsm_parse_return_type_text(a, text + 2, module_qn));
    }

    // Builtin types
    static const char* builtins[] = {
        "int","int8","int16","int32","int64",
        "uint","uint8","uint16","uint32","uint64",
        "float32","float64","string","bool","byte","rune","error",
        "any","uintptr",
        NULL
    };
    for (const char** b = builtins; *b; b++) {
        if (strcmp(text, *b) == 0) return lsm_type_builtin(a, text);
    }

    // Named type: assume local to module
    return lsm_type_named(a, lsm_arena_sprintf(a, "%s.%s", module_qn, text));
}

typedef struct {
    const char *module_qn;
} LSMGoLSPSignatureParserContext;

static const LSMType *lsm_go_lsp_parse_signature_param_text(LSMArena *arena, const char *text,
                                                            void *parser_ctx) {
    const LSMGoLSPSignatureParserContext *ctx = (const LSMGoLSPSignatureParserContext *)parser_ctx;
    const char *module_qn = ctx && ctx->module_qn ? ctx->module_qn : "";
    return lsm_parse_return_type_text(arena, text, module_qn);
}

// --- Entry point: build registry from file defs + run LSP ---

void lsm_run_go_lsp(LSMArena* arena, LSMFileResult* result,
    const char* source, int source_len, TSNode root) {

    LSMTypeRegistry reg;
    lsm_registry_init(&reg, arena);

    // Register Go stdlib types/functions
    lsm_go_stdlib_register(&reg, arena);

    const char* module_qn = result->module_qn;

    // Phase 1: Register all types and functions from the file's own definitions
    for (int i = 0; i < result->defs.count; i++) {
        LSMDefinition* d = &result->defs.items[i];
        if (!d->qualified_name || !d->name) continue;

        // Register every type-like container (Class/Struct/Type/Interface/Enum/
        // Trait). Struct included so a Go `type T struct {...}` (now labelled
        // "Struct") is registered as a type and its methods/embedding resolve.
        if (lsm_label_is_type_like(d->label)) {
            LSMRegisteredType rt;
            memset(&rt, 0, sizeof(rt));
            rt.qualified_name = d->qualified_name;
            rt.short_name = d->name;
            rt.is_interface = d->label && strcmp(d->label, "Interface") == 0;

            // Populate embedded_types from base_classes (Go struct embedding)
            if (d->base_classes) {
                int bc_count = 0;
                while (d->base_classes[bc_count]) bc_count++;
                if (bc_count > 0) {
                    const char** embedded = (const char**)lsm_arena_alloc(arena, (bc_count + 1) * sizeof(const char*));
                    for (int j = 0; j < bc_count; j++) {
                        const char* bc = d->base_classes[j];
                        // Strip pointer prefix for embedded *Type
                        while (bc[0] == '*') bc++;
                        // Qualify the embedded type name
                        embedded[j] = lsm_arena_sprintf(arena, "%s.%s", module_qn, bc);
                    }
                    embedded[bc_count] = NULL;
                    rt.embedded_types = embedded;
                }
            }

            lsm_registry_add_type(&reg, rt);
        }

        // Register Function/Method nodes
        if (d->label && (strcmp(d->label, "Function") == 0 || strcmp(d->label, "Method") == 0)) {
            LSMRegisteredFunc rf;
            memset(&rf, 0, sizeof(rf));
            rf.qualified_name = d->qualified_name;
            rf.short_name = d->name;

            // Build FUNC type from return_types
            const LSMType** ret_types = NULL;
            if (d->return_types) {
                int count = 0;
                while (d->return_types[count]) count++;
                if (count > 0) {
                    ret_types = (const LSMType**)lsm_arena_alloc(arena, (count + 1) * sizeof(const LSMType*));
                    for (int j = 0; j < count; j++) {
                        ret_types[j] = lsm_parse_return_type_text(arena, d->return_types[j], module_qn);
                    }
                    ret_types[count] = NULL;
                }
            } else if (d->return_type && d->return_type[0]) {
                // Fallback: single return_type string
                ret_types = (const LSMType**)lsm_arena_alloc(arena, 2 * sizeof(const LSMType*));
                ret_types[0] = lsm_parse_return_type_text(arena, d->return_type, module_qn);
                ret_types[1] = NULL;
            }

            /* The occurrence-preserving carrier is authoritative whenever it
             * is present, including a non-NULL carrier with an explicit zero
             * count. Its slots deliberately have no names because legacy name
             * extraction is not guaranteed to have the same multiplicity. */
            const LSMType** param_types_arr = NULL;
            const char **param_names_arr = NULL;
            bool has_ordered_params =
                d->signature_param_types != NULL || d->signature_param_count > 0;
            LSMGoLSPSignatureParserContext param_parser_ctx = {module_qn};
            if (has_ordered_params) {
                param_types_arr = lsm_type_materialize_signature_params(
                    arena, d->signature_param_types, d->signature_param_count,
                    lsm_go_lsp_parse_signature_param_text, &param_parser_ctx);
            } else if (d->param_types) {
                param_names_arr = d->param_names;
                int count = 0;
                while (d->param_types[count]) count++;
                if (count > 0) {
                    param_types_arr = (const LSMType**)lsm_arena_alloc(arena, (count + 1) * sizeof(const LSMType*));
                    for (int j = 0; j < count; j++) {
                        param_types_arr[j] = lsm_parse_return_type_text(arena, d->param_types[j], module_qn);
                    }
                    param_types_arr[count] = NULL;
                }
            } else {
                param_names_arr = d->param_names;
            }

            rf.signature = lsm_type_func(arena, param_names_arr, param_types_arr, ret_types);

            // Prefer the extractor's semantic receiver QN. Raw receiver text is
            // retained only as a compatibility fallback for hand-built defs;
            // reparsing it alone loses valid unnamed receivers such as `(Right)`.
            if (strcmp(d->label, "Method") == 0) {
                if (d->parent_class && d->parent_class[0]) {
                    rf.receiver_type = d->parent_class;
                } else if (d->receiver && d->receiver[0]) {
                    // Legacy named receiver formats: "(name *Type)" / "(name Type)".
                    const char *r = d->receiver;
                    while (*r == '(' || *r == ' ')
                        r++;
                    while (*r && *r != ' ' && *r != '*')
                        r++;
                    while (*r == ' ' || *r == '*')
                        r++;
                    const char *end = r;
                    while (*end && *end != ')' && *end != ' ')
                        end++;
                    if (end > r) {
                        char *type_name = lsm_arena_strndup(arena, r, (size_t)(end - r));
                        rf.receiver_type = lsm_arena_sprintf(arena, "%s.%s", module_qn, type_name);
                    }
                }

                if (rf.receiver_type && !lsm_registry_lookup_type(&reg, rf.receiver_type)) {
                    const char *dot = strrchr(rf.receiver_type, '.');
                    LSMRegisteredType auto_type;
                    memset(&auto_type, 0, sizeof(auto_type));
                    auto_type.qualified_name = rf.receiver_type;
                    auto_type.short_name = dot ? dot + 1 : rf.receiver_type;
                    lsm_registry_add_type(&reg, auto_type);
                }
            }

            lsm_registry_add_func(&reg, rf);
        }
    }

    // Phase 1b: Scan AST for struct definitions to populate embedded_types
    {
        // Collect top-level children once (O(n)); ts_node_child(root,i) is O(i) → O(n²).
        uint32_t rkn = 0;
        TSNode* rkids = lsm_lsp_collect_children(arena, root, &rkn);
        for (uint32_t i = 0; i < rkn; i++) {
            TSNode top = rkids[i];
            const char* tk = ts_node_type(top);
            // type_declaration contains type_spec children
            if (strcmp(tk, "type_declaration") != 0) continue;
            uint32_t td_nc = ts_node_child_count(top);
            for (uint32_t j = 0; j < td_nc; j++) {
                TSNode spec = ts_node_child(top, j);
                if (ts_node_is_null(spec) || !ts_node_is_named(spec)) continue;
                const char* spec_kind = ts_node_type(spec);

                // type_alias: type Foo = Bar
                if (strcmp(spec_kind, "type_alias") == 0) {
                    TSNode alias_name = ts_node_child_by_field_name(spec, "name", 4);
                    TSNode alias_type = ts_node_child_by_field_name(spec, "type", 4);
                    if (!ts_node_is_null(alias_name) && !ts_node_is_null(alias_type)) {
                        char* aname = lsm_node_text(arena, alias_name, source);
                        char* atarget = lsm_node_text(arena, alias_type, source);
                        if (aname && aname[0] && atarget && atarget[0]) {
                            const char* alias_type_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, aname);
                            const char* alias_target_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, atarget);
                            bool found_a = false;
                            for (int ti = 0; ti < reg.type_count; ti++) {
                                if (reg.types[ti].qualified_name &&
                                    strcmp(reg.types[ti].qualified_name, alias_type_qn) == 0) {
                                    reg.types[ti].alias_of = alias_target_qn;
                                    found_a = true;
                                    break;
                                }
                            }
                            if (!found_a) {
                                LSMRegisteredType alias_rt;
                                memset(&alias_rt, 0, sizeof(alias_rt));
                                alias_rt.qualified_name = alias_type_qn;
                                alias_rt.short_name = aname;
                                alias_rt.alias_of = alias_target_qn;
                                lsm_registry_add_type(&reg, alias_rt);
                            }
                        }
                    }
                    continue;
                }

                if (strcmp(spec_kind, "type_spec") != 0) continue;

                TSNode name_node = ts_node_child_by_field_name(spec, "name", 4);
                TSNode type_node = ts_node_child_by_field_name(spec, "type", 4);
                if (ts_node_is_null(name_node) || ts_node_is_null(type_node)) continue;

                char* type_name = lsm_node_text(arena,name_node, source);
                if (!type_name || !type_name[0]) continue;
                const char* type_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, type_name);

                // Interface type: extract method names for satisfaction checking
                if (strcmp(ts_node_type(type_node), "interface_type") == 0) {
                    const char* iface_methods[64];
                    int iface_method_count = 0;
                    uint32_t inl_nc = ts_node_named_child_count(type_node);
                    for (uint32_t k = 0; k < inl_nc && iface_method_count < 63; k++) {
                        TSNode child = ts_node_named_child(type_node, k);
                        if (ts_node_is_null(child)) continue;
                        const char* ck = ts_node_type(child);
                        // method_spec: MethodName(params) returns
                        if (strcmp(ck, "method_spec") == 0 || strcmp(ck, "method_elem") == 0) {
                            TSNode mname = ts_node_child_by_field_name(child, "name", 4);
                            if (!ts_node_is_null(mname)) {
                                char* mn = lsm_node_text(arena, mname, source);
                                if (mn && mn[0]) {
                                    iface_methods[iface_method_count++] = mn;
                                }
                            }
                        }
                    }
                    if (iface_method_count > 0) {
                        for (int ti = 0; ti < reg.type_count; ti++) {
                            if (!reg.types[ti].qualified_name ||
                                strcmp(reg.types[ti].qualified_name, type_qn) != 0) continue;
                            const char** names = (const char**)lsm_arena_alloc(arena,
                                (iface_method_count + 1) * sizeof(const char*));
                            for (int mi = 0; mi < iface_method_count; mi++) {
                                names[mi] = iface_methods[mi];
                            }
                            names[iface_method_count] = NULL;
                            reg.types[ti].method_names = names;
                            break;
                        }
                    }
                    continue;
                }

                if (strcmp(ts_node_type(type_node), "struct_type") != 0) continue;

                // Find field_declaration_list inside struct_type
                TSNode field_list = ts_node_child_by_field_name(type_node, "body", 4);
                if (ts_node_is_null(field_list)) {
                    // Some grammars use first named child
                    if (ts_node_named_child_count(type_node) > 0)
                        field_list = ts_node_named_child(type_node, 0);
                }
                if (ts_node_is_null(field_list)) continue;

                // Scan field_declarations for embeds and named fields
                const char* embeds[16];
                int embed_count = 0;
                const char* fld_names[64];
                const LSMType* fld_types[64];
                int fld_count = 0;

                // Create a temporary LSP context for parsing field types
                GoLSPContext tmp_ctx;
                memset(&tmp_ctx, 0, sizeof(tmp_ctx));
                tmp_ctx.arena = arena;
                tmp_ctx.source = source;
                tmp_ctx.source_len = (int)strlen(source);
                tmp_ctx.registry = &reg;
                tmp_ctx.package_qn = module_qn;

                uint32_t fl_nc = ts_node_child_count(field_list);
                for (uint32_t k = 0; k < fl_nc; k++) {
                    TSNode field = ts_node_child(field_list, k);
                    if (ts_node_is_null(field) || !ts_node_is_named(field)) continue;
                    if (strcmp(ts_node_type(field), "field_declaration") != 0) continue;

                    TSNode fname = ts_node_child_by_field_name(field, "name", 4);
                    TSNode ftype = ts_node_child_by_field_name(field, "type", 4);

                    if (ts_node_is_null(fname) && !ts_node_is_null(ftype)) {
                        // Embedded field: has type but no name
                        if (embed_count < 15) {
                            char* embed_text = lsm_node_text(arena, ftype, source);
                            if (embed_text && embed_text[0]) {
                                const char* et = embed_text;
                                while (*et == '*') et++;
                                embeds[embed_count++] = lsm_arena_sprintf(arena, "%s.%s", module_qn, et);
                            }
                        }
                    } else if (!ts_node_is_null(fname) && !ts_node_is_null(ftype) && fld_count < 63) {
                        // Named field: name + type
                        char* fn = lsm_node_text(arena, fname, source);
                        if (fn && fn[0]) {
                            fld_names[fld_count] = fn;
                            fld_types[fld_count] = go_parse_type_node(&tmp_ctx, ftype);
                            fld_count++;
                        }
                    }
                }

                // Find the registered type and update it
                for (int ti = 0; ti < reg.type_count; ti++) {
                    if (!reg.types[ti].qualified_name ||
                        strcmp(reg.types[ti].qualified_name, type_qn) != 0) continue;

                    if (embed_count > 0) {
                        const char** arr = (const char**)lsm_arena_alloc(arena,
                            (embed_count + 1) * sizeof(const char*));
                        for (int ei = 0; ei < embed_count; ei++) arr[ei] = embeds[ei];
                        arr[embed_count] = NULL;
                        reg.types[ti].embedded_types = arr;
                    }
                    if (fld_count > 0) {
                        const char** names = (const char**)lsm_arena_alloc(arena,
                            (fld_count + 1) * sizeof(const char*));
                        const LSMType** types = (const LSMType**)lsm_arena_alloc(arena,
                            (fld_count + 1) * sizeof(const LSMType*));
                        for (int fi = 0; fi < fld_count; fi++) {
                            names[fi] = fld_names[fi];
                            types[fi] = fld_types[fi];
                        }
                        names[fld_count] = NULL;
                        types[fld_count] = NULL;
                        reg.types[ti].field_names = names;
                        reg.types[ti].field_types = types;
                    }
                    break;
                }
            }
        }
    }

    // Phase 1c: Extract type parameters from generic function declarations
    extract_type_params_from_ast(arena, &reg, root, source, module_qn);

    // Phase 2: Build LSP context and run
    GoLSPContext lsp_ctx;
    go_lsp_init(&lsp_ctx, arena, source, source_len, &reg, module_qn, &result->resolved_calls);

    // Add imports
    for (int i = 0; i < result->imports.count; i++) {
        LSMImport* imp = &result->imports.items[i];
        if (imp->local_name && imp->module_path) {
            go_lsp_add_import(&lsp_ctx, imp->local_name, imp->module_path);
        }
    }

    // Process the file
    go_lsp_process_file(&lsp_ctx, root);
}

// --- Cross-file LSP: parse source, build registry from defs, run LSP ---

// Helper: split "|"-separated string into array of LSMType*.
static const LSMType** split_pipe_types(LSMArena* a, const char* text, const char* module_qn) {
    if (!text || !text[0]) return NULL;

    // Count separators
    int count = 1;
    for (const char* p = text; *p; p++) {
        if (*p == '|') count++;
    }

    const LSMType** arr = (const LSMType**)lsm_arena_alloc(a, (count + 1) * sizeof(const LSMType*));
    if (!arr) return NULL;

    // Split and parse each type
    char* buf = lsm_arena_strdup(a, text);
    int idx = 0;
    char* start = buf;
    for (char* p = buf; ; p++) {
        if (*p == '|' || *p == '\0') {
            char save = *p;
            *p = '\0';
            if (start[0]) {
                arr[idx++] = lsm_parse_return_type_text(a, start, module_qn);
            }
            if (save == '\0') break;
            start = p + 1;
        }
    }
    arr[idx] = NULL;
    return idx > 0 ? arr : NULL;
}

// Helper: split "|"-separated string into array of const char*.
static const char** split_pipe_strings(LSMArena* a, const char* text) {
    if (!text || !text[0]) return NULL;

    int count = 1;
    for (const char* p = text; *p; p++) {
        if (*p == '|') count++;
    }

    const char** arr = (const char**)lsm_arena_alloc(a, (count + 1) * sizeof(const char*));
    if (!arr) return NULL;

    char* buf = lsm_arena_strdup(a, text);
    int idx = 0;
    char* start = buf;
    for (char* p = buf; ; p++) {
        if (*p == '|' || *p == '\0') {
            char save = *p;
            *p = '\0';
            if (start[0]) {
                arr[idx++] = lsm_arena_strdup(a, start);
            }
            if (save == '\0') break;
            start = p + 1;
        }
    }
    arr[idx] = NULL;
    return idx > 0 ? arr : NULL;
}

// Helper: parse "|"-separated "name:type" field definitions and populate a registered type.
// Format: "Binder:Binder|Name:string|Count:int"
// type_text is resolved relative to def_module_qn.
static void parse_field_defs_into_type(LSMArena* arena, LSMTypeRegistry* reg,
    const char* type_qn, const char* field_defs, const char* def_module_qn) {
    if (!field_defs || !field_defs[0] || !type_qn) return;

    // Count fields
    int count = 1;
    for (const char* p = field_defs; *p; p++) {
        if (*p == '|') count++;
    }
    if (count > 63) count = 63;

    const char** names = (const char**)lsm_arena_alloc(arena, (count + 1) * sizeof(const char*));
    const LSMType** types = (const LSMType**)lsm_arena_alloc(arena, (count + 1) * sizeof(const LSMType*));
    if (!names || !types) return;

    char* buf = lsm_arena_strdup(arena, field_defs);
    int idx = 0;
    char* start = buf;
    for (char* p = buf; ; p++) {
        if (*p == '|' || *p == '\0') {
            char save = *p;
            *p = '\0';
            // Parse "name:type"
            char* colon = NULL;
            for (char* q = start; *q; q++) {
                if (*q == ':') { colon = q; break; }
            }
            if (colon && idx < count) {
                *colon = '\0';
                names[idx] = lsm_arena_strdup(arena, start);
                types[idx] = lsm_parse_return_type_text(arena, colon + 1, def_module_qn);
                idx++;
            }
            if (save == '\0') break;
            start = p + 1;
        }
    }
    names[idx] = NULL;
    types[idx] = NULL;

    if (idx > 0) {
        // Find the registered type and update field info
        for (int ti = 0; ti < reg->type_count; ti++) {
            if (reg->types[ti].qualified_name &&
                strcmp(reg->types[ti].qualified_name, type_qn) == 0) {
                reg->types[ti].field_names = names;
                reg->types[ti].field_types = types;
                break;
            }
        }
    }
}

// --- Phase 1c: Extract type parameters from generic function declarations ---

// Helper: parse a tree-sitter type AST node with type param awareness.
// Like go_parse_type_node but checks type_identifier against type_param_names first,
// and works without a full GoLSPContext (uses raw arena/source/module_qn).
static const LSMType* parse_type_node_with_params(LSMArena* arena, TSNode node,
    const char* source, const char* module_qn, const char** type_param_names) {
    if (ts_node_is_null(node)) return lsm_type_unknown();

    const char* kind = ts_node_type(node);

    // type_identifier: check type params first, then resolve as named type
    if (strcmp(kind, "type_identifier") == 0) {
        char* name = lsm_node_text(arena, node, source);
        if (!name) return lsm_type_unknown();
        // Check type param names
        if (type_param_names) {
            for (int i = 0; type_param_names[i]; i++) {
                if (strcmp(name, type_param_names[i]) == 0) {
                    return lsm_type_type_param(arena, name);
                }
            }
        }
        // Builtin types
        if (strcmp(name, "int") == 0 || strcmp(name, "string") == 0 ||
            strcmp(name, "bool") == 0 || strcmp(name, "float64") == 0 ||
            strcmp(name, "float32") == 0 || strcmp(name, "byte") == 0 ||
            strcmp(name, "rune") == 0 || strcmp(name, "error") == 0 ||
            strcmp(name, "any") == 0 || strcmp(name, "int8") == 0 ||
            strcmp(name, "int16") == 0 || strcmp(name, "int32") == 0 ||
            strcmp(name, "int64") == 0 || strcmp(name, "uint") == 0 ||
            strcmp(name, "uint8") == 0 || strcmp(name, "uint16") == 0 ||
            strcmp(name, "uint32") == 0 || strcmp(name, "uint64") == 0 ||
            strcmp(name, "uintptr") == 0) {
            return lsm_type_builtin(arena, name);
        }
        return lsm_type_named(arena, lsm_arena_sprintf(arena, "%s.%s", module_qn, name));
    }

    // type_elem: wrapper in type_arguments
    if (strcmp(kind, "type_elem") == 0 && ts_node_named_child_count(node) > 0) {
        return parse_type_node_with_params(arena, ts_node_named_child(node, 0),
            source, module_qn, type_param_names);
    }

    // qualified_type: pkg.Type
    if (strcmp(kind, "qualified_type") == 0) {
        TSNode pkg_node = ts_node_child_by_field_name(node, "package", 7);
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(pkg_node) && !ts_node_is_null(name_node)) {
            char* pkg = lsm_node_text(arena, pkg_node, source);
            char* name = lsm_node_text(arena, name_node, source);
            if (pkg && name) {
                return lsm_type_named(arena, lsm_arena_sprintf(arena, "%s.%s", pkg, name));
            }
        }
        return lsm_type_unknown();
    }

    // pointer_type: *T
    if (strcmp(kind, "pointer_type") == 0) {
        uint32_t nc = ts_node_named_child_count(node);
        if (nc > 0)
            return lsm_type_pointer(arena, parse_type_node_with_params(arena,
                ts_node_named_child(node, nc - 1), source, module_qn, type_param_names));
        return lsm_type_unknown();
    }

    // slice_type: []T
    if (strcmp(kind, "slice_type") == 0) {
        TSNode elem = ts_node_child_by_field_name(node, "element", 7);
        if (ts_node_is_null(elem) && ts_node_named_child_count(node) > 0)
            elem = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        return lsm_type_slice(arena, parse_type_node_with_params(arena,
            elem, source, module_qn, type_param_names));
    }

    // array_type: [N]T
    if (strcmp(kind, "array_type") == 0) {
        TSNode elem = ts_node_child_by_field_name(node, "element", 7);
        if (ts_node_is_null(elem) && ts_node_named_child_count(node) > 0)
            elem = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        return lsm_type_slice(arena, parse_type_node_with_params(arena,
            elem, source, module_qn, type_param_names));
    }

    // map_type: map[K]V
    if (strcmp(kind, "map_type") == 0) {
        TSNode key = ts_node_child_by_field_name(node, "key", 3);
        TSNode value = ts_node_child_by_field_name(node, "value", 5);
        return lsm_type_map(arena,
            parse_type_node_with_params(arena, key, source, module_qn, type_param_names),
            parse_type_node_with_params(arena, value, source, module_qn, type_param_names));
    }

    // channel_type: chan T
    if (strcmp(kind, "channel_type") == 0) {
        TSNode value = ts_node_child_by_field_name(node, "value", 5);
        if (ts_node_is_null(value) && ts_node_named_child_count(node) > 0)
            value = ts_node_named_child(node, ts_node_named_child_count(node) - 1);
        char* text = lsm_node_text(arena, node, source);
        int dir = 0;
        if (text) {
            if (strncmp(text, "chan<-", 6) == 0 || strncmp(text, "chan <-", 7) == 0) dir = 1;
            else if (strncmp(text, "<-chan", 6) == 0 || strncmp(text, "<- chan", 7) == 0) dir = 2;
        }
        return lsm_type_channel(arena, parse_type_node_with_params(arena,
            value, source, module_qn, type_param_names), dir);
    }

    // function_type: func(T) R — full parsing with type param awareness
    if (strcmp(kind, "function_type") == 0) {
        TSNode params_node = ts_node_child_by_field_name(node, "parameters", 10);
        TSNode result_node = ts_node_child_by_field_name(node, "result", 6);

        const LSMType* param_types_arr[16];
        int pc = 0;
        if (!ts_node_is_null(params_node)) {
            uint32_t pnc = ts_node_child_count(params_node);
            for (uint32_t i = 0; i < pnc && pc < 15; i++) {
                TSNode child = ts_node_child(params_node, i);
                if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
                if (strcmp(ts_node_type(child), "parameter_declaration") == 0) {
                    TSNode pt = ts_node_child_by_field_name(child, "type", 4);
                    if (!ts_node_is_null(pt))
                        param_types_arr[pc++] = parse_type_node_with_params(arena,
                            pt, source, module_qn, type_param_names);
                }
            }
        }
        param_types_arr[pc] = NULL;

        const LSMType* ret_types_arr[16];
        int rc = 0;
        if (!ts_node_is_null(result_node)) {
            if (strcmp(ts_node_type(result_node), "parameter_list") == 0) {
                uint32_t rnc = ts_node_child_count(result_node);
                for (uint32_t i = 0; i < rnc && rc < 15; i++) {
                    TSNode child = ts_node_child(result_node, i);
                    if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
                    TSNode rt = ts_node_child_by_field_name(child, "type", 4);
                    if (ts_node_is_null(rt)) rt = child;
                    ret_types_arr[rc++] = parse_type_node_with_params(arena,
                        rt, source, module_qn, type_param_names);
                }
            } else {
                ret_types_arr[rc++] = parse_type_node_with_params(arena,
                    result_node, source, module_qn, type_param_names);
            }
        }
        ret_types_arr[rc] = NULL;

        const LSMType** pt = pc > 0 ? (const LSMType**)param_types_arr : NULL;
        const LSMType** rt = rc > 0 ? (const LSMType**)ret_types_arr : NULL;
        return lsm_type_func(arena, NULL, pt, rt);
    }

    // interface_type
    if (strcmp(kind, "interface_type") == 0) {
        LSMType* t = (LSMType*)lsm_arena_alloc(arena, sizeof(LSMType));
        memset(t, 0, sizeof(LSMType));
        t->kind = LSM_TYPE_INTERFACE;
        return t;
    }

    // parenthesized_type: (T)
    if (strcmp(kind, "parenthesized_type") == 0 && ts_node_named_child_count(node) > 0) {
        return parse_type_node_with_params(arena, ts_node_named_child(node, 0),
            source, module_qn, type_param_names);
    }

    // generic_type: Type[T1, T2]
    if (strcmp(kind, "generic_type") == 0) {
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        if (!ts_node_is_null(type_node))
            return parse_type_node_with_params(arena, type_node, source, module_qn, type_param_names);
    }

    return lsm_type_unknown();
}

/* Go permits one AST parameter declaration to introduce several call
 * positions: `first, second T`. Count direct names without exposing or
 * rewriting the legacy public param_names projection. */
static int lsm_go_lsp_signature_param_multiplicity(TSNode param) {
    const char *kind = ts_node_type(param);
    if (strcmp(kind, "parameter_declaration") != 0 &&
        strcmp(kind, "variadic_parameter_declaration") != 0) {
        return 1;
    }

    int field_names = 0;
    int identifier_names = 0;
    uint32_t child_count = ts_node_child_count(param);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(param, i);
        if (ts_node_is_null(child) || !ts_node_is_named(child))
            continue;
        const char *field = ts_node_field_name_for_child(param, i);
        if (field && strcmp(field, "name") == 0)
            field_names++;
        if (strcmp(ts_node_type(child), "identifier") == 0)
            identifier_names++;
    }

    int names = field_names > 0 ? field_names : identifier_names;
    return names > 0 ? names : 1;
}

static const LSMType *lsm_go_lsp_keep_known_refinement(const LSMType *candidate,
                                                       const LSMType *existing) {
    if (lsm_type_is_unknown(candidate) && !lsm_type_is_unknown(existing)) {
        return existing;
    }
    return candidate ? candidate : lsm_type_unknown();
}

// Scan AST for generic function declarations and set type_param_names + re-parse
// return types on matching registered functions.
static void extract_type_params_from_ast(LSMArena* arena, LSMTypeRegistry* reg,
    TSNode root, const char* source, const char* module_qn) {

    // Collect top-level children once (O(n)); ts_node_child(root,i) is O(i) → O(n²).
    uint32_t rkn = 0;
    TSNode* rkids = lsm_lsp_collect_children(arena, root, &rkn);
    for (uint32_t i = 0; i < rkn; i++) {
        TSNode top = rkids[i];
        if (strcmp(ts_node_type(top), "function_declaration") != 0) continue;

        // Check for type_parameter_list (field name: "type_parameters", 15 chars)
        TSNode tp_list = ts_node_child_by_field_name(top, "type_parameters", 15);
        if (ts_node_is_null(tp_list)) continue;

        // Get function name
        TSNode name_node = ts_node_child_by_field_name(top, "name", 4);
        if (ts_node_is_null(name_node)) continue;
        char* func_name = lsm_node_text(arena, name_node, source);
        if (!func_name || !func_name[0]) continue;

        // Extract type param names from type_parameter_declaration children
        const char* params[16];
        int param_count = 0;
        uint32_t tp_nc = ts_node_child_count(tp_list);
        for (uint32_t j = 0; j < tp_nc && param_count < 15; j++) {
            TSNode child = ts_node_child(tp_list, j);
            if (ts_node_is_null(child) || !ts_node_is_named(child)) continue;
            if (strcmp(ts_node_type(child), "type_parameter_declaration") != 0) continue;
            TSNode pname = ts_node_child_by_field_name(child, "name", 4);
            if (ts_node_is_null(pname)) {
                // Fallback: first type_identifier child
                uint32_t cc = ts_node_child_count(child);
                for (uint32_t k = 0; k < cc; k++) {
                    TSNode c = ts_node_child(child, k);
                    if (!ts_node_is_null(c) && ts_node_is_named(c) &&
                        strcmp(ts_node_type(c), "type_identifier") == 0) {
                        pname = c;
                        break;
                    }
                }
            }
            if (!ts_node_is_null(pname)) {
                char* pn = lsm_node_text(arena, pname, source);
                if (pn && pn[0]) {
                    params[param_count++] = lsm_arena_strdup(arena, pn);
                }
            }
        }
        if (param_count == 0) continue;
        params[param_count] = NULL;

        // Build arena-allocated type_param_names array
        const char** tp_names = (const char**)lsm_arena_alloc(arena, (param_count + 1) * sizeof(const char*));
        for (int j = 0; j <= param_count; j++) tp_names[j] = params[j];

        // Find the matching registered function and set type_param_names
        const char* func_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, func_name);
        for (int fi = 0; fi < reg->func_count; fi++) {
            if (reg->funcs[fi].qualified_name &&
                strcmp(reg->funcs[fi].qualified_name, func_qn) == 0) {
                reg->funcs[fi].type_param_names = tp_names;

                // Re-parse return types with type param awareness
                if (reg->funcs[fi].signature && reg->funcs[fi].signature->kind == LSM_TYPE_FUNC &&
                    reg->funcs[fi].signature->data.func.return_types) {
                    const LSMType** old_rets = reg->funcs[fi].signature->data.func.return_types;
                    int ret_count = 0;
                    while (old_rets[ret_count]) ret_count++;

                    // Check if any return type is a NAMED that matches a type param
                    bool needs_reparse = false;
                    for (int ri = 0; ri < ret_count && !needs_reparse; ri++) {
                        const LSMType* check = old_rets[ri];
                        // Unwrap slice/pointer to get inner named type
                        if (check->kind == LSM_TYPE_SLICE && check->data.slice.elem)
                            check = check->data.slice.elem;
                        else if (check->kind == LSM_TYPE_POINTER && check->data.pointer.elem)
                            check = check->data.pointer.elem;
                        if (check->kind == LSM_TYPE_NAMED) {
                            const char* qn = check->data.named.qualified_name;
                            const char* dot = strrchr(qn, '.');
                            const char* short_name = dot ? dot + 1 : qn;
                            for (int pi = 0; pi < param_count; pi++) {
                                if (strcmp(short_name, tp_names[pi]) == 0) {
                                    needs_reparse = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (needs_reparse) {
                        TSNode result_node = ts_node_child_by_field_name(top, "result", 6);
                        if (!ts_node_is_null(result_node)) {
                            const LSMType** new_rets = (const LSMType**)lsm_arena_alloc(arena,
                                (ret_count + 1) * sizeof(const LSMType*));

                            if (strcmp(ts_node_type(result_node), "parameter_list") == 0) {
                                int idx = 0;
                                uint32_t rnc = ts_node_child_count(result_node);
                                for (uint32_t ri = 0; ri < rnc && idx < ret_count; ri++) {
                                    TSNode rchild = ts_node_child(result_node, ri);
                                    if (ts_node_is_null(rchild) || !ts_node_is_named(rchild)) continue;
                                    TSNode rtype = ts_node_child_by_field_name(rchild, "type", 4);
                                    if (ts_node_is_null(rtype)) rtype = rchild;
                                    const LSMType *parsed = parse_type_node_with_params(
                                        arena, rtype, source, module_qn, tp_names);
                                    new_rets[idx] =
                                        lsm_go_lsp_keep_known_refinement(parsed, old_rets[idx]);
                                    idx++;
                                }
                                while (idx < ret_count) {
                                    new_rets[idx] = old_rets[idx];
                                    idx++;
                                }
                                new_rets[ret_count] = NULL;
                            } else {
                                const LSMType *parsed = parse_type_node_with_params(
                                    arena, result_node, source, module_qn, tp_names);
                                new_rets[0] = lsm_go_lsp_keep_known_refinement(parsed, old_rets[0]);
                                for (int idx = 1; idx < ret_count; idx++) {
                                    new_rets[idx] = old_rets[idx];
                                }
                                new_rets[ret_count] = NULL;
                            }

                            const LSMType *new_sig = lsm_type_func_replace_returns(
                                arena, reg->funcs[fi].signature, new_rets);
                            if (new_sig && new_sig->kind == LSM_TYPE_FUNC) {
                                reg->funcs[fi].signature = new_sig;
                            }
                        }
                    }
                }

                // Also re-parse param types with TYPE_PARAM awareness (for implicit generics)
                if (reg->funcs[fi].signature && reg->funcs[fi].signature->kind == LSM_TYPE_FUNC &&
                    reg->funcs[fi].signature->data.func.param_types) {
                    const LSMType** old_params = reg->funcs[fi].signature->data.func.param_types;
                    int pc = 0;
                    while (old_params[pc]) pc++;
                    // Re-parse from AST parameter list
                    TSNode params_node = ts_node_child_by_field_name(top, "parameters", 10);
                    if (!ts_node_is_null(params_node)) {
                        const LSMType** new_params = (const LSMType**)lsm_arena_alloc(arena,
                            (pc + 1) * sizeof(const LSMType*));
                        int idx = 0;
                        bool overflow = false;
                        uint32_t pnc = ts_node_child_count(params_node);
                        for (uint32_t pi = 0; pi < pnc && !overflow; pi++) {
                            TSNode pchild = ts_node_child(params_node, pi);
                            if (ts_node_is_null(pchild) || !ts_node_is_named(pchild)) continue;
                            const char *param_kind = ts_node_type(pchild);
                            if (strcmp(param_kind, "parameter_declaration") != 0 &&
                                strcmp(param_kind, "variadic_parameter_declaration") != 0) {
                                continue;
                            }
                            TSNode ptype = ts_node_child_by_field_name(pchild, "type", 4);
                            const LSMType *parsed =
                                ts_node_is_null(ptype)
                                    ? lsm_type_unknown()
                                    : parse_type_node_with_params(arena, ptype, source, module_qn,
                                                                  tp_names);
                            int multiplicity = lsm_go_lsp_signature_param_multiplicity(pchild);
                            for (int occurrence = 0; occurrence < multiplicity; occurrence++) {
                                if (idx >= pc) {
                                    overflow = true;
                                    break;
                                }
                                new_params[idx] =
                                    lsm_go_lsp_keep_known_refinement(parsed, old_params[idx]);
                                idx++;
                            }
                        }
                        new_params[idx] = NULL;
                        if (!overflow && idx == pc) {
                            const LSMType *old_sig = reg->funcs[fi].signature;
                            const LSMType *new_sig =
                                lsm_type_func(arena, old_sig->data.func.param_names, new_params,
                                              old_sig->data.func.return_types);
                            if (new_sig && new_sig->kind == LSM_TYPE_FUNC) {
                                reg->funcs[fi].signature = new_sig;
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}

// Forward: tree-sitter Go language (from lang_specs.c)
extern const TSLanguage* tree_sitter_go(void);

void lsm_run_go_lsp_cross(
    LSMArena* arena,
    const char* source, int source_len,
    const char* module_qn,
    LSMLSPDef* defs, int def_count,
    const char** import_names, const char** import_qns, int import_count,
    TSTree* cached_tree,
    LSMResolvedCallArray* out)
{
    if (!source || source_len <= 0) return;

    // 1. Use cached tree if available, otherwise parse fresh
    TSParser* parser = NULL;
    TSTree* tree = cached_tree;
    bool owns_tree = false;
    if (!tree) {
        parser = ts_parser_new();
        if (!parser) return;
        ts_parser_set_language(parser, tree_sitter_go());
        tree = ts_parser_parse_string(parser, NULL, source, source_len);
        owns_tree = true;
        if (!tree) { ts_parser_delete(parser); return; }
    }
    TSNode root = ts_tree_root_node(tree);

    // 2. Build registry
    LSMTypeRegistry reg;
    lsm_registry_init(&reg, arena);
    lsm_go_stdlib_register(&reg, arena);

    // Register all defs (file-local + cross-file).
    // Perf: borrow strings from defs[] directly — they live in the
    // caller's arena which outlives this call, so lsm_arena_strdup is
    // wasted work. On kubernetes (~110k defs × 11k files × 5 strdups
    // ~= 6B mallocs) this alone saves ~90s of resolve wall time.
    for (int i = 0; i < def_count; i++) {
        LSMLSPDef* d = &defs[i];
        if (!d->qualified_name || !d->short_name || !d->label) continue;

        const char* def_mod = d->def_module_qn ? d->def_module_qn : module_qn;

        // Every type-like container (Type/Class/Struct/Interface/Enum/Trait).
        // Struct included so Go structs (now labelled "Struct") register as types.
        if (lsm_label_is_type_like(d->label)) {
            LSMRegisteredType rt;
            memset(&rt, 0, sizeof(rt));
            rt.qualified_name = d->qualified_name;  // borrowed
            rt.short_name = d->short_name;          // borrowed
            rt.is_interface = d->is_interface || strcmp(d->label, "Interface") == 0;
            rt.embedded_types = split_pipe_strings(arena, d->embedded_types);

            // Set method_names for interfaces from "|"-separated string
            if (rt.is_interface && d->method_names_str && d->method_names_str[0]) {
                rt.method_names = split_pipe_strings(arena, d->method_names_str);
            }

            lsm_registry_add_type(&reg, rt);

            // Populate struct field types from field_defs
            if (d->field_defs && d->field_defs[0]) {
                parse_field_defs_into_type(arena, &reg, rt.qualified_name, d->field_defs, def_mod);
            }
        }

        // Function/Method
        if (strcmp(d->label, "Function") == 0 || strcmp(d->label, "Method") == 0) {
            LSMRegisteredFunc rf;
            memset(&rf, 0, sizeof(rf));
            rf.qualified_name = d->qualified_name;  // borrowed
            rf.short_name = d->short_name;          // borrowed

            // Build FUNC type from return_types text
            const LSMType** ret_types = split_pipe_types(arena, d->return_types, def_mod);
            LSMGoLSPSignatureParserContext param_parser_ctx = {def_mod};
            const LSMType **param_types = lsm_type_materialize_signature_params(
                arena, d->signature_param_types, d->signature_param_count,
                lsm_go_lsp_parse_signature_param_text, &param_parser_ctx);
            rf.signature = lsm_type_func(arena, NULL, param_types, ret_types);

            // Method receiver
            if (strcmp(d->label, "Method") == 0 && d->receiver_type && d->receiver_type[0]) {
                rf.receiver_type = d->receiver_type;  // borrowed
                // Auto-create type entry if not exists
                if (!lsm_registry_lookup_type(&reg, rf.receiver_type)) {
                    LSMRegisteredType auto_type;
                    memset(&auto_type, 0, sizeof(auto_type));
                    auto_type.qualified_name = rf.receiver_type;
                    const char* dot = strrchr(d->receiver_type, '.');
                    auto_type.short_name = dot ? dot + 1 : rf.receiver_type;  // borrowed substring
                    lsm_registry_add_type(&reg, auto_type);
                }
            }

            lsm_registry_add_func(&reg, rf);
        }
    }

    // 3. Phase 1b: Scan AST for struct definitions to populate embedded_types
    {
        // Collect top-level children once (O(n)); ts_node_child(root,i) is O(i) → O(n²).
        uint32_t rkn = 0;
        TSNode* rkids = lsm_lsp_collect_children(arena, root, &rkn);
        for (uint32_t i = 0; i < rkn; i++) {
            TSNode top = rkids[i];
            if (strcmp(ts_node_type(top), "type_declaration") != 0) continue;
            uint32_t td_nc = ts_node_child_count(top);
            for (uint32_t j = 0; j < td_nc; j++) {
                TSNode spec = ts_node_child(top, j);
                if (ts_node_is_null(spec) || !ts_node_is_named(spec)) continue;
                const char* spec_kind2 = ts_node_type(spec);

                // type_alias: type Foo = Bar
                if (strcmp(spec_kind2, "type_alias") == 0) {
                    TSNode alias_name = ts_node_child_by_field_name(spec, "name", 4);
                    TSNode alias_type = ts_node_child_by_field_name(spec, "type", 4);
                    if (!ts_node_is_null(alias_name) && !ts_node_is_null(alias_type)) {
                        char* aname = lsm_node_text(arena, alias_name, source);
                        char* atarget = lsm_node_text(arena, alias_type, source);
                        if (aname && aname[0] && atarget && atarget[0]) {
                            const char* alias_type_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, aname);
                            const char* alias_target_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, atarget);
                            bool found_a = false;
                            for (int ti = 0; ti < reg.type_count; ti++) {
                                if (reg.types[ti].qualified_name &&
                                    strcmp(reg.types[ti].qualified_name, alias_type_qn) == 0) {
                                    reg.types[ti].alias_of = alias_target_qn;
                                    found_a = true;
                                    break;
                                }
                            }
                            if (!found_a) {
                                LSMRegisteredType alias_rt;
                                memset(&alias_rt, 0, sizeof(alias_rt));
                                alias_rt.qualified_name = alias_type_qn;
                                alias_rt.short_name = aname;
                                alias_rt.alias_of = alias_target_qn;
                                lsm_registry_add_type(&reg, alias_rt);
                            }
                        }
                    }
                    continue;
                }

                if (strcmp(spec_kind2, "type_spec") != 0) continue;

                TSNode name_node = ts_node_child_by_field_name(spec, "name", 4);
                TSNode type_node = ts_node_child_by_field_name(spec, "type", 4);
                if (ts_node_is_null(name_node) || ts_node_is_null(type_node)) continue;
                if (strcmp(ts_node_type(type_node), "struct_type") != 0) continue;

                char* type_name = lsm_node_text(arena, name_node, source);
                if (!type_name || !type_name[0]) continue;
                const char* type_qn = lsm_arena_sprintf(arena, "%s.%s", module_qn, type_name);

                TSNode field_list = ts_node_child_by_field_name(type_node, "body", 4);
                if (ts_node_is_null(field_list)) {
                    if (ts_node_named_child_count(type_node) > 0)
                        field_list = ts_node_named_child(type_node, 0);
                }
                if (ts_node_is_null(field_list)) continue;

                const char* embeds[16];
                int embed_count = 0;
                const char* fld_names[64];
                const LSMType* fld_types[64];
                int fld_count = 0;

                GoLSPContext tmp_ctx;
                memset(&tmp_ctx, 0, sizeof(tmp_ctx));
                tmp_ctx.arena = arena;
                tmp_ctx.source = source;
                tmp_ctx.source_len = source_len;
                tmp_ctx.registry = &reg;
                tmp_ctx.package_qn = module_qn;
                tmp_ctx.import_local_names = import_names;
                tmp_ctx.import_package_qns = import_qns;
                tmp_ctx.import_count = import_count;

                uint32_t fl_nc = ts_node_child_count(field_list);
                for (uint32_t k = 0; k < fl_nc; k++) {
                    TSNode field = ts_node_child(field_list, k);
                    if (ts_node_is_null(field) || !ts_node_is_named(field)) continue;
                    if (strcmp(ts_node_type(field), "field_declaration") != 0) continue;
                    TSNode fname = ts_node_child_by_field_name(field, "name", 4);
                    TSNode ftype = ts_node_child_by_field_name(field, "type", 4);
                    if (ts_node_is_null(fname) && !ts_node_is_null(ftype)) {
                        if (embed_count < 15) {
                            char* embed_text = lsm_node_text(arena, ftype, source);
                            if (embed_text && embed_text[0]) {
                                const char* et = embed_text;
                                while (*et == '*') et++;
                                embeds[embed_count++] = lsm_arena_sprintf(arena, "%s.%s", module_qn, et);
                            }
                        }
                    } else if (!ts_node_is_null(fname) && !ts_node_is_null(ftype) && fld_count < 63) {
                        char* fn = lsm_node_text(arena, fname, source);
                        if (fn && fn[0]) {
                            fld_names[fld_count] = fn;
                            fld_types[fld_count] = go_parse_type_node(&tmp_ctx, ftype);
                            fld_count++;
                        }
                    }
                }

                for (int ti = 0; ti < reg.type_count; ti++) {
                    if (!reg.types[ti].qualified_name ||
                        strcmp(reg.types[ti].qualified_name, type_qn) != 0) continue;

                    if (embed_count > 0) {
                        const char** arr = (const char**)lsm_arena_alloc(arena,
                            (embed_count + 1) * sizeof(const char*));
                        for (int ei = 0; ei < embed_count; ei++) arr[ei] = embeds[ei];
                        arr[embed_count] = NULL;
                        reg.types[ti].embedded_types = arr;
                    }
                    if (fld_count > 0) {
                        const char** names = (const char**)lsm_arena_alloc(arena,
                            (fld_count + 1) * sizeof(const char*));
                        const LSMType** types = (const LSMType**)lsm_arena_alloc(arena,
                            (fld_count + 1) * sizeof(const LSMType*));
                        for (int fi = 0; fi < fld_count; fi++) {
                            names[fi] = fld_names[fi];
                            types[fi] = fld_types[fi];
                        }
                        names[fld_count] = NULL;
                        types[fld_count] = NULL;
                        reg.types[ti].field_names = names;
                        reg.types[ti].field_types = types;
                    }
                    break;
                }
            }
        }
    }

    // 3b. Phase 1c: Extract type params from generic function declarations
    extract_type_params_from_ast(arena, &reg, root, source, module_qn);

    // 3c. Finalize registry — builds hash buckets for O(1) lookups in
    // lsm_registry_lookup_method / lookup_type. Without this, every
    // lookup falls through to the O(N) linear scan in type_registry.c,
    // turning the resolution pass below into an O(N·C·F) cost per file
    // (kubernetes: ~64B strcmps observed = 35min). Must come AFTER all
    // mutations (Phase 1b/1c above) and BEFORE the LSP context init.
    lsm_registry_finalize(&reg);

    // 4. Build LSP context and run
    GoLSPContext ctx;
    go_lsp_init(&ctx, arena, source, source_len, &reg, module_qn, out);

    for (int i = 0; i < import_count; i++) {
        if (import_names[i] && import_qns[i]) {
            go_lsp_add_import(&ctx, import_names[i], import_qns[i]);
        }
    }

    go_lsp_process_file(&ctx, root);

    // 5. Cleanup — only free if we allocated
    if (owns_tree) {
        ts_tree_delete(tree);
        if (parser) ts_parser_delete(parser);
    }
}

/* ── Tier 2: pre-built per-language registry (gopls package summary pattern) ──
 *
 * lsm_go_build_cross_registry runs the registry build ONCE per project (in
 * pipeline.c, before the parallel resolve workers fire). The returned
 * registry is finalized (O(1) lookups) and shared READ-ONLY across all
 * workers in the resolve phase.
 *
 * lsm_run_go_lsp_cross_with_registry is the per-file entrypoint used by
 * the resolve worker. It SKIPS the registry build (uses the pre-built reg)
 * AND skips Phase 1b/1c AST scans (those mutate the registry, which would
 * race with other workers reading the shared reg). Accuracy trade-off:
 * file-local type aliases and struct embeddings discovered ONLY by Phase
 * 1b (and not already in defs[] embedded_types/field_defs strings) are
 * missed. In practice the per-file LSP during extract already captures
 * those via the def strings. */
LSMTypeRegistry* lsm_go_build_cross_registry(
    LSMArena* arena, LSMLSPDef* defs, int def_count) {
    if (!arena) return NULL;
    LSMTypeRegistry* reg = (LSMTypeRegistry*)lsm_arena_alloc(arena, sizeof(*reg));
    if (!reg) return NULL;
    lsm_registry_init(reg, arena);
    lsm_go_stdlib_register(reg, arena);

    for (int i = 0; i < def_count; i++) {
        LSMLSPDef* d = &defs[i];
        if (!d->qualified_name || !d->short_name || !d->label) continue;
        /* Filter to Go defs only — the all_defs[] array is mixed-language. */
        if (d->lang != LSM_LANG_GO) continue;
        /* In the pre-built path every def carries its own def_module_qn
         * (set by lsm_pxc_collect_all_defs). There is no caller module to
         * fall back to — this registry is project-wide, not per-file. */
        const char* def_mod = d->def_module_qn ? d->def_module_qn : "";

        // Every type-like container (Type/Class/Struct/Interface/Enum/Trait).
        // Struct included so Go structs (now labelled "Struct") register as types.
        if (lsm_label_is_type_like(d->label)) {
            LSMRegisteredType rt;
            memset(&rt, 0, sizeof(rt));
            rt.qualified_name = d->qualified_name; /* borrowed */
            rt.short_name = d->short_name;
            rt.is_interface = d->is_interface || strcmp(d->label, "Interface") == 0;
            rt.embedded_types = split_pipe_strings(arena, d->embedded_types);
            if (rt.is_interface && d->method_names_str && d->method_names_str[0]) {
                rt.method_names = split_pipe_strings(arena, d->method_names_str);
            }
            lsm_registry_add_type(reg, rt);
            if (d->field_defs && d->field_defs[0]) {
                parse_field_defs_into_type(arena, reg, rt.qualified_name,
                                           d->field_defs, def_mod);
            }
        }

        if (strcmp(d->label, "Function") == 0 || strcmp(d->label, "Method") == 0) {
            LSMRegisteredFunc rf;
            memset(&rf, 0, sizeof(rf));
            rf.qualified_name = d->qualified_name; /* borrowed */
            rf.short_name = d->short_name;
            const LSMType** ret_types = split_pipe_types(arena, d->return_types, def_mod);
            LSMGoLSPSignatureParserContext param_parser_ctx = {def_mod};
            const LSMType **param_types = lsm_type_materialize_signature_params(
                arena, d->signature_param_types, d->signature_param_count,
                lsm_go_lsp_parse_signature_param_text, &param_parser_ctx);
            rf.signature = lsm_type_func(arena, NULL, param_types, ret_types);
            if (strcmp(d->label, "Method") == 0 && d->receiver_type && d->receiver_type[0]) {
                rf.receiver_type = d->receiver_type;
                if (!lsm_registry_lookup_type(reg, rf.receiver_type)) {
                    LSMRegisteredType auto_type;
                    memset(&auto_type, 0, sizeof(auto_type));
                    auto_type.qualified_name = rf.receiver_type;
                    const char* dot = strrchr(d->receiver_type, '.');
                    auto_type.short_name = dot ? dot + 1 : rf.receiver_type;
                    lsm_registry_add_type(reg, auto_type);
                }
            }
            lsm_registry_add_func(reg, rf);
        }
    }

    lsm_registry_finalize(reg);
    reg->read_only = true; /* seal: shared Tier-2 registry is read-only during resolve */
    return reg;
}

void lsm_run_go_lsp_cross_with_registry(
    LSMArena* arena,
    const char* source, int source_len,
    const char* module_qn,
    LSMTypeRegistry* reg,
    const char** import_names, const char** import_qns, int import_count,
    TSTree* cached_tree,
    LSMResolvedCallArray* out) {
    if (!source || source_len <= 0 || !reg) return;

    TSParser* parser = NULL;
    TSTree* tree = cached_tree;
    bool owns_tree = false;
    if (!tree) {
        parser = ts_parser_new();
        if (!parser) return;
        ts_parser_set_language(parser, tree_sitter_go());
        tree = ts_parser_parse_string(parser, NULL, source, source_len);
        owns_tree = true;
        if (!tree) { ts_parser_delete(parser); return; }
    }
    TSNode root = ts_tree_root_node(tree);

    /* Phase 1b/1c (per-file AST mutations on registry) DELIBERATELY SKIPPED —
     * shared registry must stay immutable across parallel workers. */

    GoLSPContext ctx;
    go_lsp_init(&ctx, arena, source, source_len, reg, module_qn, out);
    for (int i = 0; i < import_count; i++) {
        if (import_names[i] && import_qns[i]) {
            go_lsp_add_import(&ctx, import_names[i], import_qns[i]);
        }
    }
    go_lsp_process_file(&ctx, root);

    if (owns_tree) {
        ts_tree_delete(tree);
        if (parser) ts_parser_delete(parser);
    }
}

/* ── Tier 3: AST-walk-free metadata-driven cross-file resolver ────
 *
 * KEY INSIGHT: the per-file LSP during extract ALREADY emits one
 * LSMResolvedCall per call site, with strategy="lsp_unresolved" for
 * calls it couldn't resolve. For those unresolved entries:
 *
 *   * "symbol_not_in_registry" (go_lsp.c:1142) → callee_qn is
 *     "pkg_name.field_name" (literal local import alias)
 *   * "method_not_found" (go_lsp.c:1242) → callee_qn is
 *     "<FullyQualifiedReceiverTypeQN>.<MethodName>" — receiver type
 *     was ALREADY inferred by per-file LSP, just the method wasn't
 *     in the per-file registry (cross-file)
 *   * "function_not_in_registry" (go_lsp.c:1266) → callee_qn is
 *     the bare function name (likely a same-package symbol we
 *     missed, or a cross-file package function used unqualified)
 *   * "unknown_receiver_type" (go_lsp.c:1248) → per-file LSP
 *     couldn't infer the receiver type. Cross-LSP can't either
 *     without re-walking — leave unresolved.
 *
 * So cross-LSP work reduces to:
 *   for each unresolved entry → look up callee_qn (or pkg.x via
 *   import map) in the global registry → emit resolved version
 *   on top. NO TREE-SITTER PARSE. NO AST WALK. Just hash lookups.
 *
 * This is what the user meant: walk the AST ONCE (during extract),
 * capture everything cross-LSP needs as metadata, and let cross-LSP
 * be a pure lookup pass. */
int lsm_go_fast_resolve_qualified_calls(
    LSMFileResult* result,
    LSMTypeRegistry* reg,
    const char** import_names, const char** import_qns, int import_count) {
    if (!result || !reg) return 0;
    /* Snapshot the count: we append to resolved_calls inside the loop,
     * but only want to scan the original unresolved entries. */
    int initial_count = result->resolved_calls.count;
    int newly_resolved = 0;

    for (int i = 0; i < initial_count; i++) {
        const LSMResolvedCall* uc = &result->resolved_calls.items[i];
        if (!uc->strategy || !uc->callee_qn || !uc->caller_qn) continue;
        if (strcmp(uc->strategy, "lsp_unresolved") != 0) continue;

        /* Case 1: callee_qn is already a fully-qualified type/pkg
         * symbol that per-file LSP couldn't find in its local registry
         * but the global registry has. (method_not_found with NAMED
         * receiver, or any other case where callee_qn looks like a
         * full QN). Direct lookup. */
        const LSMRegisteredFunc* f = lsm_registry_lookup_func(reg, uc->callee_qn);

        /* Case 2: callee_qn is "local_pkg_alias.suffix" — the alias
         * needs translating through the file's import map to get the
         * real module QN. */
        if (!f) {
            const char* dot = strchr(uc->callee_qn, '.');
            if (dot) {
                size_t prefix_len = (size_t)(dot - uc->callee_qn);
                if (prefix_len > 0 && prefix_len < 256) {
                    char prefix[256];
                    memcpy(prefix, uc->callee_qn, prefix_len);
                    prefix[prefix_len] = '\0';
                    const char* suffix = dot + 1;
                    for (int j = 0; j < import_count; j++) {
                        if (import_names[j] && import_qns[j] &&
                            strcmp(prefix, import_names[j]) == 0) {
                            char fq[1024];
                            int n = snprintf(fq, sizeof(fq), "%s.%s",
                                             import_qns[j], suffix);
                            if (n > 0 && n < (int)sizeof(fq)) {
                                f = lsm_registry_lookup_func(reg, fq);
                            }
                            break;
                        }
                    }
                }
            }
        }

        /* Case 3: an unqualified function value from another source file in
         * the same Go package. Go module QNs are directory-based, so this is
         * an exact package-local lookup rather than a project-wide short-name
         * guess. */
        if (!f && !strchr(uc->callee_qn, '.') && result->module_qn) {
            f = lsm_registry_lookup_symbol(reg, result->module_qn, uc->callee_qn);
        }

        if (!f) continue;

        /* Emit a resolved entry. lsm_pipeline_find_lsp_resolution
         * picks the highest-confidence match, so the unresolved entry
         * stays (harmless duplicate) but our resolved entry wins. */
        LSMResolvedCall rc = {0};
        rc.caller_qn = uc->caller_qn;
        rc.callee_qn = f->qualified_name; /* borrowed from pipeline arena */
        rc.strategy = "lsp_strategy_cross_file";
        rc.confidence = 0.92f;
        rc.reason = NULL;
        rc.kind = uc->kind;
        rc.site_start_byte = uc->site_start_byte;
        rc.site_end_byte = uc->site_end_byte;
        lsm_resolvedcall_push(&result->resolved_calls, &result->arena, rc);
        newly_resolved++;
    }

    /* Return value mirrors the old contract (count of items still
     * needing the slow path) but is always 0 now — caller should
     * unconditionally skip the slow path for Go. */
    (void)newly_resolved;
    return 0;
}

// --- Batch cross-file LSP ---

void lsm_batch_go_lsp_cross(
    LSMArena* arena,
    LSMBatchGoLSPFile* files, int file_count,
    LSMResolvedCallArray* out)
{
    if (!files || file_count <= 0 || !out) return;

    for (int f = 0; f < file_count; f++) {
        LSMBatchGoLSPFile* file = &files[f];
        memset(&out[f], 0, sizeof(LSMResolvedCallArray));

        if (!file->source || file->source_len <= 0 || file->def_count <= 0) continue;

        // Per-file arena: registry + temp data freed after each file
        LSMArena file_arena;
        lsm_arena_init(&file_arena);

        LSMResolvedCallArray file_out;
        memset(&file_out, 0, sizeof(file_out));

        // Delegate to existing per-file function
        lsm_run_go_lsp_cross(
            &file_arena,
            file->source, file->source_len,
            file->module_qn,
            file->defs, file->def_count,
            file->import_names, file->import_qns, file->import_count,
            file->cached_tree,
            &file_out);

        // Copy results to output arena (must outlive per-file arena)
        if (file_out.count > 0) {
            out[f].count = file_out.count;
            out[f].items = (LSMResolvedCall*)lsm_arena_alloc(arena,
                file_out.count * sizeof(LSMResolvedCall));
            for (int j = 0; j < file_out.count; j++) {
                LSMResolvedCall* src = &file_out.items[j];
                LSMResolvedCall* dst = &out[f].items[j];
                memset(dst, 0, sizeof(*dst));
                dst->caller_qn = src->caller_qn ? lsm_arena_strdup(arena, src->caller_qn) : NULL;
                dst->callee_qn = src->callee_qn ? lsm_arena_strdup(arena, src->callee_qn) : NULL;
                dst->strategy  = src->strategy  ? lsm_arena_strdup(arena, src->strategy)  : NULL;
                dst->confidence = src->confidence;
                dst->reason    = src->reason    ? lsm_arena_strdup(arena, src->reason)    : NULL;
                dst->kind = src->kind;
                dst->site_start_byte = src->site_start_byte;
                dst->site_end_byte = src->site_end_byte;
            }
        }

        lsm_arena_destroy(&file_arena);
    }
}
