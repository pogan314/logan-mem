/* version_cohort.c — Short transition lock + process-lifetime SH/EX barrier. */
#include "daemon/version_cohort.h"

#include "foundation/compat.h"
#include "foundation/compat_thread.h"
#include "foundation/log.h"
#include "foundation/platform.h"
#include "foundation/private_file_lock_internal.h"

#include <stdbool.h>
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
    VERSION_COHORT_RETRY_US = 1000,
    VERSION_COHORT_CLEANUP_TIMEOUT_MS = 500,
    VERSION_COHORT_RECORD_MAGIC_SIZE = 8,
    VERSION_COHORT_RECORD_VERSION_OFFSET = VERSION_COHORT_RECORD_MAGIC_SIZE,
    VERSION_COHORT_RECORD_BUILD_OFFSET =
        VERSION_COHORT_RECORD_VERSION_OFFSET + LSM_DAEMON_VERSION_TEXT_SIZE,
    VERSION_COHORT_RECORD_CACHE_OFFSET =
        VERSION_COHORT_RECORD_BUILD_OFFSET + LSM_DAEMON_BUILD_FINGERPRINT_SIZE,
    VERSION_COHORT_RECORD_PROTOCOL_OFFSET =
        VERSION_COHORT_RECORD_CACHE_OFFSET + LSM_DAEMON_BUILD_FINGERPRINT_SIZE,
    VERSION_COHORT_RECORD_STORE_OFFSET = VERSION_COHORT_RECORD_PROTOCOL_OFFSET + 4,
    VERSION_COHORT_RECORD_FEATURE_OFFSET = VERSION_COHORT_RECORD_STORE_OFFSET + 4,
    VERSION_COHORT_RECORD_SIZE = VERSION_COHORT_RECORD_FEATURE_OFFSET + 4,
    VERSION_COHORT_LOG_CAP = 1024 * 1024,
    VERSION_COHORT_PATH_CAP = 4096,
};

static const unsigned char VERSION_COHORT_RECORD_MAGIC[VERSION_COHORT_RECORD_MAGIC_SIZE] = {
    'C', 'B', 'M', 'C', 'O', 'H', 2, 0,
};
static const char VERSION_COHORT_ADMISSION_FILE[] = "lsm-version-cohort-admission-v1.lock";
static const char VERSION_COHORT_LIFETIME_FILE[] = "lsm-version-cohort-lifetime-v1.lock";
static const char VERSION_COHORT_MAINTENANCE_FILE[] = "lsm-version-cohort-maintenance-v1.lock";
static const char VERSION_COHORT_DAEMON_FILE[] = "lsm-version-cohort-daemon-v1.lock";

struct lsm_version_cohort_manager {
    lsm_private_lock_directory_t *directory;
    lsm_mutex_t mutex;
    size_t lease_count;
    uint64_t owner_pid;
    bool closing;
};

struct lsm_version_cohort_lease {
    lsm_version_cohort_manager_t *manager;
    lsm_private_file_lock_t *maintenance;
    lsm_private_file_lock_t *admission;
    lsm_private_file_lock_t *lifetime;
    bool registered;
};

struct lsm_version_cohort_daemon_claim {
    lsm_version_cohort_manager_t *manager;
    lsm_private_file_lock_t *marker;
    bool registered;
};

static uint64_t version_cohort_current_pid(void) {
#ifdef _WIN32
    return (uint64_t)GetCurrentProcessId();
#else
    return (uint64_t)getpid();
#endif
}

static uint64_t version_cohort_deadline_after(uint32_t timeout_ms) {
    uint64_t now = lsm_now_ms();
    return now > UINT64_MAX - timeout_ms ? UINT64_MAX : now + timeout_ms;
}

/* flock's LOCK_NB retries never queue in the kernel, so fairness comes only
 * from the retry schedule. Two participants retrying with the SAME fixed
 * period can phase-lock — every try landing inside the peer's brief hold
 * window — starving an EX acquisition until one side's budget expires. A
 * per-process, time-scattered jitter decorrelates the schedules. */
static void version_cohort_retry_sleep(void) {
    uint64_t mix = (lsm_now_ms() + version_cohort_current_pid()) * 2654435761ULL;
    lsm_usleep(VERSION_COHORT_RETRY_US / 2U + (uint32_t)(mix % VERSION_COHORT_RETRY_US));
}

static lsm_private_file_lock_status_t version_cohort_lock_release_until(
    lsm_private_file_lock_t **lock_io, uint64_t deadline_ms) {
    lsm_private_file_lock_status_t result = LSM_PRIVATE_FILE_LOCK_OK;
    if (!lock_io) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    while (*lock_io) {
        lsm_private_file_lock_status_t status = lsm_private_file_lock_release(lock_io);
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            result = status;
        }
        if (!*lock_io) {
            break;
        }
        if (lsm_now_ms() >= deadline_ms) {
            return result == LSM_PRIVATE_FILE_LOCK_OK ? LSM_PRIVATE_FILE_LOCK_IO : result;
        }
        version_cohort_retry_sleep();
    }
    return result;
}

static _Noreturn void version_cohort_cleanup_fail_stop(const char *component) {
    /* Observer APIs cannot return an opaque cleanup handle. Once their finite
     * retry budget is exhausted, process exit is the only way to avoid losing
     * retry authority while a coordination marker remains owned. */
    lsm_log_error("daemon.forced_shutdown", "component", component, "action",
                  "coordination_cleanup");
    (void)fflush(stdout);
    (void)fflush(stderr);
#ifdef _WIN32
    (void)TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
    abort();
#else
    _exit(EXIT_FAILURE);
#endif
}

