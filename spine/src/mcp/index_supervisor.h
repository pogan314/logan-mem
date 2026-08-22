/*
 * index_supervisor.h — run index_repository in a supervised worker subprocess.
 *
 * A single pathological file can hard-crash (SIGSEGV / stack overflow / abort) or
 * hang the native indexer, and today that takes down the whole MCP server or CLI.
 * The supervisor runs the actual index in a CHILD process (the same binary
 * re-invoked with a build-bound `cli --index-worker …` grammar), reaps it, and
 * classifies how it ended. A crash/hang is contained to the child; the parent
 * survives and reports it instead of dying.
 *
 * This module owns only the spawn/reap MECHANISM and the worker-role state. The
 * MCP handler (mcp.c) owns the gate placement and the response building, so this
 * module has no dependency on the response format.
 *
 * fork+exec only (never fork-and-run-in-child): the server holds persistent
 * threads plus mimalloc/sqlite global state with no pthread_atfork, so a
 * fork without exec would be a latent deadlock. Recursion is prevented by an argv
 * flag (`--index-worker`), never an ambient env var.
 */
#ifndef LSM_INDEX_SUPERVISOR_H
#define LSM_INDEX_SUPERVISOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

#include "foundation/subprocess.h" /* lsm_proc_outcome_t */

/* Worker-role state, set once from the CLI arg parser (main.c) when this process
 * was spawned as a supervised worker. When active, indexing must run in-process
 * (the gate must NOT re-supervise). response_out (may be NULL) is the file the
 * worker writes its final result string to, for the parent to read back. */
void lsm_index_set_worker_role(bool is_worker, const char *response_out);

/* Hidden recovery values arrive on the worker argv, then are installed as
 * worker-local environment only after exec. The supervisor process must never
 * mutate these process-wide variables around a concurrent spawn. */
#define LSM_INDEX_WORKER_SINGLE_THREAD_ARG "--index-worker-single-thread"
#define LSM_INDEX_WORKER_MARKER_ARG "--index-worker-marker"
#define LSM_INDEX_WORKER_QUARANTINE_ARG "--index-worker-quarantine"
#define LSM_INDEX_WORKER_MEMORY_BUDGET_ARG "--index-worker-memory-budget-bytes"
#define LSM_INDEX_WORKER_BUILD_ARG "--index-worker-build"
#define LSM_INDEX_WORKER_BUILD_FINGERPRINT_LENGTH 64U
#define LSM_INDEX_WORKER_BUILD_FINGERPRINT_SIZE 65U
void lsm_index_set_worker_role_options(bool is_worker, const char *response_out, bool single_thread,
                                       const char *marker_file, const char *quarantine_file,
                                       size_t memory_budget_bytes);
bool lsm_index_worker_active(void);
const char *lsm_index_worker_response_out(void);
size_t lsm_index_worker_memory_budget_bytes(void);

/* Event name of the worker log's startup header. Shared with the tests so the
 * contract has exactly one spelling. */
#define LSM_INDEX_WORKER_LOG_START_EVENT "index.worker.start"

/* Worker-side: open the worker log for post-mortem use. Call this FIRST in a
 * process admitted by the worker argv grammar, before any indexing work.
 *
 * Six reports (#1070, #1130, #1132, #1133, #1145, #1450) describe a worker that
 * died and left a log of 0 bytes: nothing was ever flushed, so every one of them
 * is unreproducible and unattributable. This makes the log always say something:
 *   1. the stream becomes crash-durable (see lsm_log_set_crash_durable), so a
 *      crash, a SIGKILL or a hang can no longer swallow what was written;
 *   2. a startup header — version, build fingerprint, pid, repo path and the
 *      worker's own arguments — is written and flushed synchronously, so even a
 *      worker that dies before its first unit of work is identifiable.
 *
 * It fixes no crash. It converts an empty file into a report we can act on.
 * Idempotent: the worker role is installed twice (process entry, then the CLI
 * arg parser), and the header is written once. */
void lsm_index_worker_log_begin(const char *args_json, const char *repo_path);

/* Capture the exact executable-image fingerprint once, during process startup
 * before any worker can be launched. Repeated calls return the original capture
 * and never re-hash a pathname that an installer may since have replaced. */
