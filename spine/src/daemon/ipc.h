/*
 * ipc.h — Authenticated local transport for the LSM daemon.
 *
 * Unix builds use owner-only Unix-domain sockets and advisory file locks.
 * Windows builds use generation-random current-user named pipes plus
 * handle-anchored private lock files in an owner-only runtime directory. All
 * implementation details stay opaque so callers cannot accidentally bypass
 * the transport checks.
 */
#ifndef LSM_DAEMON_IPC_H
#define LSM_DAEMON_IPC_H

#include "daemon/daemon.h"
#include "foundation/private_file_lock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct lsm_daemon_ipc_endpoint lsm_daemon_ipc_endpoint_t;
typedef struct lsm_daemon_ipc_listener lsm_daemon_ipc_listener_t;
typedef struct lsm_daemon_ipc_connection lsm_daemon_ipc_connection_t;
typedef struct lsm_daemon_ipc_startup_lock lsm_daemon_ipc_startup_lock_t;
typedef struct lsm_daemon_ipc_local_transition lsm_daemon_ipc_local_transition_t;
typedef struct lsm_daemon_ipc_participant_guard lsm_daemon_ipc_participant_guard_t;
typedef struct lsm_daemon_ipc_lifetime_reservation lsm_daemon_ipc_lifetime_reservation_t;

/* A receive wait with no wall-clock expiry. The wait remains interruptible by
 * peer EOF and lsm_daemon_ipc_connection_interrupt(). This is intentionally a
 * named sentinel rather than a very large finite timeout: authenticated MCP
 * sessions routinely remain idle for days, and a long-running application
 * request must not lose its control connection merely because no new frame is
 * arriving. */
#define LSM_DAEMON_IPC_WAIT_FOREVER UINT32_MAX

/* Create one endpoint namespace for an exact 16-lowercase-hex instance key.
 * POSIX addresses are deterministic; Windows resolves the current daemon
 * generation through an owner-only nonce record. runtime_parent may be NULL
 * to use the platform's secure local default. The returned runtime directory
 * is an owner-only child of that parent. */
lsm_daemon_ipc_endpoint_t *lsm_daemon_ipc_endpoint_new(const char *instance_key,
                                                       const char *runtime_parent);
void lsm_daemon_ipc_endpoint_free(lsm_daemon_ipc_endpoint_t *endpoint);
const char *lsm_daemon_ipc_endpoint_address(const lsm_daemon_ipc_endpoint_t *endpoint);
const char *lsm_daemon_ipc_endpoint_runtime_dir(const lsm_daemon_ipc_endpoint_t *endpoint);

/* Duplicate the endpoint's already-validated owner-only runtime-directory
 * handle for native lock files. Callers never reopen the path themselves. */
lsm_private_file_lock_status_t lsm_daemon_ipc_private_lock_directory_new(
    const lsm_daemon_ipc_endpoint_t *endpoint, lsm_private_lock_directory_t **directory_out);

/* Listener and connection handles are non-inheritable. */
lsm_daemon_ipc_listener_t *lsm_daemon_ipc_listen(const lsm_daemon_ipc_endpoint_t *endpoint);

/* A daemon host may reserve endpoint ownership before constructing any
 * stateful subsystem, then transfer that exact reservation into its listener.
 * POSIX listener publication binds a private generation anchor, durably
 * records the pending anchor identity through a recoverable deterministic
 * temp link, hard-links the anchor into the stable socket name without
 * overwrite, commits an anchor-bearing identity marker through the same
 * protocol, and removes pending last. The anchor remains linked for the
 * listener lifetime.
 * listen_reserved consumes *reservation_io only on success and sets it to
 * NULL; on failure the caller still owns it. A reservation is bound to the
 * endpoint that created it and cannot be adopted by another endpoint. The
 * daemon host must retain a participant guard before taking the reservation;
 * the convenience listen() wrapper owns that guard automatically. */
int lsm_daemon_ipc_lifetime_reservation_try_acquire(
    const lsm_daemon_ipc_endpoint_t *endpoint,
    lsm_daemon_ipc_lifetime_reservation_t **reservation_out);
