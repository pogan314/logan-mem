/*
 * repro_issue363.c — Regression guard for bug #363 (both axes now FIXED).
 *
 * Issue: #363 — "Linux: lsm_system_info / lsm_default_worker_count don't
 *               respect cgroup CPU/memory limits"
 *
 * Both axes of #363 have shipped; this case is now a permanent GREEN guard:
 *
 *   CPU axis — FIXED in v0.8.0 (commit a5a3d1d).
 *     lsm_detect_cgroup_cpus() reads /sys/fs/cgroup/cpu.max (v2) or
 *     .../cpu/cpu.cfs_quota_us + .../cpu/cpu.cfs_period_us (v1); the result
 *     feeds detect_system_linux(). lsm_default_worker_count() also honours the
 *     LSM_WORKERS env override (commit d952238). Both tested in test_platform.c.
 *
 *   Memory axis — FIXED.
 *     lsm_detect_cgroup_mem() reads /sys/fs/cgroup/memory.max (v2) or
 *     .../memory/memory.limit_in_bytes (v1); detect_system_linux() applies
 *     min(cgroup, host). The missing env knob — the "EXACT OPEN GAP" this
 *     repro was filed for — now exists: lsm_mem_init() reads LSM_MEM_BUDGET_MB
 *     and routes it through the pure lsm_mem_resolve_budget() (explicit
 *     override > implicit detection, mirroring LSM_WORKERS).
 *
 * IMPORTANT — clamp semantics:
 *   lsm_mem_resolve_budget() clamps the override to effective (cgroup/host) RAM
 *   so it can never claim more than the box has. So a *large* override is NOT
 *   honoured verbatim on a small host — it clamps to total_ram. This guard
 *   therefore uses a small budget (REPRO363_BUDGET_MB) that is at/below any
 *   realistic host's RAM, so it is honoured exactly and the assertion is stable
 *   on every runner. The clamp behaviour itself is unit-tested separately in
 *   tests/test_mem.c (resolve_budget_override_clamped_to_total).
 *
 * NOTE on lsm_mem_init() caching:
 *   g_budget is initialised once via atomic_compare_exchange_strong, so this
 *   guard relies on running before any suite that calls lsm_mem_init(); the
 *   repro runner does not init the budget before this suite.
 */

#include "test_framework.h"
#include <foundation/mem.h>
#include <foundation/compat.h>
#include <stdint.h>
#include <stdlib.h>

/* Deliberately small so the resolver honours it exactly (never clamps) on any
 * host — see the clamp-semantics note above. */
#define REPRO363_BUDGET_MB 128UL
#define REPRO363_BUDGET_BYTES (REPRO363_BUDGET_MB * 1024UL * 1024UL)

/*
 * repro_issue363_mem_budget_env_override
 *
 * Precondition: LSM_MEM_BUDGET_MB is set before the process's first
 * lsm_mem_init(). The budget must equal the override (honoured exactly, since
 * it is below any host's effective RAM), proving the env knob is wired.
 */
TEST(repro_issue363_mem_budget_env_override) {
    lsm_setenv("LSM_MEM_BUDGET_MB", "128", 1);

    lsm_mem_init(0.5);

    size_t budget = lsm_mem_budget();

    lsm_unsetenv("LSM_MEM_BUDGET_MB");

    ASSERT_EQ((long long)budget, (long long)REPRO363_BUDGET_BYTES);

    PASS();
}

SUITE(repro_issue363) {
    RUN_TEST(repro_issue363_mem_budget_env_override);
}
