/*
 * lock_registry.h — Generic writer-preference registry over private file locks.
 */
#ifndef LSM_LOCK_REGISTRY_H
#define LSM_LOCK_REGISTRY_H

#include "foundation/private_file_lock.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define LSM_LOCK_REGISTRY_NAME_CAP 80U

typedef struct lsm_lock_registry lsm_lock_registry_t;
typedef struct lsm_lock_lease lsm_lock_lease_t;
typedef atomic_bool lsm_lock_cancel_token_t;

lsm_lock_registry_t *lsm_lock_registry_new(lsm_private_lock_directory_t *directory);

/* Sticky cancellation: stores true with release ordering and wakes registry
 * waiters. The token must outlive every acquisition that observes it. */
lsm_private_file_lock_status_t lsm_lock_registry_request_cancel(lsm_lock_registry_t *registry,
                                                                lsm_lock_cancel_token_t *token);

/* The directory is borrowed and must outlive the registry and every lease.
 * Resource keys must come from a bounded internal namespace: lock sidecars are
 * deliberately stable and are not unlinked on release. */

/* deadline_ms is an absolute lsm_now_ms() deadline; UINT64_MAX means no
 * deadline. cancel_token may be NULL; otherwise initialize it to false before
 * acquisition and keep it alive until acquisition returns. Tokens are sticky
 * and are not reset by the registry. Clean cancellation/deadline rollback
 * returns BUSY with a NULL lease. Waiter-bookkeeping or native-rollback
 * failures can return IO with a cleanup-only lease; callers must release every
 * non-NULL lease even when acquisition did not return OK. IO may return a NULL
 * lease after a terminal native close error when no retryable ownership or
 * accounting remains. */
lsm_private_file_lock_status_t lsm_lock_registry_acquire(
    lsm_lock_registry_t *registry, const char *resource_key, lsm_private_file_lock_mode_t mode,
    uint64_t deadline_ms, const lsm_lock_cancel_token_t *cancel_token,
    lsm_lock_lease_t **lease_out);

/* Make one fair process-local/native acquisition attempt without parking.
 * A free lock can succeed; existing queued/native contention returns BUSY.
 * Cleanup-only lease semantics are identical to the waiting API. */
lsm_private_file_lock_status_t lsm_lock_registry_try_acquire(lsm_lock_registry_t *registry,
                                                             const char *resource_key,
                                                             lsm_private_file_lock_mode_t mode,
                                                             lsm_lock_lease_t **lease_out);

/* Writers release .rw before .turn. Readers retain only .rw. The same release
 * call disposes cleanup-only leases returned by failed acquisition rollback,
 * first detaching any retained waiter and then releasing native handles. OK
 * clears *lease_io. IO normally leaves a non-NULL retryable lease whose waiter,
 * active, or pending-cleanup accounting keeps the registry live. A terminal
 * native close error can return IO while clearing *lease_io after all ownership
 * and accounting have been discharged. */
lsm_private_file_lock_status_t lsm_lock_lease_release(lsm_lock_lease_t **lease_io);

/* Refuses to free a registry with active leases, waiters, or pending cleanup.
 * OK destroys its resources, clears *registry_io, and retires the control
 * identity for the process lifetime so copied stale pointers cannot alias a
 * future registry; stale operations fail with IO. */
lsm_private_file_lock_status_t lsm_lock_registry_free(lsm_lock_registry_t **registry_io);

#endif /* LSM_LOCK_REGISTRY_H */
