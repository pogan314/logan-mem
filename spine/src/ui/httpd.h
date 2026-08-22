/*
 * httpd.h — First-party HTTP/1.1 server transport for the graph UI.
 *
 * Original implementation written for this project from RFC 9112 and the
 * needs of the graph-UI endpoints. Localhost-only by construction.
 *
 * Design constraints (deliberate — do not "improve" without reading this):
 *   - SINGLE-THREADED, sequential request handling. The routing layer
 *     (http_server.c) keeps per-request state in static buffers; a thread
 *     pool would break it. Reads and writes have hard deadlines, and stop
 *     interrupts the event-loop-owned active socket from another thread.
 *   - Binds 127.0.0.1 only (IPv4 loopback). Never any other interface.
 *   - Every response carries explicit Content-Length and "Connection: close";
 *     keep-alive is intentionally NOT implemented (smaller parsing surface;
 *     loopback reconnects are sub-millisecond). Known trade-off: on Windows,
 *     aggressive UI polling accumulates TIME_WAIT sockets against the ~16K
 *     dynamic-port ceiling — revisit only if real users report it.
 *   - Strict parsing: CRLF line endings only (bare LF rejected), request
 *     head capped at 16 KB (real requests here are < 1 KB; the cap exists
 *     to bound memory, not to accommodate growth), bodies read only via
 *     Content-Length (capped), Transfer-Encoding: chunked rejected with 411.
 *   - The request path is matched RAW — never percent-decoded before
 *     routing ("/api%2Fbrowse" must not match "/api/browse"). "%00" or a
 *     raw NUL anywhere in the request target is rejected with 400. Only
 *     query parameter VALUES are decoded (lsm_http_query_param), and
 *     decoded values containing NUL are rejected.
 */
#ifndef LSM_UI_HTTPD_H
#define LSM_UI_HTTPD_H

#include <stdbool.h>
#include <stddef.h>

/* Maximum request head (request line + headers + terminating CRLFCRLF). */
#define LSM_HTTP_MAX_HEAD (16 * 1024)
/* Maximum request body accepted via Content-Length. */
#define LSM_HTTP_MAX_BODY (1024 * 1024)
/* Default per-connection receive deadline. */
#define LSM_HTTP_RECV_DEADLINE_MS 5000
typedef struct lsm_httpd lsm_httpd_t;         /* listener */
typedef struct lsm_http_conn lsm_http_conn_t; /* accepted connection */

/* Observable transport phase used by deterministic lifecycle tests. */
typedef enum {
    LSM_HTTPD_ACTIVITY_IDLE = 0,
    LSM_HTTPD_ACTIVITY_READING_REQUEST = 1,
    LSM_HTTPD_ACTIVITY_RESPONDING = 2,
} lsm_httpd_activity_t;

/* A parsed request. `path` and `query` are raw (NOT percent-decoded).
 * Selected headers are copied for the routing/security layer ("" when
 * absent). `body` is heap-allocated and NUL-terminated. */
typedef struct {
    char method[16];
    char path[2048];
    char query[2048];
    unsigned char http_minor; /* 0 for HTTP/1.0, 1 for HTTP/1.1 */
    char origin[256];
    char host[256];
    char content_type[128];
    char accept_language[256];
    char *body;
    size_t body_len;
} lsm_http_req_t;

/* ── Listener lifecycle ───────────────────────────────────────── */

/* Listen on 127.0.0.1:<port>. port 0 binds an ephemeral port (tests).
 * Returns NULL if the port is unavailable. */
lsm_httpd_t *lsm_httpd_listen(int port);

/* The actually-bound port (differs from the requested one for port 0). */
int lsm_httpd_port(const lsm_httpd_t *d);

/* Override the per-connection receive deadline (tests use short values). */
void lsm_httpd_set_recv_deadline_ms(lsm_httpd_t *d, int ms);

/* Interrupt the current accepted connection, if any. Safe from another
 * thread; the event-loop thread retains close/free ownership. */
void lsm_httpd_interrupt(lsm_httpd_t *d);

/* Interrupt and free a quiescent listener. Returns false without freeing while
 * an accepted connection still owns the listener; its event-loop owner must
 * close that connection before the caller retries. */
bool lsm_httpd_close(lsm_httpd_t *d);

/* Snapshot the active connection phase under the listener lifecycle lock.
 * This is an observation-only seam for deterministic concurrency tests. */
lsm_httpd_activity_t lsm_httpd_activity_for_test(lsm_httpd_t *d);
/* Caps SO_SNDBUF on subsequently accepted sockets so a non-reading peer
 * produces a deterministic backpressure point on every platform. */
void lsm_httpd_set_send_buffer_for_test(lsm_httpd_t *d, int bytes);
void lsm_httpd_set_send_deadline_for_test(lsm_httpd_t *d, int ms);

/* ── Connection handling ──────────────────────────────────────── */

/* Wait up to timeout_ms for a client. NULL on timeout (caller re-checks
 * its stop flag and calls again). */
lsm_http_conn_t *lsm_httpd_accept(lsm_httpd_t *d, int timeout_ms);

/* Read and parse one request from the connection.
 * Returns 0 on success. On failure returns the HTTP status the caller
 * should send before closing (400, 408, 411, 413, 431), or -1 for a
 * connection-level error where no response is possible. */
int lsm_httpd_read_request(lsm_http_conn_t *c, lsm_http_req_t *req);

void lsm_http_req_free(lsm_http_req_t *req);

/* Send a response. extra_headers is a string of zero or more complete
 * "Name: value\r\n" lines (may be ""). Content-Length and
 * "Connection: close" are always added here — callers must not. */
void lsm_http_replyf(lsm_http_conn_t *c, int status, const char *extra_headers, const char *fmt,
                     ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

/* Binary-safe variant for embedded assets. */
void lsm_http_reply_buf(lsm_http_conn_t *c, int status, const char *extra_headers, const void *data,
                        size_t len);

int lsm_http_conn_status(const lsm_http_conn_t *c);
size_t lsm_http_conn_response_bytes(const lsm_http_conn_t *c);
void lsm_httpd_conn_close(lsm_http_conn_t *c);

/* ── Pure helpers (unit-tested without sockets) ───────────────── */

/* Parse a request head from `data` (which may also contain body bytes).
 * On success returns 0 and sets *body_offset (start of body within data)
 * and *content_length (0 when no Content-Length header is present).
 * Returns LSM_HTTP_NEED_MORE when the terminating CRLFCRLF has not
 * arrived yet, otherwise the HTTP error status to send (400/411/413/431).
 * req->body / req->body_len are NOT touched here. */
#define LSM_HTTP_NEED_MORE (-1)
int lsm_http_parse_head(const char *data, size_t len, lsm_http_req_t *req, size_t *body_offset,
                        size_t *content_length);

/* Exact match, or prefix match when `pattern` ends with '*'.
 * Used by route patterns such as "/api/layout*" and "/assets" + star. */
bool lsm_http_path_match(const char *str, const char *pattern);

/* Extract a query parameter value, percent-decoded (%XX and '+' → space).
 * Returns true only for a present, non-empty value that fits buf and
 * contains no NUL after decoding. */
bool lsm_http_query_param(const char *query, const char *name, char *buf, int bufsz);

#endif /* LSM_UI_HTTPD_H */
