/*
 * daemon.h — Process-local coordination and wire framing for the LSM daemon.
 *
 * Transport and worker supervision live outside this module. The coordinator
 * binds clients and resource subscriptions to transport connections, coalesces
 * shared work, and defines the daemon's terminal shutdown transition.
 */
#ifndef LSM_DAEMON_H
#define LSM_DAEMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Permanent framing version for the account-wide rendezvous endpoint. Never
 * bump this for detailed runtime payload changes: incompatible executable
 * generations must still exchange the stable HELLO conflict envelope. */
#define LSM_DAEMON_RENDEZVOUS_FRAME_VERSION 1U
#define LSM_DAEMON_FRAME_HEADER_SIZE 12U
#define LSM_DAEMON_MAX_FRAME_SIZE (10U * 1024U * 1024U)
#define LSM_DAEMON_KEY_SIZE 17U

typedef enum {
    LSM_DAEMON_FRAME_REQUEST = 1,
    LSM_DAEMON_FRAME_RESPONSE = 2,
} lsm_daemon_frame_type_t;

typedef struct {
    lsm_daemon_frame_type_t type;
    uint16_t flags;
    uint32_t length;
} lsm_daemon_frame_t;

typedef struct lsm_daemon_coordinator lsm_daemon_coordinator_t;

typedef uint64_t lsm_daemon_client_id_t;
typedef uint64_t lsm_daemon_subscription_id_t;

#define LSM_DAEMON_CLIENT_ID_INVALID ((lsm_daemon_client_id_t)0)
#define LSM_DAEMON_SUBSCRIPTION_ID_INVALID ((lsm_daemon_subscription_id_t)0)

typedef enum {
    LSM_DAEMON_COORDINATOR_RUNNING = 1,
    LSM_DAEMON_COORDINATOR_STOPPING = 2,
} lsm_daemon_coordinator_state_t;

typedef enum {
    LSM_DAEMON_SUBSCRIPTION_REJECTED = 0,
    LSM_DAEMON_SUBSCRIPTION_STARTED = 1,
    LSM_DAEMON_SUBSCRIPTION_JOINED = 2,
} lsm_daemon_subscription_result_t;

typedef enum {
    LSM_DAEMON_JOB_NONE = 0,
    LSM_DAEMON_JOB_RUNNING = 1,
    LSM_DAEMON_JOB_CANCEL_REQUESTED = 2,
    LSM_DAEMON_JOB_REAPING = 3,
} lsm_daemon_job_state_t;

typedef void (*lsm_daemon_job_cancel_fn)(const char *project_key, void *context);
typedef void (*lsm_daemon_watch_release_fn)(const char *project_key, void *context);

typedef struct {
    lsm_daemon_job_cancel_fn cancel_job;
    lsm_daemon_watch_release_fn release_watch;
    void *context;
} lsm_daemon_coordinator_hooks_t;

/* lease_timeout_ms is fixed for the coordinator lifetime. All timestamps must
 * come from the same monotonic clock domain. */
lsm_daemon_coordinator_t *lsm_daemon_coordinator_new(uint64_t lease_timeout_ms);

/* A PERMANENT coordinator (backing a `daemon start` generation) never
 * self-transitions to STOPPING when its client count reaches zero; only the
 * explicit stop/drain paths end it. */
void lsm_daemon_coordinator_set_permanent(lsm_daemon_coordinator_t *coordinator, bool permanent);
/* The caller must first quiesce coordinator calls and hook invocations. */
void lsm_daemon_coordinator_free(lsm_daemon_coordinator_t *coordinator);

/* Hooks are copied. Their context must remain valid until the coordinator is
 * quiescent. Hooks are always invoked after releasing the coordinator mutex. */
bool lsm_daemon_coordinator_set_hooks(lsm_daemon_coordinator_t *coordinator,
                                      const lsm_daemon_coordinator_hooks_t *hooks);
