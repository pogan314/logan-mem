/* lock_registry.c — FIFO process-local registry over private file locks. */
#include "foundation/lock_registry.h"

#include "foundation/compat_thread.h"
#include "foundation/lock_registry_internal.h"
#include "foundation/platform.h"
#include "foundation/private_file_lock_internal.h"
#include "foundation/sha256.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

enum {
    LOCK_REGISTRY_RESOURCE_CAP = 4096,
    LOCK_REGISTRY_NATIVE_RETRY_MS = 1,
};

typedef struct lock_registry_waiter lock_registry_waiter_t;
typedef struct lock_registry_entry lock_registry_entry_t;

struct lock_registry_waiter {
    lsm_private_file_lock_mode_t mode;
    lock_registry_waiter_t *next;
    bool queued;
};

struct lock_registry_entry {
    char turn_name[LSM_LOCK_REGISTRY_NAME_CAP];
    char rw_name[LSM_LOCK_REGISTRY_NAME_CAP];
    lock_registry_waiter_t *waiter_head;
    lock_registry_waiter_t *waiter_tail;
    lock_registry_waiter_t *attempt_waiter;
    size_t active_readers;
    bool writer_active;
    lock_registry_entry_t *next;
};

struct lsm_lock_registry {
    lsm_private_lock_directory_t *directory;
    lsm_private_fork_condition_t *condition;
    lsm_mutex_t mutex;
    lock_registry_entry_t *entries;
    size_t waiter_count;
    size_t active_lease_count;
    size_t pending_cleanup_count;
    uint64_t owner_pid;
    bool closing;
    lsm_lock_registry_stage_hook_fn stage_hook;
    void *stage_context;
    lsm_lock_registry_release_handle_t test_release_fault_handle;
    lsm_private_file_lock_release_step_t test_release_fault_step;
    bool test_release_fault_armed;
    lsm_lock_registry_abort_failure_t test_abort_failure;
    bool test_abort_failure_armed;
    atomic_uint_fast64_t test_condition_wait_calls;
    atomic_size_t test_condition_waiters_now;
    struct lsm_lock_registry *next_live;
    struct lsm_lock_registry *next_retired;
};

struct lsm_lock_lease {
    lsm_lock_registry_t *registry;
    lock_registry_entry_t *entry;
    lock_registry_waiter_t waiter;
    lsm_private_file_lock_mode_t mode;
    lsm_private_file_lock_t *turn;
    lsm_private_file_lock_t *rw;
    uint64_t owner_pid;
    bool active;
    bool cleanup_only;
    bool waiter_cleanup_pending;
    bool pending_registered;
    bool native_released;
    bool critical_released;
    bool release_error;
    bool test_abort_lock_failure_path;
};

/* Protected by lsm_private_file_lock_fork_guard_enter(). Besides serializing
 * teardown against fork, this lets a caller already waiting on that guard
 * reject a registry that another thread has just freed without touching the
 * freed mutex. */
static lsm_lock_registry_t *lock_registry_live;
/* Process-lifetime identity tombstones prevent a stale raw pointer from
 * becoming live again through allocator address reuse. The list itself keeps
 * the small retired control allocations reachable to leak detectors. */
static lsm_lock_registry_t *lock_registry_retired;

static uint64_t lock_registry_current_pid(void) {
#ifdef _WIN32
    return (uint64_t)GetCurrentProcessId();
#else
    return (uint64_t)getpid();
#endif
}

static bool lock_registry_is_live_unlocked(const lsm_lock_registry_t *registry) {
    for (const lsm_lock_registry_t *cursor = lock_registry_live; cursor;
         cursor = cursor->next_live) {
        if (cursor == registry) {
            return true;
        }
    }
    return false;
}

/* Lock order is always global fork guard, then registry mutex. No native lock
 * operation or user callback is permitted while either one is held. */
static bool lock_registry_lock(lsm_lock_registry_t *registry) {
    if (!registry || !lsm_private_file_lock_fork_guard_enter()) {
        return false;
    }
    if (!lock_registry_is_live_unlocked(registry)) {
        lsm_private_file_lock_fork_guard_leave();
        return false;
    }
    lsm_mutex_lock(&registry->mutex);
    if (registry->closing || registry->owner_pid != lock_registry_current_pid()) {
        lsm_mutex_unlock(&registry->mutex);
        lsm_private_file_lock_fork_guard_leave();
        return false;
    }
    return true;
}

static void lock_registry_unlock(lsm_lock_registry_t *registry) {
    lsm_mutex_unlock(&registry->mutex);
    lsm_private_file_lock_fork_guard_leave();
}

/* Requires the global fork guard and registry mutex, in that order. */
static void lock_registry_broadcast_locked(lsm_lock_registry_t *registry) {
    lsm_private_fork_condition_broadcast_while_guarded(registry->condition);
}

/* Requires the global fork guard and registry mutex, in that order. The
 * condition is associated with the global guard: release only the registry
 * mutex before the atomic guard-release-and-wait operation, then restore the
 * full G -> R lock order before returning. */