bool lsm_index_supervisor_capture_build_fingerprint(void);
const char *lsm_index_supervisor_build_fingerprint(void);

typedef struct {
    const char *expected_build_fingerprint;
    const char *args_json;
    const char *response_out;
    bool single_thread;
    const char *marker_file;
    const char *quarantine_file;
    size_t memory_budget_bytes;
} lsm_index_worker_invocation_t;

typedef enum {
    LSM_INDEX_WORKER_ARGV_NOT_WORKER = 0,
    LSM_INDEX_WORKER_ARGV_VALID,
    LSM_INDEX_WORKER_ARGV_INVALID,
    LSM_INDEX_WORKER_ARGV_BUILD_UNAVAILABLE,
    LSM_INDEX_WORKER_ARGV_BUILD_MISMATCH,
} lsm_index_worker_argv_status_t;

/* Parse the one internal worker grammar. Any argv containing --index-worker
 * outside this exact shape is INVALID, never an ordinary CLI request. A valid
 * shape is admitted only when its expected fingerprint matches the image
 * captured by this process before stateful initialization. */
lsm_index_worker_argv_status_t lsm_index_worker_parse_process_argv(
    int argc, char *const argv[], lsm_index_worker_invocation_t *invocation_out);
const char *lsm_index_worker_argv_status_message(lsm_index_worker_argv_status_t status);

/* Host marking (#845): the supervisor gate is OPT-IN per process. Only the real
 * logan-spine-mcp binary calls this (first thing in main(), before any
 * subcommand dispatch, so MCP server + CLI + HTTP paths are all covered).
 * EMBEDDERS of lsm_mcp_handle_tool (test binaries, future library users) never
 * call it, so they index in-process by default. Without this gate the supervisor
 * resolved the CURRENT executable and re-invoked it as
 * `<self> cli --index-worker …` — a test binary ignores those args and re-runs
 * its suites instead, producing recursive spawn chains (11-min hangs; kernel
 * VM-map pressure during the 2026-07-04 host panics). */
void lsm_index_supervisor_mark_host(void);

/* True when handle_index_repository must wrap the run in a supervised child:
 * this process called lsm_index_supervisor_mark_host() (i.e. it IS the real
 * binary, not an embedder) and is not itself a worker. Supervision is mandatory
 * for a marked host; ambient configuration cannot select in-process indexing. */
bool lsm_index_supervisor_should_wrap(void);

/* TEST HOOK (#845): process-wide count of worker-start attempts, incremented on
 * entry to lsm_index_worker_start (including calls made by the synchronous
 * wrapper). Embedder tests assert the count is unchanged across an
 * index_repository call to prove indexing ran IN-PROCESS. */
int lsm_index_supervisor_spawn_count(void);

/* Test hook: single-threaded spawn count — must stay ZERO (production
 * recovery is parallel-only; no sequential runs). */
int lsm_index_supervisor_spawn_st_count(void);

typedef struct {
    lsm_proc_outcome_t outcome; /* how the worker ended */
    int exit_code;              /* worker exit code (-1 if signalled) */
    int term_signal;            /* POSIX terminating signal, else 0 */
    bool cancellation_requested;
    bool forced;
    bool tree_quiesced;
    bool supervision_failed;
    bool response_rejected; /* clean worker exceeded the bounded response protocol */
    char *response;         /* worker result only after a contained, uncancelled CLEAN exit;
                             * borrowed for async polls, caller-owned from the sync wrapper */
} lsm_index_worker_result_t;

/* Daemon-owned, nonblocking supervisor for one contained worker process tree. */
typedef struct lsm_index_worker_handle lsm_index_worker_handle_t;

typedef enum {
    LSM_INDEX_WORKER_POLL_ERROR = -1,
    LSM_INDEX_WORKER_POLL_RUNNING = 0,
    LSM_INDEX_WORKER_POLL_TERMINAL = 1,
} lsm_index_worker_poll_t;

/* Start returns after process creation. All string arguments are copied by the
 * contained subprocess layer. Recovery values are encoded only as hidden argv
 * and applied after exec by lsm_index_set_worker_role_options(). */
