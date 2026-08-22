/* project_lock.h — Shared daemon/local-CLI project mutation leases. */
#ifndef LSM_DAEMON_PROJECT_LOCK_H
#define LSM_DAEMON_PROJECT_LOCK_H

#include "daemon/ipc.h"
#include "foundation/lock_registry.h"

#include <stdint.h>

typedef struct lsm_project_lock_manager lsm_project_lock_manager_t;
typedef struct lsm_project_lock_lease lsm_project_lock_lease_t;

/* Each manager is an independent process-local registry over the endpoint's
 * owner-only runtime directory. Separate LSM processes therefore coordinate
 * through the same native lock files without sharing memory. */
lsm_project_lock_manager_t *lsm_project_lock_manager_new(const lsm_daemon_ipc_endpoint_t *endpoint);

/* Normal projects hold SH(project-set) + EX(project). "*" holds
 * EX(project-set), blocking every named project. Project lock keys are ASCII
 * case-folded to cover filename aliases on case-insensitive filesystems. */
lsm_private_file_lock_status_t lsm_project_lock_acquire(lsm_project_lock_manager_t *manager,
                                                        const char *project, uint64_t deadline_ms,
                                                        const lsm_lock_cancel_token_t *cancel_token,
                                                        lsm_project_lock_lease_t **lease_out);

/* One fair, nonblocking attempt for UI/watcher paths. */
lsm_private_file_lock_status_t lsm_project_lock_try_acquire(lsm_project_lock_manager_t *manager,
                                                            const char *project,
                                                            lsm_project_lock_lease_t **lease_out);

lsm_private_file_lock_status_t lsm_project_lock_lease_release(lsm_project_lock_lease_t **lease_io);

lsm_private_file_lock_status_t lsm_project_lock_request_cancel(lsm_project_lock_manager_t *manager,
                                                               lsm_lock_cancel_token_t *token);

/* Refuses teardown while any lease/cleanup state remains. */
lsm_private_file_lock_status_t lsm_project_lock_manager_free(
    lsm_project_lock_manager_t **manager_io);

#endif /* LSM_DAEMON_PROJECT_LOCK_H */
