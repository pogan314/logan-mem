/*
 * test_type_rep.c — Phase 1 type-rep extensions for Python LSP.
 *
 * Covers the five new kinds (UNION, LITERAL, PROTOCOL, MODULE, CALLABLE),
 * structural equality, union normalization (flatten + dedup + collapse),
 * Optional[T] sugar, and protocol structural matching.
 */
#include "test_framework.h"
#include "lsm.h"
#include "lsp/type_rep.h"

/* ── UNION: flatten, dedup, collapse ──────────────────────────── */

TEST(typerep_union_two_distinct) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *s = lsm_type_builtin(&a, "str");
    const LSMType *m[2] = {i, s};
    const LSMType *u = lsm_type_union(&a, m, 2);
    ASSERT(lsm_type_is_union(u));
    ASSERT_EQ(u->data.union_type.count, 2);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_union_dedupes_duplicates) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i1 = lsm_type_builtin(&a, "int");
    const LSMType *i2 = lsm_type_builtin(&a, "int");
    const LSMType *m[2] = {i1, i2};
    const LSMType *u = lsm_type_union(&a, m, 2);
    /* dedup collapses to a single int type, not a union */
    ASSERT(!lsm_type_is_union(u));
    ASSERT_EQ(u->kind, LSM_TYPE_BUILTIN);
    ASSERT_STR_EQ(u->data.builtin.name, "int");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_union_flattens_nested) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *s = lsm_type_builtin(&a, "str");
    const LSMType *b = lsm_type_builtin(&a, "bytes");
    const LSMType *inner_pair[2] = {i, s};
    const LSMType *inner = lsm_type_union(&a, inner_pair, 2);
    const LSMType *outer_pair[2] = {inner, b};
    const LSMType *u = lsm_type_union(&a, outer_pair, 2);
    /* flattening yields 3 distinct members */
    ASSERT(lsm_type_is_union(u));
    ASSERT_EQ(u->data.union_type.count, 3);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_union_single_member_collapses) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *m[1] = {i};
    const LSMType *u = lsm_type_union(&a, m, 1);
    ASSERT(!lsm_type_is_union(u));
    ASSERT_EQ(u->kind, LSM_TYPE_BUILTIN);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_union_empty_is_unknown) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *u = lsm_type_union(&a, NULL, 0);
    ASSERT(lsm_type_is_unknown(u));
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_optional_is_union_with_none) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *opt = lsm_type_optional(&a, i);
    ASSERT(lsm_type_is_union(opt));
    ASSERT_EQ(opt->data.union_type.count, 2);
    /* membership: int + None */
    bool has_int = false, has_none = false;
    for (int k = 0; k < opt->data.union_type.count; k++) {
        const LSMType *m = opt->data.union_type.members[k];
        if (m->kind == LSM_TYPE_BUILTIN && strcmp(m->data.builtin.name, "int") == 0)
            has_int = true;
        if (m->kind == LSM_TYPE_BUILTIN && strcmp(m->data.builtin.name, "None") == 0)
            has_none = true;
    }
    ASSERT(has_int);
    ASSERT(has_none);
    lsm_arena_destroy(&a);
    PASS();
}

/* ── LITERAL ───────────────────────────────────────────────────── */

TEST(typerep_literal_int_3) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *base = lsm_type_builtin(&a, "int");
    const LSMType *lit = lsm_type_literal(&a, base, "3");
    ASSERT_EQ(lit->kind, LSM_TYPE_LITERAL);
    ASSERT(lsm_type_equal(lit->data.literal.base, base));
    ASSERT_STR_EQ(lit->data.literal.literal_text, "3");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_literal_equality_distinguishes_text) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *base = lsm_type_builtin(&a, "str");
    const LSMType *foo = lsm_type_literal(&a, base, "\"foo\"");
    const LSMType *bar = lsm_type_literal(&a, base, "\"bar\"");
    const LSMType *foo2 = lsm_type_literal(&a, base, "\"foo\"");
    ASSERT(!lsm_type_equal(foo, bar));
    ASSERT(lsm_type_equal(foo, foo2));
    lsm_arena_destroy(&a);
    PASS();
}

/* ── PROTOCOL ──────────────────────────────────────────────────── */

