/*
 * worker_pool.c — Parallel-for dispatch with pthreads.
 *
 * Uses pthreads with 8MB stacks and atomic work-stealing index.
 * GCD is avoided because its worker threads have 512KB stacks,
 * which overflows on deeply nested ASTs (tree-sitter + walk_defs).
 *
 * Each worker pulls indices from a shared atomic counter — zero
 * contention, natural load balancing across heterogeneous cores.
 */
#include "pipeline/worker_pool.h"
#include "foundation/constants.h"

enum { WP_TRUE = 1, WP_MIN = 1, WP_STEP = 1 };
#include "foundation/platform.h"
#include "foundation/compat_thread.h"

#include <stdatomic.h>
#include <stdlib.h>

/* 8 MB stack per worker — matches main thread default.
 * Required for deep AST recursion (tree-sitter + walk_defs). */
#define LSM_WORKER_STACK_SIZE ((size_t)8 * LSM_SZ_1K * LSM_SZ_1K)

/* ── Serial fallback ─────────────────────────────────────────────── */

static void run_serial(int count, lsm_parallel_fn fn, void *ctx) {
    for (int i = 0; i < count; i++) {
        fn(i, ctx);
    }
}

/* ── pthreads backend ────────────────────────────────────────────── */

typedef struct {
    lsm_parallel_fn fn;
    void *ctx;
    _Atomic int *next_idx;
    int count;
} pthread_worker_arg_t;

static void *pthread_worker(void *arg) {
    pthread_worker_arg_t *wa = arg;
    while (WP_TRUE) {
        int idx = atomic_fetch_add_explicit(wa->next_idx, WP_STEP, memory_order_relaxed);
        if (idx >= wa->count) {
            break;
        }
        wa->fn(idx, wa->ctx);
    }
    return NULL;
}

static void run_pthreads(int count, lsm_parallel_fn fn, void *ctx, int nworkers) {
    _Atomic int next_idx = 0;

    pthread_worker_arg_t wa = {
        .fn = fn,
        .ctx = ctx,
        .next_idx = &next_idx,
        .count = count,
    };

    lsm_thread_t *threads = (lsm_thread_t *)malloc((size_t)nworkers * sizeof(lsm_thread_t));
    if (!threads) {
        run_serial(count, fn, ctx);
        return;
    }

    for (int i = 0; i < nworkers; i++) {
        if (lsm_thread_create(&threads[i], LSM_WORKER_STACK_SIZE, pthread_worker, &wa) != 0) {
            /* Failed to create thread — let remaining work run in main thread */
            nworkers = i;
            break;
        }
    }

    /* Main thread also participates */
    while (WP_TRUE) {
        int idx = atomic_fetch_add_explicit(&next_idx, WP_STEP, memory_order_relaxed);
        if (idx >= count) {
            break;
        }
        fn(idx, ctx);
    }

    for (int i = 0; i < nworkers; i++) {
        lsm_thread_join(&threads[i]);
    }

    free(threads);
}

/* ── Public API ──────────────────────────────────────────────────── */

void lsm_parallel_for(int count, lsm_parallel_fn fn, void *ctx, lsm_parallel_for_opts_t opts) {
    if (count <= 0 || !fn) {
        return;
    }

    /* Determine worker count */
    int nworkers = opts.max_workers;
    if (nworkers <= 0) {
        nworkers = lsm_default_worker_count(true);
    }
    if (nworkers < WP_MIN) {
        nworkers = SKIP_ONE;
    }

    /* Serial fallback: single worker or trivially small workload */
    if (nworkers <= WP_MIN || count <= WP_MIN) {
        run_serial(count, fn, ctx);
        return;
    }

    run_pthreads(count, fn, ctx, nworkers);
}