#ifndef _WIN32
static void version_cohort_startup_lock_release_complete(lsm_daemon_ipc_startup_lock_t **lock_io) {
    uint64_t deadline = version_cohort_deadline_after(VERSION_COHORT_CLEANUP_TIMEOUT_MS);
    while (lock_io && *lock_io) {
        (void)lsm_daemon_ipc_startup_lock_release(lock_io);
        if (!*lock_io) {
            return;
        }
        if (lsm_now_ms() >= deadline) {
            version_cohort_cleanup_fail_stop("startup_lock_cleanup");
        }
        version_cohort_retry_sleep();
    }
}
#endif

static bool version_cohort_identity_valid(const lsm_daemon_build_identity_t *identity) {
    const char *cache = identity ? identity->cache_fingerprint : NULL;
    if (!cache) {
        return false;
    }
    size_t cache_length = 0;
    while (cache_length < LSM_DAEMON_BUILD_FINGERPRINT_SIZE && cache[cache_length]) {
        unsigned char ch = (unsigned char)cache[cache_length];
        if (!((ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
              (ch >= (unsigned char)'a' && ch <= (unsigned char)'f'))) {
            return false;
        }
        cache_length++;
    }
    if (cache_length != LSM_DAEMON_BUILD_FINGERPRINT_SIZE - 1U || cache[cache_length] != '\0') {
        return false;
    }
    lsm_daemon_conflict_t comparison;
    return lsm_daemon_hello_compare(identity, identity, &comparison) == LSM_DAEMON_HELLO_COMPATIBLE;
}

static void version_cohort_u32_encode(unsigned char out[4], uint32_t value) {
    out[0] = (unsigned char)(value & 0xffU);
    out[1] = (unsigned char)((value >> 8) & 0xffU);
    out[2] = (unsigned char)((value >> 16) & 0xffU);
    out[3] = (unsigned char)((value >> 24) & 0xffU);
}

static uint32_t version_cohort_u32_decode(const unsigned char in[4]) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
           ((uint32_t)in[3] << 24);
}

static bool version_cohort_record_encode(const lsm_daemon_build_identity_t *identity,
                                         unsigned char out[VERSION_COHORT_RECORD_SIZE]) {
    if (!out || !version_cohort_identity_valid(identity)) {
        return false;
    }
    memset(out, 0, VERSION_COHORT_RECORD_SIZE);
    memcpy(out, VERSION_COHORT_RECORD_MAGIC, VERSION_COHORT_RECORD_MAGIC_SIZE);
    size_t version_length = strlen(identity->semantic_version);
    size_t build_length = strlen(identity->build_fingerprint);
    memcpy(out + VERSION_COHORT_RECORD_VERSION_OFFSET, identity->semantic_version,
           version_length + 1);
    memcpy(out + VERSION_COHORT_RECORD_BUILD_OFFSET, identity->build_fingerprint, build_length + 1);
    if (identity->cache_fingerprint) {
        size_t cache_length = strlen(identity->cache_fingerprint);
        memcpy(out + VERSION_COHORT_RECORD_CACHE_OFFSET, identity->cache_fingerprint,
               cache_length + 1);
    }
    version_cohort_u32_encode(out + VERSION_COHORT_RECORD_PROTOCOL_OFFSET, identity->protocol_abi);
    version_cohort_u32_encode(out + VERSION_COHORT_RECORD_STORE_OFFSET, identity->store_abi);
    version_cohort_u32_encode(out + VERSION_COHORT_RECORD_FEATURE_OFFSET, identity->feature_abi);
    return true;
}

static bool version_cohort_record_decode(const unsigned char record[VERSION_COHORT_RECORD_SIZE],
                                         size_t length, lsm_daemon_build_identity_t *identity_out,
                                         char version_out[LSM_DAEMON_VERSION_TEXT_SIZE],
                                         char build_out[LSM_DAEMON_BUILD_FINGERPRINT_SIZE],
                                         char cache_out[LSM_DAEMON_BUILD_FINGERPRINT_SIZE]) {
    if (!record || length != VERSION_COHORT_RECORD_SIZE || !identity_out || !version_out ||
        !build_out || !cache_out ||
        memcmp(record, VERSION_COHORT_RECORD_MAGIC, VERSION_COHORT_RECORD_MAGIC_SIZE) != 0) {
        return false;
    }
    memcpy(version_out, record + VERSION_COHORT_RECORD_VERSION_OFFSET,
           LSM_DAEMON_VERSION_TEXT_SIZE);
    memcpy(build_out, record + VERSION_COHORT_RECORD_BUILD_OFFSET,
           LSM_DAEMON_BUILD_FINGERPRINT_SIZE);
    memcpy(cache_out, record + VERSION_COHORT_RECORD_CACHE_OFFSET,
           LSM_DAEMON_BUILD_FINGERPRINT_SIZE);
    lsm_daemon_build_identity_t identity = {
        .semantic_version = version_out,
        .build_fingerprint = build_out,
        .cache_fingerprint = cache_out[0] ? cache_out : NULL,
        .protocol_abi = version_cohort_u32_decode(record + VERSION_COHORT_RECORD_PROTOCOL_OFFSET),
        .store_abi = version_cohort_u32_decode(record + VERSION_COHORT_RECORD_STORE_OFFSET),
        .feature_abi = version_cohort_u32_decode(record + VERSION_COHORT_RECORD_FEATURE_OFFSET),
    };
    if (!version_cohort_identity_valid(&identity)) {
        return false;
    }
    *identity_out = identity;
    return true;
}

static lsm_version_cohort_status_t version_cohort_status_from_lock(
    lsm_private_file_lock_status_t status) {
    switch (status) {
    case LSM_PRIVATE_FILE_LOCK_OK:
        return LSM_VERSION_COHORT_OK;
    case LSM_PRIVATE_FILE_LOCK_BUSY:
        return LSM_VERSION_COHORT_BUSY;
    case LSM_PRIVATE_FILE_LOCK_UNSAFE:
        return LSM_VERSION_COHORT_UNSAFE;
    case LSM_PRIVATE_FILE_LOCK_IO:
    default:
        return LSM_VERSION_COHORT_IO;
    }
}

