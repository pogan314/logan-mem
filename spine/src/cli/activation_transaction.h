/*
 * activation_transaction.h -- Transactional binary activation primitives.
 *
 * This is an internal CLI module.  It deliberately knows nothing about daemon
 * coordination or editor configuration: callers must acquire the maintenance
 * barrier before commit and retain it until finalize/rollback completes.
 */
#ifndef LSM_ACTIVATION_TRANSACTION_H
#define LSM_ACTIVATION_TRANSACTION_H

#include <stdbool.h>
#include <stddef.h>

typedef struct lsm_activation_transaction lsm_activation_transaction_t;

typedef enum {
    LSM_ACTIVATION_TRANSACTION_OK = 0,
    /* Windows could not unlink an inactive backup (normally because the old
     * executable image is still mapped) but safely registered it for deletion
     * at reboot.  The committed activation remains valid. */
    LSM_ACTIVATION_TRANSACTION_DEFERRED = 1,
    LSM_ACTIVATION_TRANSACTION_INVALID_ARGUMENT = -1,
    LSM_ACTIVATION_TRANSACTION_NO_MEMORY = -2,
    LSM_ACTIVATION_TRANSACTION_IO = -3,
    LSM_ACTIVATION_TRANSACTION_INVALID_STATE = -4,
    /* The post-commit validator rejected the candidate and rollback succeeded. */
    LSM_ACTIVATION_TRANSACTION_VALIDATION_FAILED = -5,
    /* The target changed, but restoring the retained backup also failed. */
    LSM_ACTIVATION_TRANSACTION_ROLLBACK_FAILED = -6,
} lsm_activation_transaction_status_t;

typedef bool (*lsm_activation_transaction_validator_fn)(const char *target_path, void *context);

/* Test-only seam: invoked after an absent target has been revalidated and
 * immediately before its staged candidate is published.  Production callers
 * leave this unset. */
typedef void (*lsm_activation_transaction_before_absent_publish_for_test_fn)(
    const char *target_path, void *context);
void lsm_activation_transaction_set_before_absent_publish_for_test(
    lsm_activation_transaction_before_absent_publish_for_test_fn hook, void *context);

/* Test-only seam: make the next `count` Windows rename attempts fail as though
 * another handle held the file, so the transient-lock retry can be proven
 * without racing a real scanner. Inert on POSIX and when count is 0. */
void lsm_activation_transaction_rename_failures_set_for_test(unsigned int count);

/* Stage a candidate beside target_path (therefore on the same filesystem).
 * The staged file is private to the current account and executable. */
lsm_activation_transaction_status_t lsm_activation_transaction_stage_bytes(
    const char *target_path, const void *candidate, size_t candidate_size,
    lsm_activation_transaction_t **transaction_out);

/* Copy candidate_path into a private executable stage beside target_path. */
lsm_activation_transaction_status_t lsm_activation_transaction_stage_file(
    const char *target_path, const char *candidate_path,
    lsm_activation_transaction_t **transaction_out);

/* Prepare an atomic removal.  A missing target is a valid no-op transaction. */
lsm_activation_transaction_status_t lsm_activation_transaction_stage_removal(
    const char *target_path, lsm_activation_transaction_t **transaction_out);

/* Atomically publish the candidate (or remove the target), retaining any old
 * target at backup_path.  If validator rejects the post-commit state, this
 * function rolls back before returning VALIDATION_FAILED. */
lsm_activation_transaction_status_t lsm_activation_transaction_commit(
    lsm_activation_transaction_t *transaction, lsm_activation_transaction_validator_fn validator,
    void *validator_context);

/* Restore the retained target after a successful commit. */
lsm_activation_transaction_status_t lsm_activation_transaction_rollback(
    lsm_activation_transaction_t *transaction);

/* Accept the committed state and delete the retained backup.  On Windows,
 * DEFERRED means deletion was safely registered for reboot; deferred_path
 * remains available for logging until close(). */
lsm_activation_transaction_status_t lsm_activation_transaction_finalize(
    lsm_activation_transaction_t *transaction);

/* Close an object.  An uncommitted object is cleanly aborted; a committed but
 * unfinalized object is rolled back.  On cleanup failure, ownership stays with
 * the caller so paths and rollback can be retried. */
lsm_activation_transaction_status_t lsm_activation_transaction_close(
    lsm_activation_transaction_t **transaction_io);

const char *lsm_activation_transaction_target_path(const lsm_activation_transaction_t *transaction);
const char *lsm_activation_transaction_staged_path(const lsm_activation_transaction_t *transaction);
const char *lsm_activation_transaction_backup_path(const lsm_activation_transaction_t *transaction);
const char *lsm_activation_transaction_deferred_path(
    const lsm_activation_transaction_t *transaction);

const char *lsm_activation_transaction_status_message(lsm_activation_transaction_status_t status);

/* Which security predicate refused the most recent transaction, as
 * "predicate (os N)", or "" when nothing refused since the last prepare.
 * The predicates refuse without a usable OS last-error, so this is the only
 * way a caller can say WHY staging failed. Reset by every prepare/stage
 * entry; single-threaded like the rest of the transaction API. */
const char *lsm_activation_transaction_refusal_note(void);

#ifdef LSM_ENABLE_TEST_SEAMS
void lsm_activation_transaction_note_refusal_for_testing(const char *predicate,
                                                         unsigned long os_error);
#endif

#endif /* LSM_ACTIVATION_TRANSACTION_H */
