/*
 * test_hash_table.c — RED phase tests for foundation/hash_table.
 */
#include "test_framework.h"
#include "../src/foundation/hash_table.h"

TEST(ht_create_free) {
    LSMHashTable *ht = lsm_ht_create(16);
    ASSERT_NOT_NULL(ht);
    ASSERT_EQ(lsm_ht_count(ht), 0);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_set_get) {
    LSMHashTable *ht = lsm_ht_create(8);
    int val = 42;
    void *prev = lsm_ht_set(ht, "hello", &val);
    ASSERT_NULL(prev); /* first insert */
    void *got = lsm_ht_get(ht, "hello");
    ASSERT_EQ(got, &val);
    ASSERT_EQ(*(int *)got, 42);
    ASSERT_EQ(lsm_ht_count(ht), 1);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_set_overwrite) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v1 = 1, v2 = 2;
    lsm_ht_set(ht, "key", &v1);
    void *prev = lsm_ht_set(ht, "key", &v2);
    ASSERT_EQ(prev, &v1); /* returns old value */
    ASSERT_EQ(*(int *)lsm_ht_get(ht, "key"), 2);
    ASSERT_EQ(lsm_ht_count(ht), 1); /* still 1 entry */
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_get_missing) {
    LSMHashTable *ht = lsm_ht_create(8);
    ASSERT_NULL(lsm_ht_get(ht, "nope"));
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_has) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v = 1;
    lsm_ht_set(ht, "exists", &v);
    ASSERT_TRUE(lsm_ht_has(ht, "exists"));
    ASSERT_FALSE(lsm_ht_has(ht, "missing"));
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_delete) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v = 99;
    lsm_ht_set(ht, "delete_me", &v);
    ASSERT_EQ(lsm_ht_count(ht), 1);

    void *removed = lsm_ht_delete(ht, "delete_me");
    ASSERT_EQ(removed, &v);
    ASSERT_EQ(lsm_ht_count(ht), 0);
    ASSERT_NULL(lsm_ht_get(ht, "delete_me"));
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_delete_missing) {
    LSMHashTable *ht = lsm_ht_create(8);
    void *removed = lsm_ht_delete(ht, "nope");
    ASSERT_NULL(removed);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_many_entries) {
    LSMHashTable *ht = lsm_ht_create(4); /* tiny initial, force resize */
    char keys[200][32];
    int vals[200];
    for (int i = 0; i < 200; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key_%03d", i);
        vals[i] = i * 10;
        lsm_ht_set(ht, keys[i], &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 200);
    /* Verify all entries survive resize */
    for (int i = 0; i < 200; i++) {
        void *got = lsm_ht_get(ht, keys[i]);
        ASSERT_NOT_NULL(got);
        ASSERT_EQ(*(int *)got, i * 10);
    }
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_delete_then_reinsert) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v1 = 1, v2 = 2;
    lsm_ht_set(ht, "key", &v1);
    lsm_ht_delete(ht, "key");
    lsm_ht_set(ht, "key", &v2);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, "key"), 2);
    ASSERT_EQ(lsm_ht_count(ht), 1);
    lsm_ht_free(ht);
    PASS();
}

static int foreach_sum;
static void sum_values(const char *key, void *value, void *userdata) {
    (void)key;
    (void)userdata;
    foreach_sum += *(int *)value;
}