static lsm_private_file_lock_status_t version_cohort_lock_until(
    lsm_version_cohort_manager_t *manager, const char *base_name, lsm_private_file_lock_mode_t mode,
    uint64_t deadline_ms, lsm_private_file_lock_t **lock_out) {
    for (;;) {
        lsm_private_file_lock_status_t status =
            lsm_private_file_lock_try_acquire(manager->directory, base_name, mode, lock_out);
        if (status != LSM_PRIVATE_FILE_LOCK_BUSY ||
            (deadline_ms != UINT64_MAX && lsm_now_ms() >= deadline_ms)) {
            return status;
        }
        version_cohort_retry_sleep();
    }
}

static lsm_version_cohort_lease_t *version_cohort_lease_new(lsm_version_cohort_manager_t *manager) {
    if (!manager) {
        return NULL;
    }
    lsm_version_cohort_lease_t *lease = calloc(1, sizeof(*lease));
    if (!lease) {
        return NULL;
    }
    lsm_mutex_lock(&manager->mutex);
    bool admitted = !manager->closing && manager->owner_pid == version_cohort_current_pid();
    if (admitted) {
        manager->lease_count++;
    }
    lsm_mutex_unlock(&manager->mutex);
    if (!admitted) {
        free(lease);
        return NULL;
    }
    lease->manager = manager;
    lease->registered = true;
    return lease;
}

static bool version_cohort_manager_register(lsm_version_cohort_manager_t *manager) {
    if (!manager) {
        return false;
    }
    lsm_mutex_lock(&manager->mutex);
    bool admitted = !manager->closing && manager->owner_pid == version_cohort_current_pid();
    if (admitted) {
        manager->lease_count++;
    }
    lsm_mutex_unlock(&manager->mutex);
    return admitted;
}

static bool version_cohort_manager_unregister(lsm_version_cohort_manager_t *manager) {
    if (!manager) {
        return false;
    }
    lsm_mutex_lock(&manager->mutex);
    bool registered =
        manager->owner_pid == version_cohort_current_pid() && manager->lease_count > 0;
    if (registered) {
        manager->lease_count--;
    }
    lsm_mutex_unlock(&manager->mutex);
    return registered;
}

lsm_private_file_lock_status_t lsm_version_cohort_lease_release(
    lsm_version_cohort_lease_t **lease_io) {
    if (!lease_io || !*lease_io) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_version_cohort_lease_t *lease = *lease_io;
    lsm_private_file_lock_status_t result = LSM_PRIVATE_FILE_LOCK_OK;
    if (lease->lifetime) {
        lsm_private_file_lock_status_t status = lsm_private_file_lock_release(&lease->lifetime);
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            result = status;
        }
    }
    if (lease->lifetime) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (lease->admission) {
        lsm_private_file_lock_status_t status = lsm_private_file_lock_release(&lease->admission);
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            result = status;
        }
    }
    if (lease->admission) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (lease->maintenance) {
        lsm_private_file_lock_status_t status = lsm_private_file_lock_release(&lease->maintenance);
        if (status != LSM_PRIVATE_FILE_LOCK_OK) {
            result = status;
        }
    }
    if (lease->maintenance) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (lease->registered && lease->manager) {
        lsm_mutex_lock(&lease->manager->mutex);
        if (lease->manager->lease_count > 0) {
            lease->manager->lease_count--;
        } else {
            result = LSM_PRIVATE_FILE_LOCK_IO;
        }
        lsm_mutex_unlock(&lease->manager->mutex);
        lease->registered = false;
    }
    free(lease);
    *lease_io = NULL;
    return result;
}

static lsm_version_cohort_status_t version_cohort_failed(lsm_version_cohort_lease_t *lease,
                                                         lsm_version_cohort_status_t status,
                                                         lsm_version_cohort_lease_t **lease_out) {
    lsm_version_cohort_lease_t *cleanup = lease;
    lsm_private_file_lock_status_t cleanup_status = lsm_version_cohort_lease_release(&cleanup);
    if (cleanup) {
        *lease_out = cleanup;
        return LSM_VERSION_COHORT_IO;
    }
    return cleanup_status == LSM_PRIVATE_FILE_LOCK_OK ? status : LSM_VERSION_COHORT_IO;
}

lsm_version_cohort_manager_t *lsm_version_cohort_manager_new(
    const lsm_daemon_ipc_endpoint_t *endpoint) {
    lsm_private_lock_directory_t *directory = NULL;
    lsm_private_file_lock_status_t directory_status =
        lsm_daemon_ipc_private_lock_directory_new(endpoint, &directory);
    if (directory_status != LSM_PRIVATE_FILE_LOCK_OK || !directory) {
        if (directory) {
            lsm_private_lock_directory_close(directory);
        }
        return NULL;
    }
    lsm_version_cohort_manager_t *manager = calloc(1, sizeof(*manager));
    if (!manager) {
        lsm_private_lock_directory_close(directory);
        return NULL;
    }
    manager->directory = directory;
    manager->owner_pid = version_cohort_current_pid();
    lsm_mutex_init(&manager->mutex);
    return manager;
}

static lsm_version_cohort_status_t version_cohort_claim_new(
    lsm_version_cohort_lease_t *lease, const lsm_daemon_build_identity_t *identity,
    uint64_t deadline_ms) {
    unsigned char record[VERSION_COHORT_RECORD_SIZE];
    if (!version_cohort_record_encode(identity, record) ||
        lsm_private_file_lock_payload_write(lease->lifetime, record, sizeof(record)) !=
            LSM_PRIVATE_FILE_LOCK_OK) {
        return LSM_VERSION_COHORT_IO;
    }
    lsm_private_file_lock_status_t release_status = lsm_private_file_lock_release(&lease->lifetime);
    if (release_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return LSM_VERSION_COHORT_IO;
    }
    return version_cohort_status_from_lock(
        version_cohort_lock_until(lease->manager, VERSION_COHORT_LIFETIME_FILE,
                                  LSM_PRIVATE_FILE_LOCK_SH, deadline_ms, &lease->lifetime));
}

