/*
 * test_scope.c — Tests for the chunked linked-frame LSMScope.
 *
 * Phase 0 of Python LSP integration: replaces the legacy fixed 64-binding
 * array with growable per-scope chunks. Tests verify dynamic growth,
 * binding-shadowing semantics, and the parent-chain lookup contract that
 * Go and C/C++ LSP implementations rely on.
 */
#include "test_framework.h"
#include "lsm.h"
#include "lsp/scope.h"
#include "lsp/type_rep.h"

/* Build a NAMED type for a fixture string. Arena-allocated. */
static const LSMType *named_t(LSMArena *a, const char *qn) {
    return lsm_type_named(a, qn);
}

/* ── Basic API ─────────────────────────────────────────────────── */

TEST(scope_push_returns_distinct_scope) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    ASSERT_NOT_NULL(root);
    ASSERT_NOT_NULL(child);
    ASSERT(child != root);
    ASSERT(child->parent == root);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_pop_returns_parent) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    ASSERT(lsm_scope_pop(child) == root);
    ASSERT(lsm_scope_pop(root) == NULL);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_lookup_unbound_returns_unknown) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    const LSMType *t = lsm_scope_lookup(s, "missing");
    ASSERT_NOT_NULL(t);
    ASSERT(lsm_type_is_unknown(t));
    lsm_arena_destroy(&a);
    PASS();
}

/* ── Binding semantics ─────────────────────────────────────────── */

TEST(scope_bind_then_lookup) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    lsm_scope_bind(s, "x", named_t(&a, "int"));
    const LSMType *t = lsm_scope_lookup(s, "x");
    ASSERT_NOT_NULL(t);
    ASSERT(t->kind == LSM_TYPE_NAMED);
    ASSERT_STR_EQ(t->data.named.qualified_name, "int");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_rebind_overwrites_in_place) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    lsm_scope_bind(s, "x", named_t(&a, "int"));
    lsm_scope_bind(s, "x", named_t(&a, "string"));
    const LSMType *t = lsm_scope_lookup(s, "x");
    ASSERT_STR_EQ(t->data.named.qualified_name, "string");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_lookup_walks_parent_chain) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    lsm_scope_bind(root, "outer", named_t(&a, "Outer"));
    lsm_scope_bind(child, "inner", named_t(&a, "Inner"));
    const LSMType *outer_in_child = lsm_scope_lookup(child, "outer");
    const LSMType *inner_in_root = lsm_scope_lookup(root, "inner");
    ASSERT_STR_EQ(outer_in_child->data.named.qualified_name, "Outer");
    ASSERT(lsm_type_is_unknown(inner_in_root));
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_child_shadows_parent) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    lsm_scope_bind(root, "x", named_t(&a, "ParentInt"));
    lsm_scope_bind(child, "x", named_t(&a, "ChildStr"));
    const LSMType *t = lsm_scope_lookup(child, "x");
    ASSERT_STR_EQ(t->data.named.qualified_name, "ChildStr");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_callable_identity_follows_nearest_binding) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    const LSMType *callback_type = named_t(&a, "Callback");
    lsm_scope_bind_callable(root, "callback", callback_type, "pkg.actual");
    ASSERT_TRUE(lsm_scope_contains(child, "callback"));
    ASSERT_STR_EQ(lsm_scope_lookup_callable(child, "callback"), "pkg.actual");

    lsm_scope_bind(child, "callback", named_t(&a, "int"));
    ASSERT_NULL(lsm_scope_lookup_callable(child, "callback"));
    ASSERT_STR_EQ(lsm_scope_lookup_callable(root, "callback"), "pkg.actual");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_assignment_updates_or_clears_nearest_callable_only) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *child = lsm_scope_push(&a, root);
    const LSMType *callback_type = named_t(&a, "Callback");
    lsm_scope_bind_callable(root, "callback", callback_type, "pkg.actual");

    ASSERT_TRUE(lsm_scope_update_callable(child, "callback", "pkg.alternate"));
    ASSERT_STR_EQ(lsm_scope_lookup_callable(root, "callback"), "pkg.alternate");
    ASSERT(lsm_scope_lookup(root, "callback") == callback_type);
    ASSERT_TRUE(lsm_scope_update_callable(child, "callback", NULL));
    ASSERT_NULL(lsm_scope_lookup_callable(root, "callback"));
    ASSERT_FALSE(lsm_scope_update_callable(child, "missing", "pkg.never"));

    lsm_scope_bind_callable(root, "callback", callback_type, "pkg.restored");
    lsm_scope_bind_callable(child, "callback", callback_type, "pkg.child");
    ASSERT_TRUE(lsm_scope_update_callable(child, "callback", "pkg.child_new"));
    ASSERT_STR_EQ(lsm_scope_lookup_callable(child, "callback"), "pkg.child_new");
    ASSERT_STR_EQ(lsm_scope_lookup_callable(root, "callback"), "pkg.restored");
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_checked_bind_reports_child_oom_despite_parent_name) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *root = lsm_scope_push(&a, NULL);
    LSMScope *ordinary_child = lsm_scope_push(&a, root);
    LSMScope *callable_child = lsm_scope_push(&a, root);
    const LSMType *callback_type = named_t(&a, "Callback");
    const LSMType *shadow_type = named_t(&a, "Shadow");
    lsm_scope_bind_callable(root, "callback", callback_type, "pkg.parent");

    int saved_nblocks = a.nblocks;
    size_t saved_used = a.used;
    a.nblocks = LSM_ARENA_MAX_BLOCKS;
    a.used = a.block_size;
    bool ordinary_bound = lsm_scope_bind_checked(ordinary_child, "callback", shadow_type);
    bool callable_bound = lsm_scope_bind_callable_checked(
        callable_child, "callback", shadow_type, "pkg.child");
    a.nblocks = saved_nblocks;
    a.used = saved_used;

    ASSERT_FALSE(ordinary_bound);
    ASSERT_FALSE(callable_bound);
    ASSERT_STR_EQ(lsm_scope_lookup_callable(ordinary_child, "callback"), "pkg.parent");
    ASSERT_STR_EQ(lsm_scope_lookup_callable(callable_child, "callback"), "pkg.parent");
    lsm_arena_destroy(&a);
    PASS();
}