TEST(ht_foreach) {
    LSMHashTable *ht = lsm_ht_create(8);
    int vals[] = {10, 20, 30};
    lsm_ht_set(ht, "a", &vals[0]);
    lsm_ht_set(ht, "b", &vals[1]);
    lsm_ht_set(ht, "c", &vals[2]);

    foreach_sum = 0;
    lsm_ht_foreach(ht, sum_values, NULL);
    ASSERT_EQ(foreach_sum, 60);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_clear) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v = 1;
    lsm_ht_set(ht, "a", &v);
    lsm_ht_set(ht, "b", &v);
    lsm_ht_clear(ht);
    ASSERT_EQ(lsm_ht_count(ht), 0);
    ASSERT_NULL(lsm_ht_get(ht, "a"));
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_empty_string_key) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v = 42;
    lsm_ht_set(ht, "", &v);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, ""), 42);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_long_key) {
    LSMHashTable *ht = lsm_ht_create(8);
    /* 200-char key */
    char long_key[201];
    memset(long_key, 'x', 200);
    long_key[200] = '\0';
    int v = 99;
    lsm_ht_set(ht, long_key, &v);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, long_key), 99);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_create_small_capacity) {
    /* Small initial-capacity hints (7, 0, 1) must still produce a
     * usable table. LSMHashTable is opaque now (Verstable internals)
     * so we check the API contract, not impl details like power-of-2
     * sizing. */
    LSMHashTable *ht = lsm_ht_create(7);
    ASSERT_NOT_NULL(ht);
    int v = 42;
    lsm_ht_set(ht, "test", &v);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, "test"), 42);
    lsm_ht_free(ht);
    PASS();
}

/* ── Edge case tests ───────────────────────────────────────────── */

TEST(ht_get_key_returns_stored_pointer) {
    LSMHashTable *ht = lsm_ht_create(8);
    /* Use a heap key so we can verify the stored pointer differs from lookup */
    char stored_key[] = "mykey";
    int v = 1;
    lsm_ht_set(ht, stored_key, &v);
    /* Look up with a different buffer containing same string */
    char lookup[] = "mykey";
    const char *got = lsm_ht_get_key(ht, lookup);
    ASSERT_NOT_NULL(got);
    ASSERT_STR_EQ(got, "mykey");
    /* Should return the pointer we inserted with, not the lookup buffer */
    ASSERT_EQ((uintptr_t)got, (uintptr_t)stored_key);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_get_key_missing) {
    LSMHashTable *ht = lsm_ht_create(8);
    const char *got = lsm_ht_get_key(ht, "nonexistent");
    ASSERT_NULL(got);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_get_key_null_ht) {
    const char *got = lsm_ht_get_key(NULL, "key");
    ASSERT_NULL(got);
    PASS();
}

TEST(ht_get_key_null_key) {
    LSMHashTable *ht = lsm_ht_create(8);
    const char *got = lsm_ht_get_key(ht, NULL);
    ASSERT_NULL(got);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_count_null) {
    ASSERT_EQ(lsm_ht_count(NULL), 0);
    PASS();
}

TEST(ht_free_null) {
    /* Should not crash */
    lsm_ht_free(NULL);
    PASS();
}

TEST(ht_clear_null) {
    /* Should not crash */
    lsm_ht_clear(NULL);
    PASS();
}

TEST(ht_foreach_null_ht) {
    /* Should not crash */
    lsm_ht_foreach(NULL, sum_values, NULL);
    PASS();
}