TEST(typerep_protocol_method_set) {
    LSMArena a;
    lsm_arena_init(&a);
    const char *methods[] = {"read", "close", NULL};
    const LSMType *proto = lsm_type_protocol(&a, "typing.IO", methods, NULL);
    ASSERT(lsm_type_is_protocol(proto));
    ASSERT_STR_EQ(proto->data.protocol.qualified_name, "typing.IO");
    ASSERT_NOT_NULL(proto->data.protocol.method_names);
    ASSERT_STR_EQ(proto->data.protocol.method_names[0], "read");
    ASSERT_STR_EQ(proto->data.protocol.method_names[1], "close");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_protocol_satisfied_by_protocol_with_superset) {
    LSMArena a;
    lsm_arena_init(&a);
    const char *needed[] = {"read", "close", NULL};
    const char *have[] = {"read", "write", "close", "flush", NULL};
    const LSMType *proto = lsm_type_protocol(&a, "P1", needed, NULL);
    const LSMType *cand = lsm_type_protocol(&a, "P2", have, NULL);
    ASSERT(lsm_type_protocol_satisfied_by(proto, cand));
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_protocol_unsatisfied_when_method_missing) {
    LSMArena a;
    lsm_arena_init(&a);
    const char *needed[] = {"read", "close", NULL};
    const char *have[] = {"read", NULL};
    const LSMType *proto = lsm_type_protocol(&a, "P1", needed, NULL);
    const LSMType *cand = lsm_type_protocol(&a, "P2", have, NULL);
    ASSERT(!lsm_type_protocol_satisfied_by(proto, cand));
    lsm_arena_destroy(&a);
    PASS();
}

/* ── MODULE ────────────────────────────────────────────────────── */

TEST(typerep_module_carries_qn) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *m = lsm_type_module(&a, "os.path");
    ASSERT(lsm_type_is_module(m));
    ASSERT_STR_EQ(m->data.module.module_qn, "os.path");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_module_equality_by_qn) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *a1 = lsm_type_module(&a, "os");
    const LSMType *a2 = lsm_type_module(&a, "os");
    const LSMType *b = lsm_type_module(&a, "sys");
    ASSERT(lsm_type_equal(a1, a2));
    ASSERT(!lsm_type_equal(a1, b));
    lsm_arena_destroy(&a);
    PASS();
}

/* ── CALLABLE ──────────────────────────────────────────────────── */

TEST(typerep_callable_with_args_and_return) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *s = lsm_type_builtin(&a, "str");
    const LSMType *params[2] = {i, s};
    const LSMType *c = lsm_type_callable(&a, params, 2, i);
    ASSERT_EQ(c->kind, LSM_TYPE_CALLABLE);
    ASSERT_EQ(c->data.callable.param_count, 2);
    ASSERT(lsm_type_equal(c->data.callable.return_type, i));
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_callable_elliptic_arity_minus_one) {
    /* Callable[..., R] — variadic in Python type-hint sense. */
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *r = lsm_type_builtin(&a, "int");
    const LSMType *c = lsm_type_callable(&a, NULL, -1, r);
    ASSERT_EQ(c->data.callable.param_count, -1);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(typerep_callable_equality) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *i = lsm_type_builtin(&a, "int");
    const LSMType *s = lsm_type_builtin(&a, "str");
    const LSMType *p1[1] = {i};
    const LSMType *c1 = lsm_type_callable(&a, p1, 1, s);
    const LSMType *c2 = lsm_type_callable(&a, p1, 1, s);
    const LSMType *c3 = lsm_type_callable(&a, p1, 1, i); /* different return */
    ASSERT(lsm_type_equal(c1, c2));
    ASSERT(!lsm_type_equal(c1, c3));
    lsm_arena_destroy(&a);
    PASS();
}

/* ── SUBSTITUTION ──────────────────────────────────────────────── */

TEST(typerep_substitute_unbound_param_preserved) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *t = lsm_type_type_param(&a, "T");
    const char *params[] = {"T", NULL};
    const LSMType *args[] = {NULL, NULL};
    const LSMType *sub = lsm_type_substitute(&a, t, params, args);
    ASSERT(sub == t);
    lsm_arena_destroy(&a);
    PASS();
}