void lsm_daemon_ipc_lifetime_reservation_release(
    lsm_daemon_ipc_lifetime_reservation_t *reservation);
lsm_daemon_ipc_listener_t *lsm_daemon_ipc_listen_reserved(
    const lsm_daemon_ipc_endpoint_t *endpoint,
    lsm_daemon_ipc_lifetime_reservation_t **reservation_io);
void lsm_daemon_ipc_listener_close(lsm_daemon_ipc_listener_t *listener);

/* Create or validate one private current-user directory at an already
 * canonical local path. Ancestors are handle-validated without mutation:
 * POSIX permits only root/current-user owners and no group/other write (apart
 * from a root-owned sticky parent selecting a trusted-owned child); Windows
 * permits current user/SYSTEM/Administrators/TrustedInstaller and rejects
 * untrusted mutation ACEs. The final directory is hardened to owner-only
 * access. Symlink, reparse-point, foreign-owner, and unsafe filesystem states
 * fail closed. This is the common trust boundary for the canonical account
 * cache root as well as daemon-private artifacts. */
bool lsm_daemon_ipc_private_directory_secure(const char *directory_path);

/* Human-readable reason for this process's most recent private-namespace
 * validation refusal (empty string when none). Diagnostic only — callers
 * append it to their error messages; policy decisions never read it. */
const char *lsm_daemon_ipc_validation_detail(void);
#ifdef LSM_ENABLE_TEST_SEAMS
/* #1537: seed the detail so a test can prove the CLI refusal surfaces it. */
void lsm_daemon_ipc_set_validation_detail_for_testing(const char *detail);
#endif

/* Create/validate an owner-only directory and securely open one regular
 * owner-only append log within it. User-controlled path components may not be
 * symlinks, junctions, or other reparse points; trusted root-owned macOS /tmp
 * and /var aliases are normalized before validation. A file larger than
 * rotate_cap_bytes is moved to <base_name>.1 before a new stream is opened.
 * The caller owns the returned FILE and must fclose it. */
FILE *lsm_daemon_ipc_private_log_open(const char *directory_path, const char *base_name,
                                      size_t rotate_cap_bytes);

/* Tri-state operations return 1 on success, 0 on timeout, and -1 on error.
 * receive_frame also accepts LSM_DAEMON_IPC_WAIT_FOREVER. */
int lsm_daemon_ipc_accept(lsm_daemon_ipc_listener_t *listener, uint32_t timeout_ms,
                          lsm_daemon_ipc_connection_t **connection_out);

/* No-spawn endpoint liveness probe for binary activation safety. Returns
 * 1 when a secure daemon endpoint is connected, pending, or busy; 0 only when
 * it is absent; -1 when endpoint ownership or safety cannot be proven. A
 * secure but refused Unix socket also fails closed as active because BSD may
 * use ECONNREFUSED for a full listen queue.
 * It never sends a protocol frame, so exact and conflicting builds are both
 * reported as active. */
int lsm_daemon_ipc_endpoint_probe(const lsm_daemon_ipc_endpoint_t *endpoint, uint32_t timeout_ms);

/* Observe the daemon-generation lifetime reservation without spawning a
 * daemon or retaining the reservation. Returns 1 while a host is constructing,
 * listening, or closing the stable endpoint, 0 when the reservation is free,
 * and -1 when ownership/safety cannot be proven. Stale cleanup holds both the
 * startup lock and a temporary lifetime reservation; listener construction
 * then transfers a lifetime reservation through listener teardown. */
int lsm_daemon_ipc_lifetime_reservation_probe(const lsm_daemon_ipc_endpoint_t *endpoint);

#ifndef _WIN32
/* Remove only a provably current-generation stale Unix socket identity. The
 * caller must first observe an absent lifetime reservation and retain the
 * matching startup lock for the complete call; the implementation rechecks
 * both conditions. Stable deletion requires a committed marker whose named
 * anchor and stable path are the same secure socket inode; pending alone may
 * only complete that commit when both paths independently corroborate it.
 * A differing stable replacement is preserved while owned anchor/records are
 * collected. Returns 1 when the stable endpoint is absent and all owned
 * artifacts are absent or were removed, 0 when cleanup is refused for a live,
 * legacy, unknown, or mismatched identity, and -1 for an invalid lock or
 * unsafe/I/O state. This operation never connects to the endpoint. */