lsm_version_cohort_status_t lsm_version_cohort_acquire(lsm_version_cohort_manager_t *manager,
                                                       const lsm_daemon_build_identity_t *identity,
                                                       uint64_t deadline_ms,
                                                       lsm_version_cohort_lease_t **lease_out,
                                                       lsm_daemon_conflict_t *conflict_out) {
    if (lease_out) {
        *lease_out = NULL;
    }
    if (conflict_out) {
        memset(conflict_out, 0, sizeof(*conflict_out));
    }
    if (!manager || !identity || !lease_out || !conflict_out ||
        !version_cohort_identity_valid(identity)) {
        return LSM_VERSION_COHORT_UNSAFE;
    }
    lsm_version_cohort_lease_t *lease = version_cohort_lease_new(manager);
    if (!lease) {
        return LSM_VERSION_COHORT_IO;
    }
    /* Global order is maintenance -> admission -> lifetime. Retaining SH
     * through admission closes the probe/admission race: an activation cannot
     * publish EX intent until this already-started transition has finished,
     * while every later participant fails its non-blocking SH attempt. */
    lsm_private_file_lock_status_t lock_status =
        lsm_private_file_lock_try_acquire(manager->directory, VERSION_COHORT_MAINTENANCE_FILE,
                                          LSM_PRIVATE_FILE_LOCK_SH, &lease->maintenance);
    if (lock_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, version_cohort_status_from_lock(lock_status),
                                     lease_out);
    }
    lock_status =
        version_cohort_lock_until(manager, VERSION_COHORT_ADMISSION_FILE, LSM_PRIVATE_FILE_LOCK_EX,
                                  deadline_ms, &lease->admission);
    if (lock_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, version_cohort_status_from_lock(lock_status),
                                     lease_out);
    }

    lock_status =
        lsm_private_file_lock_try_acquire(manager->directory, VERSION_COHORT_LIFETIME_FILE,
                                          LSM_PRIVATE_FILE_LOCK_EX, &lease->lifetime);
    lsm_version_cohort_status_t status = version_cohort_status_from_lock(lock_status);
    if (status == LSM_VERSION_COHORT_OK) {
        /* No live holder: this participant CLAIMS the cohort and NO identity
         * comparison happens — a mismatched build is admitted, not refused.
         * That is correct when nothing is running, and it is the only path by
         * which a mismatched client can join. Name it: "was the lifetime lock
         * held?" is exactly what separates a local run (conflict raised) from
         * a CI run (client admitted), and it was not observable in any log. */
        lsm_log_info("version_cohort.claimed_unheld", "build",
                     identity->build_fingerprint ? identity->build_fingerprint : "<null>");
        status = version_cohort_claim_new(lease, identity, deadline_ms);
    } else if (status == LSM_VERSION_COHORT_BUSY) {
        lock_status =
            lsm_private_file_lock_try_acquire(manager->directory, VERSION_COHORT_LIFETIME_FILE,
                                              LSM_PRIVATE_FILE_LOCK_SH, &lease->lifetime);
        status = version_cohort_status_from_lock(lock_status);
        if (status == LSM_VERSION_COHORT_OK) {
            unsigned char record[VERSION_COHORT_RECORD_SIZE];
            size_t record_length = 0;
            char active_version[LSM_DAEMON_VERSION_TEXT_SIZE];
            char active_build[LSM_DAEMON_BUILD_FINGERPRINT_SIZE];
            char active_cache[LSM_DAEMON_BUILD_FINGERPRINT_SIZE];
            lsm_daemon_build_identity_t active;
            lsm_private_file_lock_status_t payload_status = lsm_private_file_lock_payload_read(
                lease->lifetime, record, sizeof(record), &record_length);
            bool decoded = payload_status == LSM_PRIVATE_FILE_LOCK_OK &&
                           version_cohort_record_decode(record, record_length, &active,
                                                        active_version, active_build, active_cache);
            if (!decoded) {
                /* The active lifetime record could not be read/decoded from a
                 * peer holder - names whether the cross-process payload was
                 * unreadable vs present-but-malformed (a Windows lock-payload
                 * visibility question the unix flock path does not hit). */
                char len_text[16];
                (void)snprintf(len_text, sizeof(len_text), "%zu", record_length);
                lsm_log_warn("version_cohort.record_undecodable", "payload_status",
                             payload_status == LSM_PRIVATE_FILE_LOCK_OK ? "read_ok" : "read_failed",
                             "bytes", len_text);
                status = LSM_VERSION_COHORT_UNSAFE;
            } else {
                lsm_daemon_hello_status_t comparison =
                    lsm_daemon_hello_compare(&active, identity, conflict_out);
                if (comparison == LSM_DAEMON_HELLO_INVALID) {
                    /* The record decoded but hello_compare rejected an identity
                     * field as invalid (empty version/fingerprint/cache). Name
                     * the decoded active values so the malformed field on the
                     * Windows cross-process read path is visible. */
                    lsm_log_warn(
                        "version_cohort.active_identity_invalid", "version",
                        active.semantic_version ? active.semantic_version : "<null>", "build",
                        active.build_fingerprint ? active.build_fingerprint : "<null>", "cache",
                        active.cache_fingerprint ? active.cache_fingerprint : "<null>");
                }
                const char *active_cache_fingerprint =
                    active.cache_fingerprint ? active.cache_fingerprint : "";
                const char *requested_cache_fingerprint =
                    identity->cache_fingerprint ? identity->cache_fingerprint : "";
                if (comparison == LSM_DAEMON_HELLO_COMPATIBLE &&
                    strcmp(active_cache_fingerprint, requested_cache_fingerprint) != 0) {
                    comparison = LSM_DAEMON_HELLO_CACHE_CONFLICT;
                    conflict_out->status = comparison;
                    (void)snprintf(conflict_out->active_cache_fingerprint,
                                   sizeof(conflict_out->active_cache_fingerprint), "%s",
                                   active_cache_fingerprint);
                    (void)snprintf(conflict_out->requested_cache_fingerprint,
                                   sizeof(conflict_out->requested_cache_fingerprint), "%s",
                                   requested_cache_fingerprint);
                }
                if (comparison == LSM_DAEMON_HELLO_COMPATIBLE) {
                    status = LSM_VERSION_COHORT_OK;
                } else if (comparison == LSM_DAEMON_HELLO_INVALID) {
                    status = LSM_VERSION_COHORT_UNSAFE;
                } else {
                    lsm_private_file_lock_status_t release_status =
                        lsm_private_file_lock_release(&lease->lifetime);
                    if (release_status != LSM_PRIVATE_FILE_LOCK_OK) {
                        status = LSM_VERSION_COHORT_IO;
                    } else {
                        lock_status = lsm_private_file_lock_try_acquire(
                            manager->directory, VERSION_COHORT_LIFETIME_FILE,
                            LSM_PRIVATE_FILE_LOCK_EX, &lease->lifetime);
                        status = version_cohort_status_from_lock(lock_status);
                        if (status == LSM_VERSION_COHORT_OK) {
                            status = version_cohort_claim_new(lease, identity, deadline_ms);
                        } else if (status == LSM_VERSION_COHORT_BUSY) {
                            status = LSM_VERSION_COHORT_CONFLICT;
                        }
                    }
                }
            }
        }
    }

    if (status != LSM_VERSION_COHORT_OK) {
        return version_cohort_failed(lease, status, lease_out);
    }
    lsm_private_file_lock_status_t admission_release =
        lsm_private_file_lock_release(&lease->admission);
    if (admission_release != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, LSM_VERSION_COHORT_IO, lease_out);
    }
    lsm_private_file_lock_status_t maintenance_release =
        lsm_private_file_lock_release(&lease->maintenance);
    if (maintenance_release != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, LSM_VERSION_COHORT_IO, lease_out);
    }
    *lease_out = lease;
    return LSM_VERSION_COHORT_OK;
}