/* #427: type_args may be SHORTER than type_params — a class template
 * instantiated with fewer args than declared params (e.g. `Box<Widget>` for
 * `template<class T, class U, class V>`) or trailing default template args.
 * Matching a param whose index exceeds the args length must NOT index past the
 * args array's NULL terminator. Pre-fix, this read args[2] one element past the
 * 2-slot stack array (ASan stack-buffer-overflow) and returned a bogus LSMType*
 * that was later dereferenced -> SEGV in type_to_qn (c_lsp.c). The unbound
 * param must be preserved as-is. */
TEST(typerep_substitute_short_args_no_oob_issue427) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *t = lsm_type_type_param(&a, "V");                   /* the 3rd declared param */
    const char *params[] = {"T", "U", "V", NULL};                      /* 3 declared params */
    const LSMType *args[] = {lsm_type_type_param(&a, "Widget"), NULL}; /* 1 arg */
    const LSMType *sub = lsm_type_substitute(&a, t, params, args);
    ASSERT(sub == t); /* "V" (index 2) has no supplied arg -> preserved, no OOB */
    lsm_arena_destroy(&a);
    PASS();
}

/* A caller that forgets to NULL-terminate type_args (a stack array) makes the
 * bounded walk read uninitialized memory — and a garbage "pointer" within the
 * param count would be BOUND to a type param and woven into the resulting type
 * graph (bitcoin serialize.h: Using<Fmt>(v) with explicit args bound T to stack
 * garbage; the corrupt graph was dereferenced later -> SIGSEGV). Implausible
 * values (misaligned / null-page) must act as the terminator instead. */
TEST(typerep_substitute_rejects_garbage_args_entries) {
    LSMArena a;
    lsm_arena_init(&a);
    const LSMType *t =
        lsm_type_reference(&a, lsm_type_type_param(&a, "T")); /* T& as in Wrapper<F, T&> */
    const char *params[] = {"F", "T", NULL};
    const LSMType *args[2];
    args[0] = lsm_type_named(&a, "proj.Fmt"); /* explicit arg for F */
    args[1] = (const LSMType *)0x37;          /* simulated uninitialized stack garbage */
    const LSMType *sub = lsm_type_substitute(&a, t, params, args);
    ASSERT_NOT_NULL(sub);
    ASSERT_EQ(sub->kind, LSM_TYPE_REFERENCE);
    /* T has no real binding: it must be preserved, never the garbage value. */
    ASSERT_TRUE(sub->data.reference.elem != (const LSMType *)0x37);
    ASSERT_EQ(sub->data.reference.elem->kind, LSM_TYPE_TYPE_PARAM);
    lsm_arena_destroy(&a);
    PASS();
}

/* ── Suite ─────────────────────────────────────────────────────── */

SUITE(type_rep) {
    /* UNION */
    RUN_TEST(typerep_union_two_distinct);
    RUN_TEST(typerep_union_dedupes_duplicates);
    RUN_TEST(typerep_union_flattens_nested);
    RUN_TEST(typerep_union_single_member_collapses);
    RUN_TEST(typerep_union_empty_is_unknown);
    RUN_TEST(typerep_optional_is_union_with_none);
    /* LITERAL */
    RUN_TEST(typerep_literal_int_3);
    RUN_TEST(typerep_literal_equality_distinguishes_text);
    /* PROTOCOL */
    RUN_TEST(typerep_protocol_method_set);
    RUN_TEST(typerep_protocol_satisfied_by_protocol_with_superset);
    RUN_TEST(typerep_protocol_unsatisfied_when_method_missing);
    /* MODULE */
    RUN_TEST(typerep_module_carries_qn);
    RUN_TEST(typerep_module_equality_by_qn);
    /* CALLABLE */
    RUN_TEST(typerep_callable_with_args_and_return);
    RUN_TEST(typerep_callable_elliptic_arity_minus_one);
    RUN_TEST(typerep_callable_equality);
    /* SUBSTITUTION */
    RUN_TEST(typerep_substitute_unbound_param_preserved);
    RUN_TEST(typerep_substitute_short_args_no_oob_issue427);
    RUN_TEST(typerep_substitute_rejects_garbage_args_entries);
}
