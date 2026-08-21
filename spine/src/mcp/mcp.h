/*
 * mcp.h — MCP (Model Context Protocol) server for logan-spine-mcp.
 *
 * Implements JSON-RPC 2.0 over stdio with the MCP tool calling protocol.
 * Provides 14 graph analysis tools (search, trace, query, index, etc.)
 */
#ifndef LSM_MCP_H
#define LSM_MCP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "mcp/index_supervisor.h"

/* ── Forward declarations ─────────────────────────────────────── */

typedef struct lsm_store lsm_store_t; /* from store/store.h */
struct lsm_watcher;                   /* from watcher/watcher.h */
struct lsm_config;                    /* from cli/cli.h */

/* ── JSON-RPC types ───────────────────────────────────────────── */

typedef struct {
    const char *jsonrpc;    /* "2.0" */
    const char *method;     /* e.g. "initialize", "tools/call" */
    int64_t id;             /* request ID (numeric form; -1 if notification) */
    const char *id_str;     /* non-NULL when id is a JSON string (issue #253) */
    bool has_id;            /* false for notifications */
    const char *params_raw; /* raw JSON string of params */
} lsm_jsonrpc_request_t;

typedef struct {
    int64_t id;
    const char *id_str;      /* non-NULL to echo a string id verbatim (issue #253) */
    const char *result_json; /* JSON string for result (success) */
    const char *error_json;  /* JSON string for error (failure), NULL on success */
    int error_code;          /* JSON-RPC error code */
} lsm_jsonrpc_response_t;

/* ── JSON-RPC parsing / formatting ────────────────────────────── */

/* Parse a JSON-RPC request line. Returns 0 on success, -1 on error.
 * Caller must call lsm_jsonrpc_request_free(). */
int lsm_jsonrpc_parse(const char *line, lsm_jsonrpc_request_t *out);
void lsm_jsonrpc_request_free(lsm_jsonrpc_request_t *r);

/* Format a JSON-RPC response. Returns heap-allocated JSON string. */
char *lsm_jsonrpc_format_response(const lsm_jsonrpc_response_t *resp);

/* Format a JSON-RPC error response. Returns heap-allocated JSON string. */
char *lsm_jsonrpc_format_error(int64_t id, int code, const char *message);

/* ── MCP protocol helpers ─────────────────────────────────────── */

/* Format an MCP tool result with text content. Returns heap-allocated JSON. */
char *lsm_mcp_text_result(const char *text, bool is_error);

/* Return true when notifications/cancelled params target the active request. */
bool lsm_mcp_cancel_request_matches(const char *params_json, int64_t active_id,
                                    const char *active_id_str);

/* Format the tools/list response. Returns heap-allocated JSON. */
char *lsm_mcp_tools_list(void);

/* Return a tool's JSON input_schema string by name (static; do not free), or
 * NULL if the tool is unknown. Backs the CLI flag parser + per-tool --help. */
const char *lsm_mcp_tool_input_schema(const char *tool_name);

/* Registry accessors: the number of tools tools/list advertises, and the name
 * of tool `index` (static, do not free; NULL when out of range). */
int lsm_mcp_tool_count(void);
const char *lsm_mcp_tool_name(int index);

/* Render the top-level --help "Tools:" block from the registry so the help
 * text cannot drift from tools/list (#1361). Heap-allocated; caller frees. */
char *lsm_mcp_tools_help_list(void);

/* Format the initialize response. params_json is the raw initialize params
 * (used for protocol version negotiation). Returns heap-allocated JSON. */
char *lsm_mcp_initialize_response(const char *params_json);

/* ── Tool argument helpers ────────────────────────────────────── */

/* Extract a string argument from the tools/call params JSON.
 * Returns heap-allocated copy, or NULL if not found. */
char *lsm_mcp_get_string_arg(const char *args_json, const char *key);

/* Extract an int argument. Returns default_val if not found. */
int lsm_mcp_get_int_arg(const char *args_json, const char *key, int default_val);

/* Extract a bool argument. Returns false if not found. */
bool lsm_mcp_get_bool_arg(const char *args_json, const char *key);

/* Extract the tool name from a tools/call params JSON. Heap-allocated. */
char *lsm_mcp_get_tool_name(const char *params_json);

/* Extract the arguments sub-object from tools/call params. Heap-allocated JSON string. */
char *lsm_mcp_get_arguments(const char *params_json);

/* ── MCP Server ───────────────────────────────────────────────── */

typedef struct lsm_mcp_server lsm_mcp_server_t;

typedef enum {
    LSM_MCP_TOOL_PROFILE_ALL = 0,
    /* Agent-facing restricted surfaces: only explicitly allowlisted inspection
     * tools are advertised or callable. Internal cache maintenance may still
     * write, so these are intentionally not named strictly read-only modes. */
    LSM_MCP_TOOL_PROFILE_ANALYSIS = 1,
    LSM_MCP_TOOL_PROFILE_SCOUT = 2,
} lsm_mcp_tool_profile_t;