static lsm_version_cohort_status_t version_cohort_reserve_for_mutation_internal(
    lsm_version_cohort_manager_t *manager, uint64_t deadline_ms,
    lsm_version_cohort_quiesce_fn quiesce, void *quiesce_context,
    lsm_version_cohort_quiesce_result_t *quiesce_result_out, lsm_version_cohort_lease_t **lease_out,
    bool require_finite_deadline) {
    if (lease_out) {
        *lease_out = NULL;
    }
    if (quiesce_result_out) {
        *quiesce_result_out = LSM_VERSION_COHORT_QUIESCE_NOT_NEEDED;
    }
    if (!manager || !quiesce_result_out || !lease_out ||
        (require_finite_deadline && deadline_ms == UINT64_MAX)) {
        return LSM_VERSION_COHORT_UNSAFE;
    }
    lsm_version_cohort_lease_t *lease = version_cohort_lease_new(manager);
    if (!lease) {
        return LSM_VERSION_COHORT_IO;
    }
    /* Publish crash-safe intent before waiting for an admission already in
     * flight. Normal admissions retain maintenance SH until their admission
     * transition ends, giving every participant the same deadlock-free native
     * order: maintenance -> admission -> lifetime. */
    lsm_private_file_lock_status_t lock_status =
        version_cohort_lock_until(manager, VERSION_COHORT_MAINTENANCE_FILE,
                                  LSM_PRIVATE_FILE_LOCK_EX, deadline_ms, &lease->maintenance);
    if (lock_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, version_cohort_status_from_lock(lock_status),
                                     lease_out);
    }
    lock_status =
        version_cohort_lock_until(manager, VERSION_COHORT_ADMISSION_FILE, LSM_PRIVATE_FILE_LOCK_EX,
                                  deadline_ms, &lease->admission);
    if (lock_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, version_cohort_status_from_lock(lock_status),
                                     lease_out);
    }
    lock_status =
        lsm_private_file_lock_try_acquire(manager->directory, VERSION_COHORT_LIFETIME_FILE,
                                          LSM_PRIVATE_FILE_LOCK_EX, &lease->lifetime);
    if (lock_status == LSM_PRIVATE_FILE_LOCK_BUSY) {
        if (!quiesce) {
            *quiesce_result_out = LSM_VERSION_COHORT_QUIESCE_REFUSED;
            return version_cohort_failed(lease, LSM_VERSION_COHORT_BUSY, lease_out);
        }

        /* Admission remains exclusively locked across the callback and wait.
         * Consequently the active lifetime set can only shrink, and no new
         * participant can race mutation after accepting the quiesce request. */
        lsm_version_cohort_quiesce_result_t quiesce_result = quiesce(quiesce_context);
        *quiesce_result_out = quiesce_result;
        if (quiesce_result == LSM_VERSION_COHORT_QUIESCE_REFUSED) {
            return version_cohort_failed(lease, LSM_VERSION_COHORT_BUSY, lease_out);
        }
        if (quiesce_result == LSM_VERSION_COHORT_QUIESCE_ERROR) {
            return version_cohort_failed(lease, LSM_VERSION_COHORT_IO, lease_out);
        }
        if (quiesce_result != LSM_VERSION_COHORT_QUIESCE_REQUESTED) {
            return version_cohort_failed(lease, LSM_VERSION_COHORT_UNSAFE, lease_out);
        }
        lock_status =
            version_cohort_lock_until(manager, VERSION_COHORT_LIFETIME_FILE,
                                      LSM_PRIVATE_FILE_LOCK_EX, deadline_ms, &lease->lifetime);
    }
    if (lock_status != LSM_PRIVATE_FILE_LOCK_OK) {
        return version_cohort_failed(lease, version_cohort_status_from_lock(lock_status),
                                     lease_out);
    }

    /* All three locks intentionally remain in the lease. Maintenance makes
     * admission fail fast, admission closes the transition, and lifetime
     * proves all earlier participants have drained. */
    *lease_out = lease;
    return LSM_VERSION_COHORT_OK;
}

