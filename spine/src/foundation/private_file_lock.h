/*
 * private_file_lock.h — Secure locks inside a prevalidated private directory.
 *
 * This is an internal foundation primitive. It deliberately does not choose a
 * product runtime path; callers must supply an opaque directory handle created
 * by the platform runtime-path layer.
 */
#ifndef LSM_PRIVATE_FILE_LOCK_H
#define LSM_PRIVATE_FILE_LOCK_H

#include <stdint.h>

typedef enum {
    LSM_PRIVATE_FILE_LOCK_OK = 0,
    LSM_PRIVATE_FILE_LOCK_BUSY = 1,
    LSM_PRIVATE_FILE_LOCK_UNSAFE = 2,
    LSM_PRIVATE_FILE_LOCK_IO = 3,
} lsm_private_file_lock_status_t;

typedef enum {
    LSM_PRIVATE_FILE_LOCK_SH = 1,
    LSM_PRIVATE_FILE_LOCK_EX = 2,
} lsm_private_file_lock_mode_t;

typedef struct lsm_private_lock_directory lsm_private_lock_directory_t;
typedef struct lsm_private_file_lock lsm_private_file_lock_t;

/* Basenames are fixed internal names, never paths. Acquisition is
 * nonblocking; BUSY is the only contention result. Stable lock files are never
 * unlinked by this API. Any non-NULL *lock_out on any status owns native
 * cleanup state and must be passed to lsm_private_file_lock_release(). */
lsm_private_file_lock_status_t lsm_private_file_lock_try_acquire(
    lsm_private_lock_directory_t *directory, const char *base_name,
    lsm_private_file_lock_mode_t mode, lsm_private_file_lock_t **lock_out);

/* OK terminally closes the native handle and clears *lock_io. IO retains a
 * non-NULL object only while native ownership is safely retryable. POSIX
 * close(2) consumes descriptor ownership once invoked even if it reports an
 * error, so that terminal IO case clears *lock_io to prevent fd-reuse races. */
lsm_private_file_lock_status_t lsm_private_file_lock_release(lsm_private_file_lock_t **lock_io);

void lsm_private_lock_directory_close(lsm_private_lock_directory_t *directory);
const char *lsm_private_lock_directory_path(const lsm_private_lock_directory_t *directory);

#endif /* LSM_PRIVATE_FILE_LOCK_H */
