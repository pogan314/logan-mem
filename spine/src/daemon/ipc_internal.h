/*
 * ipc_internal.h — Private, platform-neutral seams used by daemon IPC tests.
 */
#ifndef LSM_DAEMON_IPC_INTERNAL_H
#define LSM_DAEMON_IPC_INTERNAL_H

#include "daemon/ipc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Windows daemon rendezvous addresses are generation-specific and
 * unguessable. These platform-neutral seams keep the SID/nonce derivation and
 * fixed record parser covered on every CI host; the Windows endpoint
 * constructor supplies the current token's binary SID and the startup winner
 * supplies a BCryptGenRandom nonce. */
#define LSM_DAEMON_IPC_WINDOWS_NAME_CAP 256U
#define LSM_DAEMON_IPC_WINDOWS_NONCE_SIZE 32U
#define LSM_DAEMON_IPC_WINDOWS_RENDEZVOUS_RECORD_SIZE \
    (8U + LSM_DAEMON_IPC_WINDOWS_NONCE_SIZE + LSM_DAEMON_IPC_WINDOWS_NAME_CAP)
#define LSM_DAEMON_IPC_WINDOWS_RENDEZVOUS_FILE "lsm-rendezvous.lock"

bool lsm_daemon_ipc_windows_generation_address(
    const uint8_t *sid, size_t sid_length, const char *instance_key,
    const uint8_t nonce[LSM_DAEMON_IPC_WINDOWS_NONCE_SIZE],
    char address_out[LSM_DAEMON_IPC_WINDOWS_NAME_CAP]);

/* Exact pre-cohort Windows namespace retained only for migration safety. New
 * generations never publish the deterministic pipe and never treat either
 * object as current authority; startup/lifetime may hold the old startup mutex
 * solely as a compatibility guard against an overlapping pre-cohort process. */
bool lsm_daemon_ipc_windows_legacy_names(const char *canonical_runtime_parent,
                                         const char *instance_key,
                                         char pipe_out[LSM_DAEMON_IPC_WINDOWS_NAME_CAP],
                                         char startup_mutex_out[LSM_DAEMON_IPC_WINDOWS_NAME_CAP]);

bool lsm_daemon_ipc_windows_rendezvous_record_encode(
    const uint8_t nonce[LSM_DAEMON_IPC_WINDOWS_NONCE_SIZE], const char *address,
    uint8_t record_out[LSM_DAEMON_IPC_WINDOWS_RENDEZVOUS_RECORD_SIZE]);
bool lsm_daemon_ipc_windows_rendezvous_record_decode(
    const uint8_t *record, size_t record_length,
    uint8_t nonce_out[LSM_DAEMON_IPC_WINDOWS_NONCE_SIZE],
    char address_out[LSM_DAEMON_IPC_WINDOWS_NAME_CAP]);

typedef enum {
    LSM_IPC_PENDING_WAIT_FAILED = -1,
    LSM_IPC_PENDING_WAIT_TIMEOUT = 0,
    LSM_IPC_PENDING_WAIT_SIGNALED = 1,
} lsm_ipc_pending_wait_status_t;

typedef enum {
    LSM_IPC_PENDING_FINISH_FAILED = -1,
    LSM_IPC_PENDING_FINISH_CANCELLED = 0,
    LSM_IPC_PENDING_FINISH_COMPLETED = 1,
} lsm_ipc_pending_finish_status_t;

typedef struct {
    void *context;
    lsm_ipc_pending_wait_status_t (*wait)(void *context, uint32_t timeout_ms);
    void (*cancel)(void *context);
    lsm_ipc_pending_finish_status_t (*finish)(void *context, bool blocking,
                                              uint32_t *transferred_out);
} lsm_ipc_pending_ops_t;

/* Returns 1 for a completed transfer, 0 for a cancelled timeout, and -1 for
 * an error. Every pending operation is terminal before this function returns. */
int lsm_daemon_ipc_wait_pending(const lsm_ipc_pending_ops_t *ops, uint32_t timeout_ms,
                                uint32_t *transferred_out);

/* Internal receive path for fixed-size unauthenticated protocol envelopes.
 * Payloads above max_payload_length poison the stream. The implementation must
 * reject from the decoded header before allocating or reading that payload. */
int lsm_daemon_ipc_receive_frame_bounded(lsm_daemon_ipc_connection_t *connection,
                                         uint32_t timeout_ms, uint32_t max_payload_length,
                                         lsm_daemon_frame_t *frame_out, uint8_t **payload_out);

/* Narrow crash/fault seams for publication-state and retry-state tests. They
 * are inert unless a test installs a hook/failure count in its own process. */
typedef enum {
    LSM_DAEMON_IPC_POSIX_PUBLICATION_ANCHOR_DURABLE = 1,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_PENDING_DURABLE = 2,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_STABLE_DURABLE = 3,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_MARKER_DURABLE = 4,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_PENDING_REMOVED = 5,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_PENDING_TEMP_SYNCED = 6,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_PENDING_RECORD_LINKED = 7,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_MARKER_TEMP_SYNCED = 8,
    LSM_DAEMON_IPC_POSIX_PUBLICATION_MARKER_RECORD_LINKED = 9,
} lsm_daemon_ipc_posix_publication_stage_t;

typedef void (*lsm_daemon_ipc_posix_publication_hook_fn)(
    lsm_daemon_ipc_posix_publication_stage_t stage, void *context);

void lsm_daemon_ipc_posix_publication_hook_set_for_test(
    lsm_daemon_ipc_posix_publication_hook_fn hook, void *context);
void lsm_daemon_ipc_windows_legacy_guard_release_failures_set_for_test(unsigned int count);

/* Deterministic-interleaving seam: fires on the Windows startup path once the
 * startup lock is held, before the rendezvous handoff. A test parks here to
 * pin the "startup is holding the lock" interleaving by construction. Polling
 * for that state instead is a race — the whole acquire → handoff → release
 * sequence completes in microseconds whenever the handoff does not block, so
 * the observation window has no lower bound. */
typedef void (*lsm_daemon_ipc_startup_gate_fn)(void *context);
void lsm_daemon_ipc_startup_gate_set_for_test(lsm_daemon_ipc_startup_gate_fn gate, void *context);

#endif /* LSM_DAEMON_IPC_INTERNAL_H */