lsm_version_cohort_status_t lsm_version_cohort_reserve_for_mutation(
    lsm_version_cohort_manager_t *manager, uint64_t deadline_ms,
    lsm_version_cohort_quiesce_fn quiesce, void *quiesce_context,
    lsm_version_cohort_quiesce_result_t *quiesce_result_out,
    lsm_version_cohort_lease_t **lease_out) {
    return version_cohort_reserve_for_mutation_internal(
        manager, deadline_ms, quiesce, quiesce_context, quiesce_result_out, lease_out, true);
}

lsm_version_cohort_status_t lsm_version_cohort_reserve_exclusive(
    lsm_version_cohort_manager_t *manager, uint64_t deadline_ms,
    lsm_version_cohort_lease_t **lease_out) {
    lsm_version_cohort_quiesce_result_t ignored_quiesce;
    return version_cohort_reserve_for_mutation_internal(manager, deadline_ms, NULL, NULL,
                                                        &ignored_quiesce, lease_out, false);
}

static lsm_version_cohort_maintenance_presence_t version_cohort_maintenance_presence_internal(
    lsm_version_cohort_manager_t *manager, bool terminal_observer) {
    if (!manager || !version_cohort_manager_register(manager)) {
        return LSM_VERSION_COHORT_MAINTENANCE_UNSAFE;
    }

    lsm_version_cohort_maintenance_presence_t presence = LSM_VERSION_COHORT_MAINTENANCE_IO;
    lsm_private_file_lock_t *observer = NULL;
    lsm_private_file_lock_status_t status = lsm_private_file_lock_try_acquire(
        manager->directory, VERSION_COHORT_MAINTENANCE_FILE, LSM_PRIVATE_FILE_LOCK_SH, &observer);
    if (status == LSM_PRIVATE_FILE_LOCK_OK && observer) {
        presence = LSM_VERSION_COHORT_MAINTENANCE_ABSENT;
    } else if (status == LSM_PRIVATE_FILE_LOCK_BUSY) {
        presence = LSM_VERSION_COHORT_MAINTENANCE_REQUESTED;
    } else if (status == LSM_PRIVATE_FILE_LOCK_UNSAFE) {
        presence = LSM_VERSION_COHORT_MAINTENANCE_UNSAFE;
    }

    lsm_private_file_lock_status_t observer_release = version_cohort_lock_release_until(
        &observer, version_cohort_deadline_after(VERSION_COHORT_CLEANUP_TIMEOUT_MS));
    if (observer_release != LSM_PRIVATE_FILE_LOCK_OK) {
        presence = LSM_VERSION_COHORT_MAINTENANCE_IO;
    }
    if (observer) {
        if (terminal_observer) {
            /* The caller's contract is immediate no-I/O process termination.
             * Returning the error preserves that terminal thread's ability to
             * exit even when structured logging or stdio is backpressured. The
             * native handle intentionally remains process-owned until exit. */
            return LSM_VERSION_COHORT_MAINTENANCE_IO;
        }
        version_cohort_cleanup_fail_stop("maintenance_observer_cleanup");
    }
    if (!version_cohort_manager_unregister(manager)) {
        presence = LSM_VERSION_COHORT_MAINTENANCE_IO;
    }
    return presence;
}

lsm_version_cohort_maintenance_presence_t lsm_version_cohort_maintenance_presence(
    lsm_version_cohort_manager_t *manager) {
    return version_cohort_maintenance_presence_internal(manager, false);
}

lsm_version_cohort_maintenance_presence_t lsm_version_cohort_maintenance_presence_terminal(
    lsm_version_cohort_manager_t *manager) {
    return version_cohort_maintenance_presence_internal(manager, true);
}

lsm_version_cohort_status_t lsm_version_cohort_daemon_claim_acquire(
    lsm_version_cohort_manager_t *manager, lsm_version_cohort_daemon_claim_t **claim_out) {
    if (claim_out) {
        *claim_out = NULL;
    }
    if (!manager || !claim_out) {
        return LSM_VERSION_COHORT_UNSAFE;
    }
    lsm_version_cohort_daemon_claim_t *claim = calloc(1, sizeof(*claim));
    if (!claim || !version_cohort_manager_register(manager)) {
        free(claim);
        return LSM_VERSION_COHORT_IO;
    }
    claim->manager = manager;
    claim->registered = true;
    lsm_private_file_lock_status_t lock_status = lsm_private_file_lock_try_acquire(
        manager->directory, VERSION_COHORT_DAEMON_FILE, LSM_PRIVATE_FILE_LOCK_EX, &claim->marker);
    if (lock_status == LSM_PRIVATE_FILE_LOCK_OK) {
        *claim_out = claim;
        return LSM_VERSION_COHORT_OK;
    }

    lsm_private_file_lock_status_t cleanup_status = version_cohort_lock_release_until(
        &claim->marker, version_cohort_deadline_after(VERSION_COHORT_CLEANUP_TIMEOUT_MS));
    if (claim->marker) {
        *claim_out = claim;
        return LSM_VERSION_COHORT_IO;
    }
    bool unregistered = version_cohort_manager_unregister(manager);
    free(claim);
    return cleanup_status == LSM_PRIVATE_FILE_LOCK_OK && unregistered
               ? version_cohort_status_from_lock(lock_status)
               : LSM_VERSION_COHORT_IO;
}