int lsm_index_worker_start(const char *args_json, size_t memory_budget_bytes, bool single_thread,
                           const char *marker_file, const char *quarantine_file,
                           lsm_index_worker_handle_t **handle_out);

/* Request-scoped variant used by interactive local CLI calls. The callback is
 * invoked by the owner thread while it polls the contained worker; log_context
 * remains caller-owned until terminal. No process-global sink is installed.
 * Poll stays bounded while draining log bursts and does not report terminal
 * until every completed worker log line has reached this callback. */
int lsm_index_worker_start_with_log(const char *args_json, size_t memory_budget_bytes,
                                    bool single_thread, const char *marker_file,
                                    const char *quarantine_file, lsm_proc_log_cb log_callback,
                                    void *log_context, lsm_index_worker_handle_t **handle_out);

/* Strictly nonblocking and called by one owner thread/event loop. result_out is
 * set to NULL while running and to a borrowed immutable cached result only at
 * terminal; repeated terminal polls return the same result until destroy. */
lsm_index_worker_poll_t lsm_index_worker_poll(lsm_index_worker_handle_t *handle,
                                              const lsm_index_worker_result_t **result_out);

/* Nonblocking and idempotent. Poll drives graceful-to-forced process-tree
 * termination; terminal is reported only after containment is quiescent (or a
 * bounded containment failure is explicitly surfaced in the result). The
 * owner must stop concurrent cancellation producers before destroy. */
bool lsm_index_worker_request_cancel(lsm_index_worker_handle_t *handle);

/* Borrowed diagnostic paths, stable until destroy. Every start uses securely
 * created unique files, so concurrent jobs in one daemon cannot collide. The
 * response file is always removed at terminal. A clean log is removed unless
 * profiling is active; failure/cancellation logs are retained. */
const char *lsm_index_worker_response_path(const lsm_index_worker_handle_t *handle);
const char *lsm_index_worker_log_path(const lsm_index_worker_handle_t *handle);

/* Terminal handles only; never waits or implicitly cancels. */
void lsm_index_worker_destroy(lsm_index_worker_handle_t *handle);

/* Spawn `<self> cli --index-worker --index-worker-build <sha256>
 * index_repository <args_json> --response-out <tmp>`,
 * supervise it (quiet-timeout for hangs), reap, and classify. On a clean exit,
 * result->response holds the worker's response string (read from the temp file).
 * Returns 0 if a worker reached a terminal result (including an explicitly
 * flagged bounded containment failure), or -1 only if no child was spawned.
 *
 * Probe knobs for the skip-and-continue recovery re-run (Stage 3c) are passed as
 * hidden worker argv and installed only inside the exec'd worker:
 *   - single_thread   → LSM_INDEX_SINGLE_THREAD=1
 *   - marker_file     → LSM_INDEX_MARKER_FILE
 *   - quarantine_file → LSM_INDEX_QUARANTINE_FILE
 * Any of the three may be false/NULL to leave that knob unset (the normal first
 * attempt passes single_thread=false, marker_file=NULL, quarantine_file=NULL). */
int lsm_index_spawn_worker(const char *args_json, bool single_thread, const char *marker_file,
                           const char *quarantine_file, lsm_index_worker_result_t *result);

int lsm_index_spawn_worker_with_log(const char *args_json, bool single_thread,
                                    const char *marker_file, const char *quarantine_file,
                                    lsm_proc_log_cb log_callback, void *log_context,
                                    lsm_index_worker_result_t *result);

/* Synchronous request-owned variant. A nonzero cancellation flag is forwarded
 * once to the contained worker; polling continues until the complete process
 * tree is terminal, so callers never drop supervision authority on cancel. */
int lsm_index_spawn_worker_with_log_cancel(const char *args_json, bool single_thread,
                                           const char *marker_file, const char *quarantine_file,
                                           lsm_proc_log_cb log_callback, void *log_context,
                                           const atomic_int *cancel_requested,
                                           lsm_index_worker_result_t *result);

void lsm_index_worker_result_free(lsm_index_worker_result_t *result);

#endif /* LSM_INDEX_SUPERVISOR_H */