int lsm_daemon_ipc_stale_generation_cleanup(const lsm_daemon_ipc_endpoint_t *endpoint,
                                            const lsm_daemon_ipc_startup_lock_t *startup_lock);
#endif

/* Migration-only observation of transport/startup primitives used by builds
 * that predate version-cohort coordination. This never connects to the old
 * endpoint and never claims a legacy primitive as ownership authority.
 * Returns 1 when a legacy generation is present or starting, 0 when absent,
 * and -1 when ownership/safety cannot be proven. */
int lsm_daemon_ipc_legacy_generation_probe(const lsm_daemon_ipc_endpoint_t *endpoint);

lsm_daemon_ipc_connection_t *lsm_daemon_ipc_connect(const lsm_daemon_ipc_endpoint_t *endpoint,
                                                    uint32_t timeout_ms);
void lsm_daemon_ipc_connection_close(lsm_daemon_ipc_connection_t *connection);

/* Bounded wait for the peer to consume everything already sent, by reading
 * (and discarding) until peer EOF or the timeout. On POSIX this is a no-op:
 * closing a stream socket leaves buffered data deliverable, so the final
 * response survives an immediate close. On Windows named pipes, closing the
 * server handle discards data the client has not yet read; a server that
 * sends a final response and closes immediately races the client's decode.
 * Peer EOF is proof of consumption because clients close only after reading
 * their pending response. Callers close the connection afterwards. */
void lsm_daemon_ipc_connection_drain(lsm_daemon_ipc_connection_t *connection, uint32_t timeout_ms);

/* Interrupt pending I/O without freeing the connection object. This is
 * idempotent and may be called from a supervisor thread; the connection's
 * owning worker must still call connection_close after its I/O unwinds. */
void lsm_daemon_ipc_connection_interrupt(lsm_daemon_ipc_connection_t *connection);

/* Kernel-reported peer process ID for this established socket/pipe. Returns
 * zero when the platform cannot provide an authenticated peer PID. The daemon
 * uses this as the anchor for executable identity verification; protocol
 * payloads are never trusted to declare their own process identity. */
uint64_t lsm_daemon_ipc_connection_peer_pid(const lsm_daemon_ipc_connection_t *connection);

/* send_frame uses a bounded internal write timeout.  receive_frame returns a
 * malloc-owned payload (NULL for an empty frame) through payload_out. */
bool lsm_daemon_ipc_send_frame(lsm_daemon_ipc_connection_t *connection,
                               lsm_daemon_frame_type_t type, uint16_t flags, const void *payload,
                               uint32_t length);
int lsm_daemon_ipc_receive_frame(lsm_daemon_ipc_connection_t *connection, uint32_t timeout_ms,
                                 lsm_daemon_frame_t *frame_out, uint8_t **payload_out);

/* Nonblocking single-winner current-generation startup serialization. POSIX
 * retains startup-v2 EX plus a SH claim on the frozen legacy startup file.
 * Windows retains startup-v2 EX plus the validated frozen legacy mutex, but
 * does not publish a generation or sentinel until prepare_handoff. This makes
 * activation/install probes non-mutating. Returns 1 when acquired, 0 on a
 * competing transition/legacy owner, and -1 on an unsafe or I/O state. */
int lsm_daemon_ipc_startup_lock_try_acquire(const lsm_daemon_ipc_endpoint_t *endpoint,
                                            lsm_daemon_ipc_startup_lock_t **lock_out);
/* Activation-only no-spawn generation probe under the exact matching,
 * retained, unprepared startup lock. It ignores the caller's own startup-v2
 * and frozen-legacy startup claims, and reports only a live daemon lifetime,
 * stable current transport, or deterministic legacy/current sentinel.
 * Returns 1 when active, 0 when authoritatively absent, and -1 when the lock,
 * endpoint, transport, or ownership cannot be validated. */