lsm_private_file_lock_status_t lsm_version_cohort_daemon_claim_release(
    lsm_version_cohort_daemon_claim_t **claim_io) {
    if (!claim_io || !*claim_io) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_version_cohort_daemon_claim_t *claim = *claim_io;
    lsm_private_file_lock_status_t result = LSM_PRIVATE_FILE_LOCK_OK;
    if (claim->marker) {
        result = lsm_private_file_lock_release(&claim->marker);
    }
    if (claim->marker) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    if (claim->registered) {
        if (!version_cohort_manager_unregister(claim->manager)) {
            result = LSM_PRIVATE_FILE_LOCK_IO;
        }
        claim->registered = false;
    }
    free(claim);
    *claim_io = NULL;
    return result;
}

lsm_version_cohort_daemon_presence_t lsm_version_cohort_daemon_claim_presence(
    lsm_version_cohort_manager_t *manager) {
    if (!manager || !version_cohort_manager_register(manager)) {
        return LSM_VERSION_COHORT_DAEMON_UNSAFE;
    }

    lsm_version_cohort_daemon_presence_t presence = LSM_VERSION_COHORT_DAEMON_IO;
    lsm_private_file_lock_t *marker = NULL;
    lsm_private_file_lock_status_t marker_status = lsm_private_file_lock_try_acquire(
        manager->directory, VERSION_COHORT_DAEMON_FILE, LSM_PRIVATE_FILE_LOCK_EX, &marker);
    if (marker_status == LSM_PRIVATE_FILE_LOCK_BUSY) {
        presence = LSM_VERSION_COHORT_DAEMON_COORDINATED;
    } else if (marker_status == LSM_PRIVATE_FILE_LOCK_OK && marker) {
        presence = LSM_VERSION_COHORT_DAEMON_ABSENT;
    } else if (marker_status == LSM_PRIVATE_FILE_LOCK_UNSAFE) {
        presence = LSM_VERSION_COHORT_DAEMON_UNSAFE;
    }

    lsm_private_file_lock_status_t marker_release = version_cohort_lock_release_until(
        &marker, version_cohort_deadline_after(VERSION_COHORT_CLEANUP_TIMEOUT_MS));
    if (marker_release != LSM_PRIVATE_FILE_LOCK_OK) {
        presence = LSM_VERSION_COHORT_DAEMON_IO;
    }
    if (marker) {
        version_cohort_cleanup_fail_stop("daemon_marker_observer_cleanup");
    }
    if (!version_cohort_manager_unregister(manager)) {
        presence = LSM_VERSION_COHORT_DAEMON_IO;
    }
    return presence;
}

static lsm_version_cohort_daemon_presence_t version_cohort_active_daemon_presence(
    lsm_version_cohort_manager_t *manager, const lsm_daemon_ipc_endpoint_t *endpoint,
    const lsm_daemon_ipc_local_transition_t *transition) {
    lsm_version_cohort_daemon_presence_t presence = LSM_VERSION_COHORT_DAEMON_IO;
    lsm_private_file_lock_t *marker = NULL;
    lsm_private_file_lock_status_t marker_status = lsm_private_file_lock_try_acquire(
        manager->directory, VERSION_COHORT_DAEMON_FILE, LSM_PRIVATE_FILE_LOCK_EX, &marker);
    if (marker_status == LSM_PRIVATE_FILE_LOCK_BUSY) {
        presence = LSM_VERSION_COHORT_DAEMON_COORDINATED;
    } else if (marker_status == LSM_PRIVATE_FILE_LOCK_OK) {
        if (!transition) {
            presence = LSM_VERSION_COHORT_DAEMON_UNCOORDINATED;
        } else {
            /* The sealed transition prevents a replacement daemon from
             * starting. Recheck lifetime while retaining the marker EX lock:
             * shutdown may have released lifetime after our first observation
             * but before the marker became available. */
            int lifetime = lsm_daemon_ipc_local_transition_lifetime_probe(endpoint, transition);
            presence = lifetime == 0   ? LSM_VERSION_COHORT_DAEMON_ABSENT
                       : lifetime == 1 ? LSM_VERSION_COHORT_DAEMON_UNCOORDINATED
                                       : LSM_VERSION_COHORT_DAEMON_UNSAFE;
        }
    } else if (marker_status == LSM_PRIVATE_FILE_LOCK_UNSAFE) {
        presence = LSM_VERSION_COHORT_DAEMON_UNSAFE;
    }
    lsm_private_file_lock_status_t marker_release = version_cohort_lock_release_until(
        &marker, version_cohort_deadline_after(VERSION_COHORT_CLEANUP_TIMEOUT_MS));
    if (marker_release != LSM_PRIVATE_FILE_LOCK_OK) {
        presence = LSM_VERSION_COHORT_DAEMON_IO;
    }
    if (marker) {
        version_cohort_cleanup_fail_stop("active_daemon_marker_cleanup");
    }
    return presence;
}

