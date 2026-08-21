/*
 * http_server.h — Embedded HTTP server for the graph visualization UI.
 *
 * Binds to 127.0.0.1:<port> only (localhost).
 * Serves embedded frontend assets and proxies /rpc to a dedicated
 * read-only lsm_mcp_server_t instance.
 *
 * Runs in a background pthread, same pattern as the watcher thread.
 */
#ifndef LSM_UI_HTTP_SERVER_H
#define LSM_UI_HTTP_SERVER_H

#include "ui/httpd.h"
#include "foundation/sha256.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct lsm_http_server lsm_http_server_t;
struct lsm_watcher;

/* Daemon-owned index boundary. The callback blocks until the coordinated
 * operation finishes; the HTTP server runs it on a tracked, joinable thread. */
typedef int (*lsm_http_index_executor_fn)(void *context, const char *root_path,
                                          const char *project_name);
typedef bool (*lsm_http_project_mutation_begin_fn)(void *context, const char *project);
typedef void (*lsm_http_project_mutation_end_fn)(void *context, const char *project);

/* Create an HTTP server on the given port.
 * Creates its own lsm_mcp_server_t with a separate read-only SQLite connection.
 * Returns NULL on failure (e.g. port in use). */
lsm_http_server_t *lsm_http_server_new(int port);

/* Free a quiescent HTTP server. Returns false without freeing if the run loop
 * or an index callback can still access it; callers must fail-stop rather than
 * continue cleanup in that state. */
bool lsm_http_server_free(lsm_http_server_t *srv);

/* Signal the HTTP server to stop (safe to call from any thread). */
void lsm_http_server_stop(lsm_http_server_t *srv);

/* Claim a future run before handing the server pointer to a new thread. This
 * closes the create-to-child-start lifetime gap. If thread creation fails,
 * cancel the still-scheduled run before freeing. */
bool lsm_http_server_schedule_run(lsm_http_server_t *srv);
bool lsm_http_server_cancel_scheduled_run(lsm_http_server_t *srv);

/* Run the scheduled HTTP server event loop from the background thread.
 * Blocks until lsm_http_server_stop() is called. */
void lsm_http_server_run(lsm_http_server_t *srv);

/* Observation-only phase seam for deterministic concurrency tests. */
lsm_httpd_activity_t lsm_http_server_activity_for_test(lsm_http_server_t *srv);

/* Check if the server started successfully (listener bound). */
bool lsm_http_server_is_running(const lsm_http_server_t *srv);

/* The actually-bound port (useful when constructed with port 0 in tests). */
int lsm_http_server_port(const lsm_http_server_t *srv);

/* Override the per-connection receive deadline (tests use short values). */
void lsm_http_server_set_recv_deadline_ms(lsm_http_server_t *srv, int ms);

/* Set external watcher reference for UI project lifecycle actions. Not owned. */
void lsm_http_server_set_watcher(lsm_http_server_t *srv, struct lsm_watcher *watcher);

/* Route UI indexing through the daemon's shared operation registry. */
void lsm_http_server_set_index_executor(lsm_http_server_t *srv, lsm_http_index_executor_fn executor,
                                        void *context);

/* Route direct UI mutations and /rpc mutation tools through the daemon's
 * per-project coordination gate. Direct UI calls are non-blocking: begin=false
 * is returned to the browser as a retryable locked response. */
void lsm_http_server_set_project_mutation_guard(lsm_http_server_t *srv,
                                                lsm_http_project_mutation_begin_fn begin,
                                                lsm_http_project_mutation_end_fn end,
                                                void *context);

/* Copy the daemon generation's private readiness secret into the server
 * before its run loop starts. GET /__cbm/ui-readiness returns only a
 * challenge-bound HMAC proof; the secret itself is never served. */
void lsm_http_server_set_readiness_secret(lsm_http_server_t *srv,
                                          const uint8_t secret[LSM_SHA256_DIGEST_LEN]);

/* Initialize the log ring buffer mutex. Must be called once before any threads. */
void lsm_ui_log_init(void);

/* Append a log line to the UI ring buffer (called from log hook). */
void lsm_ui_log_append(const char *line);

/* Set the binary path for subprocess spawning (call from main). */
void lsm_http_server_set_binary_path(const char *path);

/* Resolve argv[0] into an executable path suitable for subprocess spawning. */
bool lsm_http_server_resolve_binary_path(const char *argv0, char *out, size_t outsz);

/* Pure git-remote URL helpers used by GET /api/repo-info. Exposed for tests. */

/* Normalize a git remote (scp-style / ssh:// / https://) to a canonical
 * "https://host/org/repo" web base, with trailing ".git" and any embedded
 * credentials removed. malloc'd or NULL. Caller frees. */
char *lsm_ui_git_web_base(const char *url);

/* Copy `url` with any "user[:password]@" userinfo stripped from a
 * scheme://authority URL (scp-style is left unchanged). malloc'd, or NULL when
 * `url` is NULL. Caller frees. */
char *lsm_ui_git_strip_credentials(const char *url);

#endif /* LSM_UI_HTTP_SERVER_H */