/* Parse the process-level tool-profile flag. Explicit malformed or unknown
 * values fail closed with -1; absence selects the full default surface. */
int lsm_mcp_parse_tool_profile_args(int argc, const char *const argv[const],
                                    lsm_mcp_tool_profile_t *profile_out);

/* Restricted servers must not start a second unrestricted HTTP/RPC surface. */
bool lsm_mcp_tool_profile_allows_http(lsm_mcp_tool_profile_t profile);

/* Optional daemon-owned physical index executor. repo_path is canonical and
 * already authorized against this session; args_json contains that canonical
 * path plus all caller options. The callback returns a complete malloc-owned
 * MCP tool result. A NULL result is reported as an error and never degrades to
 * an uncoordinated in-process index. */
typedef char *(*lsm_mcp_index_executor_fn)(void *context, const char *repo_path,
                                           const char *args_json);

/* Daemon-owned exclusive lease for operations that mutate a published project
 * database. begin may wait, but must remain cancellable by its session owner;
 * every successful begin is paired with exactly one end. Standalone/worker
 * servers leave these callbacks unset. */
typedef bool (*lsm_mcp_project_mutation_begin_fn)(void *context, const char *project);
typedef void (*lsm_mcp_project_mutation_end_fn)(void *context, const char *project);

/* Nonblocking counterpart for opportunistic writes during a read request. A
 * successful call is released through the primary mutation guard's end hook. */
typedef bool (*lsm_mcp_project_mutation_try_begin_fn)(void *context, const char *project);

/* Create an MCP server. store_path is the SQLite database directory. */
lsm_mcp_server_t *lsm_mcp_server_new(const char *store_path);

/* Select the tool surface exposed by tools/list and enforced by dispatch. */
void lsm_mcp_server_set_tool_profile(lsm_mcp_server_t *srv, lsm_mcp_tool_profile_t profile);

/* Free an MCP server. */
void lsm_mcp_server_free(lsm_mcp_server_t *srv);

/* Set external watcher reference (for auto-index registration). Not owned. */
void lsm_mcp_server_set_watcher(lsm_mcp_server_t *srv, struct lsm_watcher *w);

/* Set external config store reference (for auto_index setting). Not owned. */
void lsm_mcp_server_set_config(lsm_mcp_server_t *srv, struct lsm_config *cfg);

/* Set an explicit session context for an embedded/daemon-backed server.
 * session_root is copied and its project name is derived using the same naming
 * rule as indexing. allowed_root is copied when non-NULL; an explicit NULL
 * disables the workspace boundary for this server instead of falling back to
 * LSM_ALLOWED_ROOT. Returns false when the context is invalid or cannot be
 * copied. */
bool lsm_mcp_server_set_session_context(lsm_mcp_server_t *srv, const char *session_root,
                                        const char *allowed_root);

/* Read-only session context accessors. Returned strings are owned by srv. */
const char *lsm_mcp_server_session_root(const lsm_mcp_server_t *srv);
const char *lsm_mcp_server_session_project(const lsm_mcp_server_t *srv);
const char *lsm_mcp_server_allowed_root(const lsm_mcp_server_t *srv);

/* Enable/disable per-server update checks and automatic indexing. Enabled by
 * default for standalone servers; daemon sessions disable these so the shared
 * coordinator owns background work. */
void lsm_mcp_server_set_background_tasks(lsm_mcp_server_t *srv, bool enabled);

void lsm_mcp_server_set_index_executor(lsm_mcp_server_t *srv, lsm_mcp_index_executor_fn executor,
                                       void *context);

/* Relay supervised worker logs to one local request (for CLI progress). The
 * callback/context are borrowed until the synchronous tool call returns. */
void lsm_mcp_server_set_index_log_callback(lsm_mcp_server_t *srv, lsm_proc_log_cb callback,
                                           void *context);

void lsm_mcp_server_set_project_mutation_guard(lsm_mcp_server_t *srv,
                                               lsm_mcp_project_mutation_begin_fn begin,
                                               lsm_mcp_project_mutation_end_fn end, void *context);

/* Configure an optional nonblocking acquire for the configured mutation
 * guard. If no try hook is set, a read that requires recovery fails with a
 * configuration error rather than invoking the blocking begin callback. */
void lsm_mcp_server_set_project_mutation_try_guard(lsm_mcp_server_t *srv,
                                                   lsm_mcp_project_mutation_try_begin_fn try_begin);

/* Read one complete MCP message from in. Supports newline-delimited JSON and
 * Content-Length framing, including additional headers. Returns 1 on success,
 * 0 on clean EOF, and -1 on invalid input or an I/O/allocation error. On
 * success, *message is heap-allocated and *content_length_framed identifies the
 * transport framing used by the request. */