lsm_version_cohort_daemon_presence_t lsm_version_cohort_daemon_presence(
    lsm_version_cohort_manager_t *manager, const lsm_daemon_ipc_endpoint_t *endpoint) {
    if (!manager || !endpoint || !version_cohort_manager_register(manager)) {
        return LSM_VERSION_COHORT_DAEMON_UNSAFE;
    }

    lsm_version_cohort_daemon_presence_t presence = LSM_VERSION_COHORT_DAEMON_IO;
    lsm_version_cohort_daemon_presence_t claim = lsm_version_cohort_daemon_claim_presence(manager);
    if (claim == LSM_VERSION_COHORT_DAEMON_COORDINATED) {
        presence = claim;
    } else if (claim != LSM_VERSION_COHORT_DAEMON_ABSENT) {
        presence = claim;
    } else {
        int lifetime = lsm_daemon_ipc_lifetime_reservation_probe(endpoint);
        if (lifetime == 0) {
#ifdef _WIN32
            int legacy = lsm_daemon_ipc_legacy_generation_probe(endpoint);
            if (legacy == 0) {
                presence = LSM_VERSION_COHORT_DAEMON_ABSENT;
            } else if (legacy == 1) {
                presence = LSM_VERSION_COHORT_DAEMON_UNCOORDINATED;
            } else {
                presence = LSM_VERSION_COHORT_DAEMON_UNSAFE;
            }
#else
            lsm_daemon_ipc_startup_lock_t *startup = NULL;
            int startup_status = lsm_daemon_ipc_startup_lock_try_acquire(endpoint, &startup);
            if (startup_status == 0) {
                presence = LSM_VERSION_COHORT_DAEMON_UNCOORDINATED;
            } else if (startup_status < 0 || !startup) {
                presence = LSM_VERSION_COHORT_DAEMON_UNSAFE;
            } else {
                int cleanup = lsm_daemon_ipc_stale_generation_cleanup(endpoint, startup);
                if (cleanup == 1) {
                    presence = LSM_VERSION_COHORT_DAEMON_ABSENT;
                } else if (cleanup == 0) {
                    presence = LSM_VERSION_COHORT_DAEMON_UNCOORDINATED;
                } else {
                    presence = LSM_VERSION_COHORT_DAEMON_UNSAFE;
                }
            }
            version_cohort_startup_lock_release_complete(&startup);
#endif
        } else if (lifetime == 1) {
            presence = version_cohort_active_daemon_presence(manager, NULL, NULL);
        }
    }
    if (!version_cohort_manager_unregister(manager)) {
        presence = LSM_VERSION_COHORT_DAEMON_IO;
    }
    return presence;
}

lsm_version_cohort_daemon_presence_t lsm_version_cohort_daemon_presence_under_transition(
    lsm_version_cohort_manager_t *manager, const lsm_daemon_ipc_endpoint_t *endpoint,
    const lsm_daemon_ipc_local_transition_t *transition) {
    if (!manager || !endpoint || !transition || !version_cohort_manager_register(manager)) {
        return LSM_VERSION_COHORT_DAEMON_UNSAFE;
    }

    int transition_status = lsm_daemon_ipc_local_transition_lifetime_probe(endpoint, transition);
    /* After validating the guard, classify through the marker. If it is free,
     * keep it exclusively locked while observing lifetime so daemon shutdown
     * cannot become a false conflict. The retained startup transition prevents
     * a replacement generation. */
    lsm_version_cohort_daemon_presence_t presence =
        transition_status < 0
            ? LSM_VERSION_COHORT_DAEMON_UNSAFE
            : version_cohort_active_daemon_presence(manager, endpoint, transition);
    if (!version_cohort_manager_unregister(manager)) {
        presence = LSM_VERSION_COHORT_DAEMON_IO;
    }
    return presence;
}

lsm_private_file_lock_status_t lsm_version_cohort_manager_free(
    lsm_version_cohort_manager_t **manager_io) {
    if (!manager_io || !*manager_io) {
        return LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_version_cohort_manager_t *manager = *manager_io;
    lsm_mutex_lock(&manager->mutex);
    bool leases_active = manager->lease_count != 0;
    bool can_free =
        manager->owner_pid == version_cohort_current_pid() && !leases_active && !manager->closing;
    if (can_free) {
        manager->closing = true;
    }
    lsm_mutex_unlock(&manager->mutex);
    if (!can_free) {
        return leases_active ? LSM_PRIVATE_FILE_LOCK_BUSY : LSM_PRIVATE_FILE_LOCK_IO;
    }
    lsm_private_lock_directory_close(manager->directory);
    manager->directory = NULL;
    lsm_mutex_destroy(&manager->mutex);
    free(manager);
    *manager_io = NULL;
    return LSM_PRIVATE_FILE_LOCK_OK;
}

bool lsm_version_cohort_log_conflict(const lsm_daemon_conflict_t *conflict) {
    const char *cache = lsm_resolve_cache_dir();
    if (!cache || !cache[0] || !conflict) {
        return false;
    }
    char logs[VERSION_COHORT_PATH_CAP];
    char path[VERSION_COHORT_PATH_CAP];
    int logs_written = snprintf(logs, sizeof(logs), "%s/logs", cache);
    int path_written = snprintf(path, sizeof(path), "%s/daemon-conflicts.ndjson", logs);
    if (logs_written <= 0 || logs_written >= (int)sizeof(logs) || path_written <= 0 ||
        path_written >= (int)sizeof(path)) {
        return false;
    }
    FILE *seed =
        lsm_daemon_ipc_private_log_open(logs, "daemon-conflicts.ndjson", VERSION_COHORT_LOG_CAP);
    if (!seed) {
        return false;
    }
    bool seeded = fclose(seed) == 0;
    return seeded && lsm_daemon_conflict_log_append(path, conflict, VERSION_COHORT_LOG_CAP);
}

bool lsm_version_cohort_log_uncoordinated_daemon(const lsm_daemon_build_identity_t *requested) {
    if (!version_cohort_identity_valid(requested)) {
        return false;
    }
    lsm_daemon_conflict_t conflict = {
        .status = LSM_DAEMON_HELLO_BUILD_CONFLICT,
    };
    (void)snprintf(conflict.active_version, sizeof(conflict.active_version), "%s",
                   "pre-cohort/unknown");
    memset(conflict.active_build_fingerprint, '0', sizeof(conflict.active_build_fingerprint) - 1);
    conflict.active_build_fingerprint[sizeof(conflict.active_build_fingerprint) - 1] = '\0';
    (void)snprintf(conflict.requested_version, sizeof(conflict.requested_version), "%s",
                   requested->semantic_version);
    (void)snprintf(conflict.requested_build_fingerprint,
                   sizeof(conflict.requested_build_fingerprint), "%s",
                   requested->build_fingerprint);
    return lsm_version_cohort_log_conflict(&conflict);
}