int lsm_daemon_ipc_generation_probe_under_startup_lock(
    const lsm_daemon_ipc_endpoint_t *endpoint, const lsm_daemon_ipc_startup_lock_t *startup_lock);
/* Prepare a serialized daemon handoff. POSIX validates/removes only a
 * provably current-generation stale socket identity while retaining both
 * startup-v2 EX and frozen-legacy SH. Windows creates/joins the authenticated
 * current-participant group, publishes the generation, then releases only the
 * legacy mutex. Both platforms retain startup-v2 and a temporary participant
 * claim through child readiness. */
bool lsm_daemon_ipc_startup_lock_prepare_handoff(lsm_daemon_ipc_startup_lock_t *lock);
/* Retry-safe teardown. Success consumes and clears *lock_io. A native cleanup
 * failure returns false and retains every unreleased claim in *lock_io; the
 * caller must retry within a finite deadline or fail-stop its process. */
bool lsm_daemon_ipc_startup_lock_release(lsm_daemon_ipc_startup_lock_t **lock_io);

/* Join the current participant group while a daemon bootstrap parent retains
 * its prepared startup lock. The child takes no startup-v2 claim of its own;
 * it joins the parent's authenticated legacy-compatible group and retains the
 * returned guard for its complete daemon lifetime. The endpoint must outlive
 * the guard. Returns 1 when joined, 0 when no prepared group exists or a
 * legacy transition won, and -1 on an unsafe/I/O state. Release is retry-safe
 * where the platform exposes retryable teardown and consumes *guard_io only
 * after the participant claim is completely gone. */
int lsm_daemon_ipc_participant_guard_try_join(const lsm_daemon_ipc_endpoint_t *endpoint,
                                              lsm_daemon_ipc_participant_guard_t **guard_out);
bool lsm_daemon_ipc_participant_guard_release(lsm_daemon_ipc_participant_guard_t **guard_io);

/* Standalone CLI work joins the legacy-compatible current participant group
 * without becoming a daemon client. Acquisition retains startup-v2 only for
 * the seal/presence decision. Sealing also retains a participant claim:
 * frozen-legacy SH on POSIX, or group-v1 SH plus one non-serving deterministic
 * legacy-pipe sentinel on Windows. After the transition-aware presence check,
 * begin_work commits the decision by releasing startup-v2 while keeping the
 * participant claim through local work. Multiple local participants and a
 * same-build modern daemon may then overlap, while a pre-cohort startup stays
 * excluded. POSIX sealing also removes only a provably current-generation
 * crash-left socket identity. Tri-state operations return 1 on success, 0
 * when another transition or legacy/unknown generation won, and -1 on an
 * unsafe/I/O state. The endpoint must outlive the transition. */
int lsm_daemon_ipc_local_transition_try_acquire(const lsm_daemon_ipc_endpoint_t *endpoint,
                                                lsm_daemon_ipc_local_transition_t **transition_out);
int lsm_daemon_ipc_local_transition_seal_legacy(lsm_daemon_ipc_local_transition_t *transition);
/* Observe daemon lifetime only through a successfully sealed, matching,
 * retained transition. Returns 1 while a daemon lifetime is active, 0 when
 * its absence is authoritative under the transition, and -1 when the guard,
 * endpoint, or underlying primitive cannot be verified. */
int lsm_daemon_ipc_local_transition_lifetime_probe(
    const lsm_daemon_ipc_endpoint_t *endpoint, const lsm_daemon_ipc_local_transition_t *transition);
/* Commit the classified transition and begin standalone work. This releases
 * startup-v2 but retains the participant claim. It is valid exactly once after
 * sealing/presence classification. */
bool lsm_daemon_ipc_local_transition_begin_work(lsm_daemon_ipc_local_transition_t *transition);
/* Release ends protection after the caller has stopped all local work. On
 * Windows teardown reacquires startup-v2 and the legacy mutex, releases group
 * SH, closes this participant's sentinel last, then releases mutex and v2.
 * Success consumes and clears *transition_io; a retryable native cleanup
 * failure returns false and retains it. */
bool lsm_daemon_ipc_local_transition_release(lsm_daemon_ipc_local_transition_t **transition_io);

#endif /* LSM_DAEMON_IPC_H */