static lsm_private_fork_wait_status_t lock_registry_wait_locked(lsm_lock_registry_t *registry,
                                                                uint64_t deadline_ms) {
    (void)atomic_fetch_add_explicit(&registry->test_condition_wait_calls, 1, memory_order_relaxed);
    (void)atomic_fetch_add_explicit(&registry->test_condition_waiters_now, 1, memory_order_relaxed);
    lsm_mutex_unlock(&registry->mutex);
    lsm_private_fork_wait_status_t status =
        lsm_private_fork_condition_wait_until_while_guarded(registry->condition, deadline_ms);
    lsm_mutex_lock(&registry->mutex);
    (void)atomic_fetch_sub_explicit(&registry->test_condition_waiters_now, 1, memory_order_relaxed);
    return status;
}

static uint64_t lock_registry_bounded_deadline(uint64_t deadline_ms, uint64_t interval_ms) {
    uint64_t now_ms = lsm_now_ms();
    uint64_t bounded = now_ms > UINT64_MAX - interval_ms ? UINT64_MAX : now_ms + interval_ms;
    return deadline_ms < bounded ? deadline_ms : bounded;
}

/* Native ownership can change in another process, so the attempt owner keeps
 * a bounded retry even when no process-local state transition broadcasts. */
static bool lock_registry_park_native(lsm_lock_registry_t *registry, uint64_t deadline_ms,
                                      const lsm_lock_cancel_token_t *cancel_token) {
    if (!lock_registry_lock(registry)) {
        return false;
    }
    lsm_private_fork_wait_status_t status = LSM_PRIVATE_FORK_WAIT_SIGNALED;
    if (!cancel_token || !atomic_load_explicit(cancel_token, memory_order_acquire)) {
        status = lock_registry_wait_locked(
            registry, lock_registry_bounded_deadline(deadline_ms, LOCK_REGISTRY_NATIVE_RETRY_MS));
    }
    lock_registry_unlock(registry);
    return status != LSM_PRIVATE_FORK_WAIT_ERROR;
}

static bool lock_registry_should_stop(uint64_t deadline_ms,
                                      const lsm_lock_cancel_token_t *cancel_token) {
    if (cancel_token && atomic_load_explicit(cancel_token, memory_order_acquire)) {
        return true;
    }
    return deadline_ms != UINT64_MAX && lsm_now_ms() >= deadline_ms;
}

static void lock_registry_notify(lsm_lock_registry_t *registry, lsm_private_file_lock_mode_t mode,
                                 lsm_lock_registry_stage_t stage) {
    if (!lock_registry_lock(registry)) {
        return;
    }
    lsm_lock_registry_stage_hook_fn hook = registry->stage_hook;
    if (!hook) {
        lock_registry_unlock(registry);
        return;
    }
    void *context = registry->stage_context;
    lock_registry_unlock(registry);
    hook(context, mode, stage);
}