int lsm_mcp_read_message(FILE *in, char **message, bool *content_length_framed);

/* Run the MCP server event loop on the given streams (typically stdin/stdout).
 * Blocks until EOF on input. Returns 0 on success, -1 on error. */
int lsm_mcp_server_run(lsm_mcp_server_t *srv, FILE *in, FILE *out);

/* Process a single JSON-RPC request line and return the response.
 * Returns heap-allocated JSON response string, or NULL for notifications. */
char *lsm_mcp_server_handle(lsm_mcp_server_t *srv, const char *line);

/* ── Tool handler dispatch (for testing) ──────────────────────── */

/* Handle a tools/call request. Returns MCP tool result JSON. */
char *lsm_mcp_handle_tool(lsm_mcp_server_t *srv, const char *tool_name, const char *args_json);

/* ── Supervised background index (RSS isolation, #832) ────────── */

/* One shared policy gate for supervised-index callers. Only an explicitly safe
 * terminal can yield a result or enter crash/hang recovery; cancellation,
 * failed tree containment, and unavailable worker startup are terminal. The
 * FALLBACK disposition is an internal classification used to build an explicit
 * error response, never permission for a marked host to index in-process.
 * Exposed so the security boundary is directly regression-tested. */
typedef enum {
    LSM_MCP_SUPERVISED_RESULT_FALLBACK = 0,
    LSM_MCP_SUPERVISED_RESULT_SUCCESS,
    LSM_MCP_SUPERVISED_RESULT_CONTAINED_FAILURE,
    LSM_MCP_SUPERVISED_RESULT_UNSAFE_TERMINAL,
} lsm_mcp_supervised_result_disposition_t;

lsm_mcp_supervised_result_disposition_t lsm_mcp_supervised_result_disposition(
    int spawn_result, const lsm_index_worker_result_t *worker_result);

/* Run a full index of root_path in a supervised worker SUBPROCESS (the same
 * crash/hang-isolating runner used by handle_index_repository), so the child
 * returns 100% of its RSS to the OS on exit instead of ratcheting the long-lived
 * parent. Builds {"repo_path": root_path} internally. Returns the worker's
 * response or an explicit contained-error response (caller frees). NULL is
 * reserved for invalid input/request construction failure; a marked host must
 * stop rather than use it as permission to index in-process. This is the shared
 * entry the session auto-index routes through. */
char *lsm_mcp_index_run_supervised_path(const char *root_path);

/* ── Idle store eviction ──────────────────────────────────────── */

/* Evict the cached project store if idle for more than timeout_s seconds.
 * Protects initial in-memory stores (those never accessed via a named project).
 * Called automatically by the event loop on poll() timeout. */
void lsm_mcp_server_evict_idle(lsm_mcp_server_t *srv, int timeout_s);

/* Check if the server currently has a cached store open. */
bool lsm_mcp_server_has_cached_store(lsm_mcp_server_t *srv);

/* ── Testing helpers ───────────────────────────────────────────── */

/* Get the store handle from a server (for test setup). */
lsm_store_t *lsm_mcp_server_store(lsm_mcp_server_t *srv);

/* Set the project name associated with the server's current store (for test setup).
 * This prevents resolve_store() from trying to open a .db file when tools specify a project. */
void lsm_mcp_server_set_project(lsm_mcp_server_t *srv, const char *project);

/* ── Cancellation support ─────────────────────────────────────── */

struct lsm_pipeline; /* forward decl */

/* Get the currently active pipeline (for signal handler cancellation).
 * Returns NULL if no pipeline is running. */
struct lsm_pipeline *lsm_mcp_server_active_pipeline(lsm_mcp_server_t *srv);

/* Thread-safe cancellation for daemon connection teardown. Returns false when
 * no request scope is active. Long-running request operations share the atomic
 * cancellation token and own any process-tree teardown before returning. */
bool lsm_mcp_server_cancel_active(lsm_mcp_server_t *srv);

/* Publish an outer request lifetime before crossing an asynchronous transport
 * boundary. The daemon application uses this to close the short interval
 * between accepting a request token and entering JSON-RPC/tool dispatch. Each
 * successful begin must be paired with end. */
bool lsm_mcp_server_request_scope_begin(lsm_mcp_server_t *srv);
void lsm_mcp_server_request_scope_end(lsm_mcp_server_t *srv);

/* ── URI helpers ───────────────────────────────────────────────── */

/* Parse a file:// URI and extract the filesystem path.
 * Writes to out_path (up to out_size bytes). Returns true on success.
 * On Windows, strips leading / from /C:/path. */
bool lsm_parse_file_uri(const char *uri, char *out_path, int out_size);

#endif /* LSM_MCP_H */