/* ── Dynamic growth — the Phase 0 win ───────────────────────────── */

TEST(scope_dynamic_growth_300_bindings) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    char names[300][16];
    for (int i = 0; i < 300; i++) {
        snprintf(names[i], sizeof(names[0]), "v%d", i);
        char qn[32];
        snprintf(qn, sizeof(qn), "T%d", i);
        lsm_scope_bind(s, names[i], named_t(&a, qn));
    }
    /* All 300 bindings retrievable — old 64-cap would have dropped 236 */
    for (int i = 0; i < 300; i++) {
        const LSMType *t = lsm_scope_lookup(s, names[i]);
        ASSERT_NOT_NULL(t);
        ASSERT(t->kind == LSM_TYPE_NAMED);
        char expected[32];
        snprintf(expected, sizeof(expected), "T%d", i);
        ASSERT_STR_EQ(t->data.named.qualified_name, expected);
    }
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_growth_chunk_boundary) {
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    char names[LSM_SCOPE_CHUNK_BINDINGS + 1][16];
    /* Fill exactly one chunk + spillover: forces second chunk allocation */
    for (int i = 0; i <= LSM_SCOPE_CHUNK_BINDINGS; i++) {
        snprintf(names[i], sizeof(names[0]), "n%d", i);
        char qn[16];
        snprintf(qn, sizeof(qn), "T%d", i);
        lsm_scope_bind(s, names[i], named_t(&a, qn));
    }
    /* First-chunk binding still retrievable */
    const LSMType *first = lsm_scope_lookup(s, "n0");
    ASSERT_STR_EQ(first->data.named.qualified_name, "T0");
    /* Spillover binding (would have fallen off legacy 64-cap if cap < 17) */
    char last_name[16];
    snprintf(last_name, sizeof(last_name), "n%d", LSM_SCOPE_CHUNK_BINDINGS);
    char last_qn[16];
    snprintf(last_qn, sizeof(last_qn), "T%d", LSM_SCOPE_CHUNK_BINDINGS);
    const LSMType *last = lsm_scope_lookup(s, last_name);
    ASSERT_STR_EQ(last->data.named.qualified_name, last_qn);
    lsm_arena_destroy(&a);
    PASS();
}

TEST(scope_rebind_in_old_chunk) {
    /* After many bindings have spilled into newer chunks, rebinding a name
     * from the original chunk must overwrite that binding in place rather
     * than create a duplicate in the head chunk. */
    LSMArena a;
    lsm_arena_init(&a);
    LSMScope *s = lsm_scope_push(&a, NULL);
    lsm_scope_bind(s, "early", named_t(&a, "EarlyV1"));
    char names[100][16];
    for (int i = 0; i < 100; i++) {
        snprintf(names[i], sizeof(names[0]), "fill%d", i);
        lsm_scope_bind(s, names[i], named_t(&a, "Filler"));
    }
    lsm_scope_bind(s, "early", named_t(&a, "EarlyV2"));
    const LSMType *t = lsm_scope_lookup(s, "early");
    ASSERT_STR_EQ(t->data.named.qualified_name, "EarlyV2");
    lsm_arena_destroy(&a);
    PASS();
}

/* ── Suite registration ────────────────────────────────────────── */

SUITE(scope) {
    RUN_TEST(scope_push_returns_distinct_scope);
    RUN_TEST(scope_pop_returns_parent);
    RUN_TEST(scope_lookup_unbound_returns_unknown);
    RUN_TEST(scope_bind_then_lookup);
    RUN_TEST(scope_rebind_overwrites_in_place);
    RUN_TEST(scope_lookup_walks_parent_chain);
    RUN_TEST(scope_child_shadows_parent);
    RUN_TEST(scope_callable_identity_follows_nearest_binding);
    RUN_TEST(scope_assignment_updates_or_clears_nearest_callable_only);
    RUN_TEST(scope_checked_bind_reports_child_oom_despite_parent_name);
    RUN_TEST(scope_dynamic_growth_300_bindings);
    RUN_TEST(scope_growth_chunk_boundary);
    RUN_TEST(scope_rebind_in_old_chunk);
}