static lock_registry_entry_t *lock_registry_find_entry(lsm_lock_registry_t *registry,
                                                       const char *turn_name, const char *rw_name) {
    for (lock_registry_entry_t *entry = registry->entries; entry; entry = entry->next) {
        if (strcmp(entry->turn_name, turn_name) == 0 && strcmp(entry->rw_name, rw_name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void lock_registry_waiter_push(lsm_lock_registry_t *registry, lock_registry_entry_t *entry,
                                      lock_registry_waiter_t *waiter) {
    waiter->next = NULL;
    waiter->queued = true;
    if (entry->waiter_tail) {
        entry->waiter_tail->next = waiter;
    } else {
        entry->waiter_head = waiter;
    }
    entry->waiter_tail = waiter;
    registry->waiter_count++;
}

static bool lock_registry_waiter_remove(lsm_lock_registry_t *registry, lock_registry_entry_t *entry,
                                        lock_registry_waiter_t *waiter) {
    if (!waiter->queued) {
        return false;
    }
    lock_registry_waiter_t *previous = NULL;
    lock_registry_waiter_t *cursor = entry->waiter_head;
    while (cursor && cursor != waiter) {
        previous = cursor;
        cursor = cursor->next;
    }
    if (!cursor) {
        return false;
    }
    if (previous) {
        previous->next = cursor->next;
    } else {
        entry->waiter_head = cursor->next;
    }
    if (entry->waiter_tail == cursor) {
        entry->waiter_tail = previous;
    }
    cursor->next = NULL;
    cursor->queued = false;
    registry->waiter_count--;
    lock_registry_broadcast_locked(registry);
    return true;
}

static void lock_registry_apply_release_fault(lsm_lock_registry_t *registry,
                                              lsm_lock_registry_release_handle_t handle,
                                              lsm_private_file_lock_t *lock) {
    if (!registry || !lock || !lock_registry_lock(registry)) {
        return;
    }
    bool inject =
        registry->test_release_fault_armed && registry->test_release_fault_handle == handle;
    lsm_private_file_lock_release_step_t step = registry->test_release_fault_step;
    if (inject) {
        registry->test_release_fault_armed = false;
    }
    lock_registry_unlock(registry);
    if (inject) {
        (void)lsm_private_file_lock_fail_next_release_step_for_test(lock, step);
    }
}

/* Rollback and writer release are deliberately rw-before-turn. */
static lsm_private_file_lock_status_t lock_registry_release_native(
    lsm_lock_registry_t *registry, lsm_private_file_lock_t **rw_io,
    lsm_private_file_lock_t **turn_io) {
    lsm_private_file_lock_status_t result = LSM_PRIVATE_FILE_LOCK_OK;
    lock_registry_apply_release_fault(registry, LSM_LOCK_REGISTRY_RELEASE_RW, *rw_io);
    if (*rw_io && lsm_private_file_lock_release(rw_io) != LSM_PRIVATE_FILE_LOCK_OK) {
        result = LSM_PRIVATE_FILE_LOCK_IO;
        if (*rw_io && !lsm_private_file_lock_unlock_complete(*rw_io)) {
            return result;
        }
    }
    lock_registry_apply_release_fault(registry, LSM_LOCK_REGISTRY_RELEASE_TURN, *turn_io);
    if (*turn_io && lsm_private_file_lock_release(turn_io) != LSM_PRIVATE_FILE_LOCK_OK) {
        result = LSM_PRIVATE_FILE_LOCK_IO;
    }
    return result;
}

bool lsm_lock_registry_resource_names(const char *resource_key,
                                      char turn_out[LSM_LOCK_REGISTRY_NAME_CAP],
                                      char rw_out[LSM_LOCK_REGISTRY_NAME_CAP]) {
    if (!resource_key || !resource_key[0] || !turn_out || !rw_out) {
        return false;
    }
    size_t length = strnlen(resource_key, LOCK_REGISTRY_RESOURCE_CAP + 1U);
    if (length == 0 || length > LOCK_REGISTRY_RESOURCE_CAP) {
        return false;
    }
    char digest[LSM_SHA256_HEX_LEN + 1];
    lsm_sha256_hex(resource_key, length, digest);
    int turn_written = snprintf(turn_out, LSM_LOCK_REGISTRY_NAME_CAP, "lsm-%s.turn", digest);
    int rw_written = snprintf(rw_out, LSM_LOCK_REGISTRY_NAME_CAP, "lsm-%s.rw", digest);
    return turn_written > 0 && turn_written < (int)LSM_LOCK_REGISTRY_NAME_CAP && rw_written > 0 &&
           rw_written < (int)LSM_LOCK_REGISTRY_NAME_CAP;
}

lsm_lock_registry_t *lsm_lock_registry_new(lsm_private_lock_directory_t *directory) {
    if (!directory) {
        return NULL;
    }
    lsm_lock_registry_t *registry = calloc(1, sizeof(*registry));
    if (!registry) {
        return NULL;
    }
    registry->directory = directory;
    registry->owner_pid = lock_registry_current_pid();
    lsm_mutex_init(&registry->mutex);
    registry->condition = lsm_private_fork_condition_new();
    if (!registry->condition) {
        lsm_mutex_destroy(&registry->mutex);
        free(registry);
        return NULL;
    }
    if (!lsm_private_file_lock_fork_guard_enter()) {
        lsm_private_fork_condition_free(registry->condition);
        lsm_mutex_destroy(&registry->mutex);
        free(registry);
        return NULL;
    }
    registry->next_live = lock_registry_live;
    lock_registry_live = registry;
    lsm_private_file_lock_fork_guard_leave();
    return registry;
}

lsm_private_file_lock_status_t lsm_lock_registry_request_cancel(lsm_lock_registry_t *registry,
                                                                lsm_lock_cancel_token_t *token) {
    if (!token) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    atomic_store_explicit(token, true, memory_order_release);
    if (!lock_registry_lock(registry)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lock_registry_broadcast_locked(registry);
    lock_registry_unlock(registry);
    return LSM_PRIVATE_FILE_LOCK_OK;
}

static void lock_registry_retain_waiter_cleanup(lsm_lock_lease_t *lease,
                                                lsm_private_file_lock_t **rw_io,
                                                lsm_private_file_lock_t **turn_io,
                                                lsm_lock_lease_t **lease_out) {
    lease->rw = *rw_io;
    lease->turn = *turn_io;
    lease->cleanup_only = true;
    lease->waiter_cleanup_pending = true;
    *rw_io = NULL;
    *turn_io = NULL;
    *lease_out = lease;
}

static bool lock_registry_abort_lock(lsm_lock_registry_t *registry) {
    if (!lock_registry_lock(registry)) {
        return false;
    }
    bool fail_lock = registry->test_abort_failure_armed &&
                     registry->test_abort_failure == LSM_LOCK_REGISTRY_ABORT_FAIL_LOCK;
    if (fail_lock) {
        registry->test_abort_failure_armed = false;
        lock_registry_unlock(registry);
        return false;
    }
    return true;
}

static lsm_private_file_lock_status_t lock_registry_abort_attempt(
    lsm_lock_registry_t *registry, lock_registry_entry_t *entry, lock_registry_waiter_t *waiter,
    lsm_private_file_lock_t **rw_io, lsm_private_file_lock_t **turn_io,
    lsm_private_file_lock_status_t requested_status, lsm_lock_lease_t *lease,
    lsm_lock_lease_t **lease_out) {
    if (!lock_registry_abort_lock(registry)) {
        lease->test_abort_lock_failure_path = true;
        lock_registry_retain_waiter_cleanup(lease, rw_io, turn_io, lease_out);
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    bool owns_attempt = entry->attempt_waiter == waiter;
    bool fail_remove = registry->test_abort_failure_armed &&
                       registry->test_abort_failure == LSM_LOCK_REGISTRY_ABORT_FAIL_REMOVE;
    if (fail_remove) {
        registry->test_abort_failure_armed = false;
    }
    bool removed = !fail_remove && lock_registry_waiter_remove(registry, entry, waiter);
    if (removed && owns_attempt) {
        entry->attempt_waiter = NULL;
    }
    bool has_native = *rw_io || *turn_io;
    if (removed && has_native) {
        lease->registry = registry;
        lease->mode = waiter->mode;
        lease->rw = *rw_io;
        lease->turn = *turn_io;
        lease->owner_pid = registry->owner_pid;
        lease->cleanup_only = true;
        lease->pending_registered = true;
        lease->entry = NULL;
        registry->pending_cleanup_count++;
        lock_registry_broadcast_locked(registry);
        *rw_io = NULL;
        *turn_io = NULL;
    }
    lock_registry_unlock(registry);
    if (!removed) {
        lock_registry_retain_waiter_cleanup(lease, rw_io, turn_io, lease_out);
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (!has_native) {
        free(lease);
        return owns_attempt ? requested_status : LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_lock_lease_t *cleanup = lease;
    if (lsm_lock_lease_release(&cleanup) != LSM_PRIVATE_FILE_LOCK_OK) {
        *lease_out = cleanup;
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    return owns_attempt ? requested_status : LSM_PRIVATE_FILE_LOCK_IO;
}

static lsm_private_file_lock_status_t lock_registry_attempt_native(
    lsm_lock_registry_t *registry, lock_registry_entry_t *entry, lock_registry_waiter_t *waiter,
    uint64_t deadline_ms, const lsm_lock_cancel_token_t *cancel_token, bool try_once,
    lsm_lock_lease_t *lease, lsm_lock_lease_t **lease_out) {
    lsm_private_file_lock_t *turn = NULL;
    lsm_private_file_lock_t *rw = NULL;
    lsm_private_file_lock_status_t status = LSM_PRIVATE_FILE_LOCK_BUSY;

    while (!rw) {
        if (!try_once && lock_registry_should_stop(deadline_ms, cancel_token)) {
            status = LSM_PRIVATE_FILE_LOCK_BUSY;
            break;
        }
        if (!turn) {
            lsm_private_file_lock_mode_t turn_mode = waiter->mode == LSM_PRIVATE_FILE_LOCK_SH
                                                         ? LSM_PRIVATE_FILE_LOCK_EX
                                                         : LSM_PRIVATE_FILE_LOCK_SH;
            status = lsm_private_file_lock_try_acquire(registry->directory, entry->turn_name,
                                                       turn_mode, &turn);
            if (status == LSM_PRIVATE_FILE_LOCK_BUSY) {
                lock_registry_notify(registry, waiter->mode, LSM_LOCK_REGISTRY_STAGE_TURN_BUSY);
                if (try_once) {
                    break;
                }
                if (!lock_registry_park_native(registry, deadline_ms, cancel_token)) {
                    status = LSM_PRIVATE_FILE_LOCK_IO;
                    break;
                }
                continue;
            }
            if (status != LSM_PRIVATE_FILE_LOCK_OK) {
                break;
            }
            lock_registry_notify(registry, waiter->mode, LSM_LOCK_REGISTRY_STAGE_TURN_HELD);
        }

        if (!try_once && lock_registry_should_stop(deadline_ms, cancel_token)) {
            status = LSM_PRIVATE_FILE_LOCK_BUSY;
            break;
        }
        status = lsm_private_file_lock_try_acquire(registry->directory, entry->rw_name,
                                                   waiter->mode, &rw);
        if (status == LSM_PRIVATE_FILE_LOCK_BUSY) {
            lock_registry_notify(registry, waiter->mode, LSM_LOCK_REGISTRY_STAGE_RW_BUSY);
            if (try_once) {
                break;
            }
            if (!lock_registry_park_native(registry, deadline_ms, cancel_token)) {
                status = LSM_PRIVATE_FILE_LOCK_IO;
                break;
            }
            continue;
        }
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            break;
        }
        if (waiter->mode == LSM_PRIVATE_FILE_LOCK_SH) {
            status = lsm_private_file_lock_release(&turn);
            if (status != LSM_PRIVATE_FILE_LOCK_OK) {
                break;
            }
        }
        lock_registry_notify(registry, waiter->mode, LSM_LOCK_REGISTRY_STAGE_NATIVE_READY);
    }

    if (!rw || status != LSM_PRIVATE_FILE_LOCK_OK) {
        return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn, status, lease,
                                           lease_out);
    }

    for (;;) {
        if (!try_once && lock_registry_should_stop(deadline_ms, cancel_token)) {
            return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn,
                                               LSM_PRIVATE_FILE_LOCK_BUSY, lease, lease_out);
        }
        if (!lock_registry_lock(registry)) {
            return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn,
                                               LSM_PRIVATE_FILE_LOCK_IO, lease, lease_out);
        }
        bool owns_attempt = entry->attempt_waiter == waiter && entry->waiter_head == waiter;
        bool local_state_ready = waiter->mode == LSM_PRIVATE_FILE_LOCK_SH
                                     ? !entry->writer_active
                                     : !entry->writer_active && entry->active_readers == 0;
        bool removed = owns_attempt && local_state_ready &&
                       lock_registry_waiter_remove(registry, entry, waiter);
        lsm_private_fork_wait_status_t wait_status = LSM_PRIVATE_FORK_WAIT_SIGNALED;
        if (removed) {
            entry->attempt_waiter = NULL;
            if (waiter->mode == LSM_PRIVATE_FILE_LOCK_SH) {
                entry->active_readers++;
            } else {
                entry->writer_active = true;
            }
            registry->active_lease_count++;
            lease->registry = registry;
            lease->entry = entry;
            lease->mode = waiter->mode;
            lease->turn = turn;
            lease->rw = rw;
            lease->owner_pid = registry->owner_pid;
            lease->active = true;
            turn = NULL;
            rw = NULL;
            lock_registry_broadcast_locked(registry);
        } else if (!try_once && owns_attempt &&
                   (!cancel_token || !atomic_load_explicit(cancel_token, memory_order_acquire))) {
            wait_status = lock_registry_wait_locked(registry, deadline_ms);
        }
        lock_registry_unlock(registry);
        if (removed) {
            *lease_out = lease;
            return LSM_PRIVATE_FILE_LOCK_OK;
        }
        if (try_once) {
            return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn,
                                               LSM_PRIVATE_FILE_LOCK_BUSY, lease, lease_out);
        }
        if (!owns_attempt) {
            return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn,
                                               LSM_PRIVATE_FILE_LOCK_IO, lease, lease_out);
        }
        if (wait_status == LSM_PRIVATE_FORK_WAIT_ERROR) {
            return lock_registry_abort_attempt(registry, entry, waiter, &rw, &turn,
                                               LSM_PRIVATE_FILE_LOCK_IO, lease, lease_out);
        }
    }
}

static lsm_private_file_lock_status_t lock_registry_acquire_internal(
    lsm_lock_registry_t *registry, const char *resource_key, lsm_private_file_lock_mode_t mode,
    uint64_t deadline_ms, const lsm_lock_cancel_token_t *cancel_token, bool try_once,
    lsm_lock_lease_t **lease_out) {
    if (lease_out) {
        *lease_out = NULL;
    }
    if (!registry || !lease_out) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (mode != LSM_PRIVATE_FILE_LOCK_SH && mode != LSM_PRIVATE_FILE_LOCK_EX) {
        return LSM_PRIVATE_FILE_LOCK_UNSAFE;
    }
    char turn_name[LSM_LOCK_REGISTRY_NAME_CAP];
    char rw_name[LSM_LOCK_REGISTRY_NAME_CAP];
    if (!lsm_lock_registry_resource_names(resource_key, turn_name, rw_name)) {
        return LSM_PRIVATE_FILE_LOCK_UNSAFE;
    }
    if (!try_once && lock_registry_should_stop(deadline_ms, cancel_token)) {
        return LSM_PRIVATE_FILE_LOCK_BUSY;
    }

    lock_registry_entry_t *candidate = calloc(1, sizeof(*candidate));
    lsm_lock_lease_t *lease = calloc(1, sizeof(*lease));
    if (!candidate || !lease) {
        free(candidate);
        free(lease);
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    (void)snprintf(candidate->turn_name, sizeof(candidate->turn_name), "%s", turn_name);
    (void)snprintf(candidate->rw_name, sizeof(candidate->rw_name), "%s", rw_name);
    lease->waiter.mode = mode;

    if (!lock_registry_lock(registry)) {
        free(candidate);
        free(lease);
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lock_registry_entry_t *entry = lock_registry_find_entry(registry, turn_name, rw_name);
    if (!entry) {
        entry = candidate;
        candidate = NULL;
        entry->next = registry->entries;
        registry->entries = entry;
    }
    lease->registry = registry;
    lease->entry = entry;
    lease->mode = mode;
    lease->owner_pid = registry->owner_pid;
    lock_registry_waiter_push(registry, entry, &lease->waiter);
    lock_registry_unlock(registry);
    free(candidate);

    for (;;) {
        bool should_stop = !try_once && lock_registry_should_stop(deadline_ms, cancel_token);
        if (!lock_registry_lock(registry)) {
            lease->cleanup_only = true;
            lease->waiter_cleanup_pending = true;
            *lease_out = lease;
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
        if (should_stop) {
            bool removed = lock_registry_waiter_remove(registry, entry, &lease->waiter);
            lock_registry_unlock(registry);
            if (removed) {
                free(lease);
                return LSM_PRIVATE_FILE_LOCK_BUSY;
            }
            lease->cleanup_only = true;
            lease->waiter_cleanup_pending = true;
            *lease_out = lease;
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
        bool can_attempt = entry->waiter_head == &lease->waiter && entry->attempt_waiter == NULL;
        lsm_private_fork_wait_status_t wait_status = LSM_PRIVATE_FORK_WAIT_SIGNALED;
        if (can_attempt) {
            entry->attempt_waiter = &lease->waiter;
        } else if (try_once) {
            bool removed = lock_registry_waiter_remove(registry, entry, &lease->waiter);
            lock_registry_unlock(registry);
            if (removed) {
                free(lease);
                return LSM_PRIVATE_FILE_LOCK_BUSY;
            }
            lease->cleanup_only = true;
            lease->waiter_cleanup_pending = true;
            *lease_out = lease;
            return LSM_PRIVATE_FILE_LOCK_IO;
        } else if (!cancel_token || !atomic_load_explicit(cancel_token, memory_order_acquire)) {
            wait_status = lock_registry_wait_locked(registry, deadline_ms);
        }
        lock_registry_unlock(registry);
        if (can_attempt) {
            return lock_registry_attempt_native(registry, entry, &lease->waiter, deadline_ms,
                                                cancel_token, try_once, lease, lease_out);
        }
        if (wait_status == LSM_PRIVATE_FORK_WAIT_ERROR) {
            lease->cleanup_only = true;
            lease->waiter_cleanup_pending = true;
            *lease_out = lease;
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
    }
}

lsm_private_file_lock_status_t lsm_lock_registry_acquire(
    lsm_lock_registry_t *registry, const char *resource_key, lsm_private_file_lock_mode_t mode,
    uint64_t deadline_ms, const lsm_lock_cancel_token_t *cancel_token,
    lsm_lock_lease_t **lease_out) {
    return lock_registry_acquire_internal(registry, resource_key, mode, deadline_ms, cancel_token,
                                          false, lease_out);
}

lsm_private_file_lock_status_t lsm_lock_registry_try_acquire(lsm_lock_registry_t *registry,
                                                             const char *resource_key,
                                                             lsm_private_file_lock_mode_t mode,
                                                             lsm_lock_lease_t **lease_out) {
    return lock_registry_acquire_internal(registry, resource_key, mode, UINT64_MAX, NULL, true,
                                          lease_out);
}

static lsm_private_file_lock_status_t lock_registry_cleanup_lease_release(
    lsm_lock_lease_t **lease_io) {
    lsm_lock_lease_t *lease = *lease_io;
    lsm_lock_registry_t *registry = lease->registry;
    bool waiter_state_valid = lease->waiter_cleanup_pending && lease->entry &&
                              lease->waiter.queued && !lease->pending_registered &&
                              !lease->native_released;
    bool pending_state_valid =
        !lease->waiter_cleanup_pending && lease->pending_registered &&
        (lease->native_released ? !lease->rw && !lease->turn : lease->rw || lease->turn);
    if (!registry || !lease->cleanup_only || (!waiter_state_valid && !pending_state_valid)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }

    if (lease->waiter_cleanup_pending) {
        if (!lock_registry_lock(registry)) {
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
        lock_registry_entry_t *entry = lease->entry;
        bool removed = lock_registry_waiter_remove(registry, entry, &lease->waiter);
        if (!removed) {
            lock_registry_unlock(registry);
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
        if (entry->attempt_waiter == &lease->waiter) {
            entry->attempt_waiter = NULL;
        }
        lease->waiter_cleanup_pending = false;
        lease->entry = NULL;
        bool has_native = lease->rw || lease->turn;
        if (has_native) {
            registry->pending_cleanup_count++;
            lease->pending_registered = true;
            lock_registry_broadcast_locked(registry);
        } else {
            lease->native_released = true;
        }
        lock_registry_unlock(registry);
        if (!has_native) {
            *lease_io = NULL;
            free(lease);
            return LSM_PRIVATE_FILE_LOCK_OK;
        }
    }

    if (!lock_registry_lock(registry)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    bool registered = registry->pending_cleanup_count > 0;
    lock_registry_unlock(registry);
    if (!registered) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }

    lsm_private_file_lock_status_t result = LSM_PRIVATE_FILE_LOCK_OK;
    if (!lease->native_released) {
        lsm_private_file_lock_status_t status =
            lock_registry_release_native(registry, &lease->rw, &lease->turn);
        lease->native_released = !lease->rw && !lease->turn;
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            result = LSM_PRIVATE_FILE_LOCK_IO;
            if (!lease->native_released) {
                return result;
            }
        }
    }
    if (!lock_registry_lock(registry)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (registry->pending_cleanup_count == 0) {
        lock_registry_unlock(registry);
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    registry->pending_cleanup_count--;
    lease->pending_registered = false;
    lock_registry_broadcast_locked(registry);
    lock_registry_unlock(registry);
    *lease_io = NULL;
    free(lease);
    return result;
}

lsm_private_file_lock_status_t lsm_lock_lease_release(lsm_lock_lease_t **lease_io) {
    if (!lease_io || !*lease_io) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_lock_lease_t *lease = *lease_io;
    if (lease->owner_pid != lock_registry_current_pid()) {
        lsm_private_file_lock_status_t status =
            lock_registry_release_native(lease->registry, &lease->rw, &lease->turn);
        if (lease->waiter_cleanup_pending || lease->waiter.queued) {
            return LSM_PRIVATE_FILE_LOCK_IO;
        }
        if (status == LSM_PRIVATE_FILE_LOCK_OK) {
            *lease_io = NULL;
            free(lease);
        }
        return status;
    }
    if (lease->cleanup_only) {
        return lock_registry_cleanup_lease_release(lease_io);
    }

    lsm_lock_registry_t *registry = lease->registry;
    lock_registry_entry_t *entry = lease->entry;
    bool native_state_valid = false;
    if (lease->critical_released) {
        native_state_valid = true;
    } else if (lease->mode == LSM_PRIVATE_FILE_LOCK_SH) {
        native_state_valid = lease->rw && !lease->turn;
    } else if (lease->mode == LSM_PRIVATE_FILE_LOCK_EX) {
        native_state_valid = lease->turn != NULL;
    }
    if (!registry || !entry || !lease->active || !native_state_valid ||
        !lock_registry_lock(registry)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    bool valid = registry->active_lease_count > 0;
    if (valid && lease->mode == LSM_PRIVATE_FILE_LOCK_SH) {
        valid = entry->active_readers > 0;
    } else if (valid && lease->mode == LSM_PRIVATE_FILE_LOCK_EX) {
        valid = entry->writer_active;
    } else {
        valid = false;
    }
    lock_registry_unlock(registry);
    if (!valid) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }

    if (!lease->critical_released) {
        lsm_private_file_lock_status_t status =
            lock_registry_release_native(registry, &lease->rw, &lease->turn);
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            lease->release_error = true;
        }
        if (lease->rw && !lsm_private_file_lock_unlock_complete(lease->rw)) {
            return status;
        }
        lease->critical_released = true;
        lease->native_released = !lease->rw && !lease->turn;
    }
    if (!lock_registry_lock(registry)) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (lease->mode == LSM_PRIVATE_FILE_LOCK_SH) {
        entry->active_readers--;
    } else {
        entry->writer_active = false;
    }
    registry->active_lease_count--;
    lease->active = false;
    bool cleanup_pending = lease->rw || lease->turn;
    if (cleanup_pending) {
        registry->pending_cleanup_count++;
        lease->cleanup_only = true;
        lease->pending_registered = true;
        lease->entry = NULL;
    }
    lock_registry_broadcast_locked(registry);
    lock_registry_unlock(registry);
    if (cleanup_pending) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_private_file_lock_status_t result =
        lease->release_error ? LSM_PRIVATE_FILE_LOCK_IO : LSM_PRIVATE_FILE_LOCK_OK;
    *lease_io = NULL;
    free(lease);
    return result;
}

lsm_private_file_lock_status_t lsm_lock_registry_free(lsm_lock_registry_t **registry_io) {
    if (!registry_io || !*registry_io || !lsm_private_file_lock_fork_guard_enter()) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_lock_registry_t *registry = *registry_io;
    lsm_lock_registry_t **live_cursor = &lock_registry_live;
    while (*live_cursor && *live_cursor != registry) {
        live_cursor = &(*live_cursor)->next_live;
    }
    if (!*live_cursor) {
        lsm_private_file_lock_fork_guard_leave();
        return LSM_PRIVATE_FILE_LOCK_IO;
    }

    lsm_mutex_lock(&registry->mutex);
    if (registry->closing || registry->owner_pid != lock_registry_current_pid()) {
        lsm_mutex_unlock(&registry->mutex);
        lsm_private_file_lock_fork_guard_leave();
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    bool idle =
        registry->waiter_count == 0 && registry->active_lease_count == 0 &&
        registry->pending_cleanup_count == 0 &&
        atomic_load_explicit(&registry->test_condition_waiters_now, memory_order_relaxed) == 0;
    for (lock_registry_entry_t *entry = registry->entries; idle && entry; entry = entry->next) {
        idle = !entry->waiter_head && !entry->waiter_tail && !entry->attempt_waiter &&
               entry->active_readers == 0 && !entry->writer_active;
    }
    if (!idle) {
        lsm_mutex_unlock(&registry->mutex);
        lsm_private_file_lock_fork_guard_leave();
        return LSM_PRIVATE_FILE_LOCK_BUSY;
    }
    registry->closing = true;
    *live_cursor = registry->next_live;
    registry->next_live = NULL;
    lsm_mutex_unlock(&registry->mutex);

    lock_registry_entry_t *entry = registry->entries;
    while (entry) {
        lock_registry_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }
    registry->entries = NULL;
    lsm_private_fork_condition_free(registry->condition);
    registry->condition = NULL;
    lsm_mutex_destroy(&registry->mutex);
    registry->directory = NULL;
    registry->owner_pid = 0;
    registry->stage_hook = NULL;
    registry->stage_context = NULL;
    registry->test_release_fault_handle = 0;
    registry->test_release_fault_step = 0;
    registry->test_release_fault_armed = false;
    registry->test_abort_failure = 0;
    registry->test_abort_failure_armed = false;
    atomic_store_explicit(&registry->test_condition_wait_calls, 0, memory_order_relaxed);
    atomic_store_explicit(&registry->test_condition_waiters_now, 0, memory_order_relaxed);
    registry->next_retired = lock_registry_retired;
    lock_registry_retired = registry;
    *registry_io = NULL;
    /* Retaining this guard through destruction prevents atfork from observing
     * a partially detached registry or destroyed registry mutex. */
    lsm_private_file_lock_fork_guard_leave();
    return LSM_PRIVATE_FILE_LOCK_OK;
}

size_t lsm_lock_registry_waiter_count(lsm_lock_registry_t *registry) {
    if (!lock_registry_lock(registry)) {
        return 0;
    }
    size_t count = registry->waiter_count;
    lock_registry_unlock(registry);
    return count;
}

size_t lsm_lock_registry_active_lease_count_for_test(lsm_lock_registry_t *registry) {
    if (!lock_registry_lock(registry)) {
        return 0;
    }
    size_t count = registry->active_lease_count;
    lock_registry_unlock(registry);
    return count;
}

size_t lsm_lock_registry_pending_cleanup_count_for_test(lsm_lock_registry_t *registry) {
    if (!lock_registry_lock(registry)) {
        return 0;
    }
    size_t count = registry->pending_cleanup_count;
    lock_registry_unlock(registry);
    return count;
}

bool lsm_lock_registry_is_retired_for_test(const lsm_lock_registry_t *registry) {
    if (!registry || !lsm_private_file_lock_fork_guard_enter()) {
        return false;
    }
    bool retired = false;
    for (const lsm_lock_registry_t *cursor = lock_registry_retired; cursor;
         cursor = cursor->next_retired) {
        if (cursor == registry) {
            retired = true;
            break;
        }
    }
    lsm_private_file_lock_fork_guard_leave();
    return retired;
}

size_t lsm_lock_registry_attempting_waiter_count_for_test(lsm_lock_registry_t *registry) {
    if (!lock_registry_lock(registry)) {
        return 0;
    }
    size_t count = 0;
    for (const lock_registry_entry_t *entry = registry->entries; entry; entry = entry->next) {
        if (entry->attempt_waiter) {
            count++;
        }
    }
    lock_registry_unlock(registry);
    return count;
}

uint64_t lsm_lock_registry_condition_wait_call_count_for_test(const lsm_lock_registry_t *registry) {
    return registry
               ? atomic_load_explicit(&registry->test_condition_wait_calls, memory_order_relaxed)
               : 0;
}

size_t lsm_lock_registry_condition_waiter_count_for_test(const lsm_lock_registry_t *registry) {
    return registry
               ? atomic_load_explicit(&registry->test_condition_waiters_now, memory_order_relaxed)
               : 0;
}

bool lsm_lock_lease_fail_next_release_step_for_test(lsm_lock_lease_t *lease,
                                                    lsm_lock_registry_release_handle_t handle,
                                                    lsm_private_file_lock_release_step_t step) {
    if (!lease) {
        return false;
    }
    lsm_private_file_lock_t *lock = handle == LSM_LOCK_REGISTRY_RELEASE_RW     ? lease->rw
                                    : handle == LSM_LOCK_REGISTRY_RELEASE_TURN ? lease->turn
                                                                               : NULL;
    return lsm_private_file_lock_fail_next_release_step_for_test(lock, step);
}

bool lsm_lock_lease_has_release_handle_for_test(const lsm_lock_lease_t *lease,
                                                lsm_lock_registry_release_handle_t handle) {
    if (!lease) {
        return false;
    }
    if (handle == LSM_LOCK_REGISTRY_RELEASE_RW) {
        return lease->rw != NULL;
    }
    if (handle == LSM_LOCK_REGISTRY_RELEASE_TURN) {
        return lease->turn != NULL;
    }
    return false;
}

bool lsm_lock_lease_used_abort_lock_failure_path_for_test(const lsm_lock_lease_t *lease) {
    return lease && lease->test_abort_lock_failure_path;
}

#ifndef _WIN32
bool lsm_lock_lease_fail_close_after_consuming_for_test(lsm_lock_lease_t *lease,
                                                        lsm_lock_registry_release_handle_t handle) {
    if (!lease) {
        return false;
    }
    lsm_private_file_lock_t *lock = handle == LSM_LOCK_REGISTRY_RELEASE_RW     ? lease->rw
                                    : handle == LSM_LOCK_REGISTRY_RELEASE_TURN ? lease->turn
                                                                               : NULL;
    return lsm_private_file_lock_fail_close_after_consuming_for_test(lock);
}
#endif

bool lsm_lock_registry_fail_next_native_release_step_for_test(
    lsm_lock_registry_t *registry, lsm_lock_registry_release_handle_t handle,
    lsm_private_file_lock_release_step_t step) {
    if ((handle != LSM_LOCK_REGISTRY_RELEASE_RW && handle != LSM_LOCK_REGISTRY_RELEASE_TURN) ||
        (step != LSM_PRIVATE_FILE_LOCK_RELEASE_UNLOCK &&
         step != LSM_PRIVATE_FILE_LOCK_RELEASE_CLOSE) ||
        !lock_registry_lock(registry)) {
        return false;
    }
    bool idle = registry->waiter_count == 0 && registry->active_lease_count == 0 &&
                registry->pending_cleanup_count == 0 && !registry->test_release_fault_armed;
    if (idle) {
        registry->test_release_fault_handle = handle;
        registry->test_release_fault_step = step;
        registry->test_release_fault_armed = true;
    }
    lock_registry_unlock(registry);
    return idle;
}

bool lsm_lock_registry_fail_next_abort_bookkeeping_for_test(
    lsm_lock_registry_t *registry, lsm_lock_registry_abort_failure_t failure) {
    if ((failure != LSM_LOCK_REGISTRY_ABORT_FAIL_LOCK &&
         failure != LSM_LOCK_REGISTRY_ABORT_FAIL_REMOVE) ||
        !lock_registry_lock(registry)) {
        return false;
    }
    bool idle = registry->waiter_count == 0 && registry->active_lease_count == 0 &&
                registry->pending_cleanup_count == 0 && !registry->test_abort_failure_armed;
    if (idle) {
        registry->test_abort_failure = failure;
        registry->test_abort_failure_armed = true;
    }
    lock_registry_unlock(registry);
    return idle;
}

bool lsm_lock_registry_set_stage_hook_for_test(lsm_lock_registry_t *registry,
                                               lsm_lock_registry_stage_hook_fn hook,
                                               void *context) {
    if (!lock_registry_lock(registry)) {
        return false;
    }
    bool idle = registry->waiter_count == 0 && registry->active_lease_count == 0 &&
                registry->pending_cleanup_count == 0;
    if (idle) {
        registry->stage_hook = hook;
        registry->stage_context = context;
    }
    lock_registry_unlock(registry);
    return idle;
}