lsm_daemon_coordinator_state_t lsm_daemon_coordinator_state(lsm_daemon_coordinator_t *coordinator);

/* Client IDs are daemon-issued, nonzero, monotonic, and never recycled. */
lsm_daemon_client_id_t lsm_daemon_client_connected(lsm_daemon_coordinator_t *coordinator,
                                                   uint64_t now_ms);
bool lsm_daemon_client_disconnected(lsm_daemon_coordinator_t *coordinator,
                                    lsm_daemon_client_id_t client_id, uint64_t now_ms);
bool lsm_daemon_client_heartbeat(lsm_daemon_coordinator_t *coordinator,
                                 lsm_daemon_client_id_t client_id, uint64_t now_ms);
size_t lsm_daemon_expire_leases(lsm_daemon_coordinator_t *coordinator, uint64_t now_ms);
size_t lsm_daemon_active_clients(lsm_daemon_coordinator_t *coordinator);

/* Every accepted subscription receives a unique daemon-issued handle. The
 * first subscriber starts the physical resource; later subscribers join it. */
lsm_daemon_subscription_result_t lsm_daemon_job_subscribe(
    lsm_daemon_coordinator_t *coordinator, lsm_daemon_client_id_t client_id,
    const char *project_key, lsm_daemon_subscription_id_t *subscription_id);
lsm_daemon_subscription_result_t lsm_daemon_watch_subscribe(
    lsm_daemon_coordinator_t *coordinator, lsm_daemon_client_id_t client_id,
    const char *project_key, lsm_daemon_subscription_id_t *subscription_id);
bool lsm_daemon_job_unsubscribe(lsm_daemon_coordinator_t *coordinator,
                                lsm_daemon_client_id_t client_id,
                                lsm_daemon_subscription_id_t subscription_id);
bool lsm_daemon_watch_unsubscribe(lsm_daemon_coordinator_t *coordinator,
                                  lsm_daemon_client_id_t client_id,
                                  lsm_daemon_subscription_id_t subscription_id);

size_t lsm_daemon_job_subscribers(lsm_daemon_coordinator_t *coordinator, const char *project_key);
size_t lsm_daemon_watch_subscribers(lsm_daemon_coordinator_t *coordinator, const char *project_key);
size_t lsm_daemon_active_jobs(lsm_daemon_coordinator_t *coordinator);
size_t lsm_daemon_active_watches(lsm_daemon_coordinator_t *coordinator);
lsm_daemon_job_state_t lsm_daemon_job_state(lsm_daemon_coordinator_t *coordinator,
                                            const char *project_key);

/* Cancellation is two phase. Losing the final subscriber requests cancel;
 * the job remains active until its supervisor reports completion/reaping. */
bool lsm_daemon_job_reaping(lsm_daemon_coordinator_t *coordinator, const char *project_key);
bool lsm_daemon_job_reaped(lsm_daemon_coordinator_t *coordinator, const char *project_key,
                           uint64_t now_ms);
bool lsm_daemon_job_completed(lsm_daemon_coordinator_t *coordinator, const char *project_key,
                              uint64_t now_ms);

/* STOPPING is terminal. Exit is ready only after every job/watch is gone. */
bool lsm_daemon_should_exit(lsm_daemon_coordinator_t *coordinator, uint64_t now_ms);

/* Encode/decode the permanently stable 12-byte "LSMD" rendezvous frame header
 * in network byte order. Detailed operation ABIs live above this framing. */
bool lsm_daemon_frame_header_encode(uint8_t header[LSM_DAEMON_FRAME_HEADER_SIZE],
                                    lsm_daemon_frame_type_t type, uint16_t flags, uint32_t length);
bool lsm_daemon_frame_header_decode(const uint8_t header[LSM_DAEMON_FRAME_HEADER_SIZE],
                                    lsm_daemon_frame_t *frame);

#endif /* LSM_DAEMON_H */