TEST(ht_foreach_null_fn) {
    LSMHashTable *ht = lsm_ht_create(8);
    int v = 1;
    lsm_ht_set(ht, "a", &v);
    /* Should not crash with NULL function pointer */
    lsm_ht_foreach(ht, NULL, NULL);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_delete_readd_stress) {
    /* Stress backward-shift deletion: add 100, delete all, re-add all */
    LSMHashTable *ht = lsm_ht_create(8);
    char keys[100][16];
    int vals[100];
    for (int i = 0; i < 100; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key_%03d", i);
        vals[i] = i;
        lsm_ht_set(ht, keys[i], &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 100);

    /* Delete all */
    for (int i = 0; i < 100; i++) {
        void *removed = lsm_ht_delete(ht, keys[i]);
        ASSERT_EQ(removed, &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 0);

    /* Re-add all — exercises slots freed by backward shift */
    for (int i = 0; i < 100; i++) {
        lsm_ht_set(ht, keys[i], &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 100);
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(*(int *)lsm_ht_get(ht, keys[i]), i);
    }
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_interleaved_insert_delete) {
    /* Insert 100, delete odd-indexed, verify even-indexed survive */
    LSMHashTable *ht = lsm_ht_create(8);
    char keys[100][16];
    int vals[100];
    for (int i = 0; i < 100; i++) {
        snprintf(keys[i], sizeof(keys[i]), "entry_%03d", i);
        vals[i] = i * 7;
        lsm_ht_set(ht, keys[i], &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 100);

    /* Delete all odd-indexed entries */
    for (int i = 1; i < 100; i += 2) {
        lsm_ht_delete(ht, keys[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 50);

    /* Verify even entries intact, odd entries gone */
    for (int i = 0; i < 100; i++) {
        void *got = lsm_ht_get(ht, keys[i]);
        if (i % 2 == 0) {
            ASSERT_NOT_NULL(got);
            ASSERT_EQ(*(int *)got, i * 7);
        } else {
            ASSERT_NULL(got);
        }
    }
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_create_capacity_zero) {
    /* Capacity hint 0 → library picks default. Must produce a usable
     * table (no struct-internals checks now that LSMHashTable is opaque). */
    LSMHashTable *ht = lsm_ht_create(0);
    ASSERT_NOT_NULL(ht);
    int v = 42;
    lsm_ht_set(ht, "test", &v);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, "test"), 42);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_create_capacity_one) {
    /* Capacity hint 1 → library picks default. Must produce a usable table. */
    LSMHashTable *ht = lsm_ht_create(1);
    ASSERT_NOT_NULL(ht);
    int v = 99;
    lsm_ht_set(ht, "k", &v);
    ASSERT_EQ(*(int *)lsm_ht_get(ht, "k"), 99);
    lsm_ht_free(ht);
    PASS();
}

TEST(ht_stress_10000) {
    LSMHashTable *ht = lsm_ht_create(16);
    char keys[10000][24];
    int vals[10000];
    for (int i = 0; i < 10000; i++) {
        snprintf(keys[i], sizeof(keys[i]), "stress_key_%06d", i);
        vals[i] = i;
        lsm_ht_set(ht, keys[i], &vals[i]);
    }
    ASSERT_EQ(lsm_ht_count(ht), 10000);
    /* Verify all */
    for (int i = 0; i < 10000; i++) {
        void *got = lsm_ht_get(ht, keys[i]);
        ASSERT_NOT_NULL(got);
        ASSERT_EQ(*(int *)got, i);
    }
    lsm_ht_free(ht);
    PASS();
}

/* ht_robin_hood_bounded_psl removed — Robin-Hood-specific PSL is no
 * longer an implementation detail of LSMHashTable (replaced by
 * Verstable's quadratic-probing + chain metadata). The functional
 * equivalent (fast lookups under load) is covered by ht_stress_10000
 * which would TIME OUT or produce wrong results if probing went wild. */

SUITE(hash_table) {
    RUN_TEST(ht_create_free);
    RUN_TEST(ht_set_get);
    RUN_TEST(ht_set_overwrite);
    RUN_TEST(ht_get_missing);
    RUN_TEST(ht_has);
    RUN_TEST(ht_delete);
    RUN_TEST(ht_delete_missing);
    RUN_TEST(ht_many_entries);
    RUN_TEST(ht_delete_then_reinsert);
    RUN_TEST(ht_foreach);
    RUN_TEST(ht_clear);
    RUN_TEST(ht_empty_string_key);
    RUN_TEST(ht_long_key);
    RUN_TEST(ht_create_small_capacity);
    /* Edge cases */
    RUN_TEST(ht_get_key_returns_stored_pointer);
    RUN_TEST(ht_get_key_missing);
    RUN_TEST(ht_get_key_null_ht);
    RUN_TEST(ht_get_key_null_key);
    RUN_TEST(ht_count_null);
    RUN_TEST(ht_free_null);
    RUN_TEST(ht_clear_null);
    RUN_TEST(ht_foreach_null_ht);
    RUN_TEST(ht_foreach_null_fn);
    RUN_TEST(ht_delete_readd_stress);
    RUN_TEST(ht_interleaved_insert_delete);
    RUN_TEST(ht_create_capacity_zero);
    RUN_TEST(ht_create_capacity_one);
    RUN_TEST(ht_stress_10000);
    /* ht_robin_hood_bounded_psl removed in Verstable swap. */
}
