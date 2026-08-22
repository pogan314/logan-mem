/* RED contract for early process-role classification. */
#include "test_framework.h"

#include "daemon/bootstrap.h"
#include "daemon/ipc.h"
#include "daemon/service.h"
#include "foundation/compat.h"
#include "foundation/compat_fs.h"
#include "foundation/compat_thread.h"
#include "foundation/platform.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    BOOTSTRAP_TEST_PATH_CAP = 1024,
    BOOTSTRAP_TEST_TIMEOUT_MS = 2000,
    BOOTSTRAP_TEST_SHORT_TIMEOUT_MS = 20,
};

static const char BOOTSTRAP_BUILD_A[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char BOOTSTRAP_BUILD_B[] =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

typedef struct {
    char parent[BOOTSTRAP_TEST_PATH_CAP];
    char runtime_dir[BOOTSTRAP_TEST_PATH_CAP];
    lsm_daemon_ipc_endpoint_t *endpoint;
} bootstrap_endpoint_fixture_t;

typedef struct {
    atomic_int cohort_acquire_count;
    atomic_int cohort_release_count;
    atomic_int lock_held;
    atomic_int spawn_count;
    atomic_int probe_count;
    atomic_int lock_attempt_count;
    atomic_int handoff_count;
    atomic_int diagnostic_count;
    atomic_int reserved_probes_remaining;
    atomic_int terminal_probes_remaining;
    atomic_bool available;
    atomic_bool connect_after_reserved;
    atomic_bool connect_requires_unlocked;
    lsm_daemon_bootstrap_probe_status_t forced_probe;
    lsm_version_cohort_status_t forced_cohort;
    char diagnostic[LSM_DAEMON_CONFLICT_MESSAGE_SIZE];
} bootstrap_fake_ops_t;

typedef struct {
    const lsm_daemon_bootstrap_config_t *config;
    const lsm_daemon_bootstrap_ops_t *ops;
    atomic_int *ready;
    atomic_bool *go;
    lsm_daemon_bootstrap_result_t result;
    lsm_daemon_bootstrap_status_t status;
} bootstrap_thread_call_t;

static lsm_daemon_build_identity_t bootstrap_identity(const char *version, const char *build) {
    lsm_daemon_build_identity_t identity = {
        .semantic_version = version,
        .build_fingerprint = build,
        .cache_fingerprint = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        .protocol_abi = 3,
        .store_abi = 11,
        .feature_abi = 7,
    };
    return identity;
}

static bool bootstrap_endpoint_fixture_start(bootstrap_endpoint_fixture_t *fixture,
                                             const char *tag) {
    memset(fixture, 0, sizeof(*fixture));
    int written = snprintf(fixture->parent, sizeof(fixture->parent), "%s/lsm-bootstrap-%s-XXXXXX",
                           lsm_tmpdir(), tag);
    if (written <= 0 || written >= (int)sizeof(fixture->parent) || !lsm_mkdtemp(fixture->parent)) {
        return false;
    }
    fixture->endpoint = lsm_daemon_bootstrap_endpoint_new(fixture->parent);
    const char *runtime_dir =
        fixture->endpoint ? lsm_daemon_ipc_endpoint_runtime_dir(fixture->endpoint) : NULL;
    if (!runtime_dir) {
        return false;
    }
    written = snprintf(fixture->runtime_dir, sizeof(fixture->runtime_dir), "%s", runtime_dir);
    return written > 0 && written < (int)sizeof(fixture->runtime_dir);
}

/* Compare against canonical parents only: the endpoint canonicalizes its parent
 * before building the runtime path (/var/folders/... becomes /private/var/... on
 * macOS), so a raw prefix compare would miss a correct relocation. */
static bool bootstrap_path_has_parent(const char *path, const char *parent) {
    size_t length = parent ? strlen(parent) : 0;
    return path && length > 0 && strncmp(path, parent, length) == 0 &&
           (path[length] == '/' || path[length] == '\\');
}

static void bootstrap_endpoint_fixture_finish(bootstrap_endpoint_fixture_t *fixture) {
    lsm_daemon_ipc_endpoint_free(fixture->endpoint);
    if (fixture->runtime_dir[0] != '\0') {
        (void)lsm_rmdir(fixture->runtime_dir);
    }
    if (fixture->parent[0] != '\0') {
        (void)lsm_rmdir(fixture->parent);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static lsm_daemon_bootstrap_probe_status_t bootstrap_fake_probe(
    void *opaque, const lsm_daemon_ipc_endpoint_t *endpoint,
    const lsm_daemon_build_identity_t *identity, uint32_t timeout_ms,
    lsm_daemon_runtime_client_t **client_out, lsm_daemon_runtime_connect_result_t *result_out) {
    (void)endpoint;
    (void)identity;
    (void)timeout_ms;
    bootstrap_fake_ops_t *fake = opaque;
    atomic_fetch_add(&fake->probe_count, 1);
    memset(result_out, 0, sizeof(*result_out));
    *client_out = NULL;
    int reserved_remaining = atomic_load(&fake->reserved_probes_remaining);
    while (reserved_remaining > 0 &&
           !atomic_compare_exchange_weak(&fake->reserved_probes_remaining, &reserved_remaining,
                                         reserved_remaining - 1)) {}
    if (reserved_remaining > 0) {
        if (reserved_remaining == 1 && atomic_load(&fake->connect_after_reserved)) {
            atomic_store(&fake->available, true);
        }
        return LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED;
    }
    int terminal_remaining = atomic_load(&fake->terminal_probes_remaining);
    while (terminal_remaining > 0 &&
           !atomic_compare_exchange_weak(&fake->terminal_probes_remaining, &terminal_remaining,
                                         terminal_remaining - 1)) {}
    if (terminal_remaining > 0) {
        return LSM_DAEMON_BOOTSTRAP_PROBE_TERMINAL;
    }
    if (fake->forced_probe == LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED) {
        return fake->forced_probe;
    }
    if (fake->forced_probe == LSM_DAEMON_BOOTSTRAP_PROBE_CONFLICT) {
        result_out->status = LSM_DAEMON_RUNTIME_CONNECT_CONFLICT;
        result_out->hello_status = LSM_DAEMON_HELLO_BUILD_CONFLICT;
        snprintf(result_out->message, sizeof(result_out->message),
                 "LSM daemon could not start: conflicting versions");
        return fake->forced_probe;
    }
    if (fake->forced_probe == LSM_DAEMON_BOOTSTRAP_PROBE_TERMINAL) {
        return fake->forced_probe;
    }
    if (atomic_load(&fake->available) && atomic_load(&fake->connect_requires_unlocked) &&
        atomic_load(&fake->lock_held) != 0) {
        return LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED;
    }
    if (atomic_load(&fake->available)) {
        result_out->status = LSM_DAEMON_RUNTIME_CONNECT_ACCEPTED;
        result_out->hello_status = LSM_DAEMON_HELLO_COMPATIBLE;
        result_out->client_id = 1;
        *client_out = (lsm_daemon_runtime_client_t *)(uintptr_t)1;
        return LSM_DAEMON_BOOTSTRAP_PROBE_CONNECTED;
    }
    return LSM_DAEMON_BOOTSTRAP_PROBE_UNAVAILABLE;
}

static lsm_version_cohort_status_t bootstrap_fake_cohort_acquire(
    void *opaque, const lsm_daemon_ipc_endpoint_t *endpoint,
    const lsm_daemon_build_identity_t *identity, uint64_t deadline_ms,
    lsm_daemon_bootstrap_cohort_t *cohort_out, lsm_daemon_conflict_t *conflict_out) {
    (void)endpoint;
    (void)deadline_ms;
    bootstrap_fake_ops_t *fake = opaque;
    atomic_fetch_add(&fake->cohort_acquire_count, 1);
    *cohort_out = NULL;
    memset(conflict_out, 0, sizeof(*conflict_out));
    if (fake->forced_cohort == LSM_VERSION_COHORT_CONFLICT) {
        lsm_daemon_build_identity_t active = bootstrap_identity("2.3.0", BOOTSTRAP_BUILD_A);
        (void)lsm_daemon_hello_compare(&active, identity, conflict_out);
        return fake->forced_cohort;
    }
    if (fake->forced_cohort != LSM_VERSION_COHORT_OK) {
        return fake->forced_cohort;
    }
    *cohort_out = fake;
    return LSM_VERSION_COHORT_OK;
}

static void bootstrap_fake_cohort_release(void *opaque, lsm_daemon_bootstrap_cohort_t cohort) {
    bootstrap_fake_ops_t *fake = opaque;
    if (cohort == fake) {
        atomic_fetch_add(&fake->cohort_release_count, 1);
    }
}

static int bootstrap_fake_lock(void *opaque, const lsm_daemon_ipc_endpoint_t *endpoint,
                               lsm_daemon_bootstrap_lock_t *lock_out) {
    (void)endpoint;
    bootstrap_fake_ops_t *fake = opaque;
    atomic_fetch_add(&fake->lock_attempt_count, 1);
    int expected = 0;
    if (!atomic_compare_exchange_strong(&fake->lock_held, &expected, 1)) {
        return 0;
    }
    *lock_out = fake;
    return 1;
}

static bool bootstrap_fake_unlock(void *opaque, lsm_daemon_bootstrap_lock_t *lock_io) {
    bootstrap_fake_ops_t *fake = opaque;
    if (lock_io && *lock_io == fake) {
        atomic_store(&fake->lock_held, 0);
        *lock_io = NULL;
        return true;
    }
    return lock_io && !*lock_io;
}

static bool bootstrap_fake_handoff(void *opaque, lsm_daemon_bootstrap_lock_t lock) {
    bootstrap_fake_ops_t *fake = opaque;
    if (lock != fake || atomic_load(&fake->lock_held) != 1) {
        return false;
    }
    atomic_fetch_add(&fake->handoff_count, 1);
    return true;
}

static bool bootstrap_fake_spawn(void *opaque, const lsm_daemon_bootstrap_launch_spec_t *spec) {
    bootstrap_fake_ops_t *fake = opaque;
    /* Client bootstrap must only ever spawn the EPHEMERAL two-argument
     * shape; the permanent shape belongs exclusively to `daemon start`. */
    bool exact = spec && spec->argc == 2U && spec->argv[0] && spec->argv[1] && !spec->argv[2] &&
                 strcmp(spec->argv[1], LSM_DAEMON_INTERNAL_ARG) == 0 && spec->detached &&
                 !spec->inherit_standard_handles && !spec->use_shell &&
                 atomic_load(&fake->handoff_count) > 0 && atomic_load(&fake->lock_held) == 1;
    if (!exact) {
        return false;
    }
    atomic_fetch_add(&fake->spawn_count, 1);
    atomic_store(&fake->available, true);
    return true;
}

static void bootstrap_fake_diagnostic(void *opaque, const char *message) {
    bootstrap_fake_ops_t *fake = opaque;
    atomic_fetch_add(&fake->diagnostic_count, 1);
    snprintf(fake->diagnostic, sizeof(fake->diagnostic), "%s", message ? message : "");
}

static lsm_daemon_bootstrap_ops_t bootstrap_fake_callbacks(bootstrap_fake_ops_t *fake) {
    lsm_daemon_bootstrap_ops_t ops = {
        .context = fake,
        .cohort_acquire = bootstrap_fake_cohort_acquire,
        .cohort_release = bootstrap_fake_cohort_release,
        .probe = bootstrap_fake_probe,
        .startup_lock_try_acquire = bootstrap_fake_lock,
        .startup_lock_prepare_handoff = bootstrap_fake_handoff,
        .startup_lock_release = bootstrap_fake_unlock,
        .spawn_daemon = bootstrap_fake_spawn,
        .visible_diagnostic = bootstrap_fake_diagnostic,
    };
    return ops;
}

static void *bootstrap_thread_execute(void *opaque) {
    bootstrap_thread_call_t *call = opaque;
    atomic_fetch_add(call->ready, 1);
    while (!atomic_load(call->go)) {
        const struct timespec pause = {0, 1000000L};
        (void)lsm_nanosleep(&pause, NULL);
    }
    call->status = lsm_daemon_bootstrap_execute_with_ops(call->config, call->ops, &call->result);
    return NULL;
}

static lsm_daemon_process_role_t classify(int argc, char **argv) {
    return lsm_daemon_process_role(argc, argv);
}

TEST(daemon_bootstrap_classifies_default_and_ui_as_mcp_clients) {
    char *plain[] = {"logan-spine-mcp", NULL};
    char *ui[] = {"logan-spine-mcp", "--ui=true", "--port=9750", NULL};
    ASSERT_EQ(classify(1, plain), LSM_DAEMON_PROCESS_MCP_CLIENT);
    ASSERT_EQ(classify(3, ui), LSM_DAEMON_PROCESS_MCP_CLIENT);
    ASSERT_TRUE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_MCP_CLIENT));
    PASS();
}

TEST(daemon_bootstrap_classifies_stateless_commands_without_client) {
    char *version[] = {"logan-spine-mcp", "--version", NULL};
    char *help[] = {"logan-spine-mcp", "--profile", "--help", NULL};
    char *install[] = {"logan-spine-mcp", "install", "--dry-run", NULL};
    char *uninstall[] = {"logan-spine-mcp", "uninstall", NULL};
    char *update[] = {"logan-spine-mcp", "update", "-n", NULL};
    char *doc[] = {"logan-spine-mcp", "docstrings", "x.py", NULL};
    char *cli_doc[] = {"logan-spine-mcp", "cli", "search", "docstrings", NULL};
    ASSERT_EQ(classify(2, version), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(3, help), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(3, install), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(2, uninstall), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(3, update), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(3, doc), LSM_DAEMON_PROCESS_STATELESS);
    /* `docstrings` as opaque `cli` tool input must stay LOCAL_CLI, not become stateless. */
    ASSERT_EQ(classify(4, cli_doc), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_STATELESS));
    PASS();
}

TEST(daemon_bootstrap_classifies_config_as_coordinated_local_cli) {
    char *list[] = {"logan-spine-mcp", "config", "list", NULL};
    char *set[] = {"logan-spine-mcp", "config", "set", "auto_watch", "false", NULL};
    char *help[] = {"logan-spine-mcp", "config", "--help", NULL};
    ASSERT_EQ(classify(3, list), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_EQ(classify(5, set), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_EQ(classify(3, help), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_LOCAL_CLI));
    PASS();
}

TEST(daemon_bootstrap_cli_help_is_stateless_but_tool_calls_are_local) {
    char *tool_help[] = {"logan-spine-mcp", "cli", "search_graph", "--help", NULL};
    char *tool_call[] = {"logan-spine-mcp", "cli", "search_graph", "{}", NULL};
    ASSERT_EQ(classify(4, tool_help), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(4, tool_call), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_LOCAL_CLI));
    PASS();
}

TEST(daemon_bootstrap_cli_arguments_cannot_reclassify_the_process) {
    char *install_value[] = {
        "logan-spine-mcp", "cli", "search_code", "--query", "install", NULL};
    char *version_value[] = {"logan-spine-mcp", "cli", "search_code", "--query",
                             "--version",           NULL};
    ASSERT_EQ(classify(5, install_value), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_EQ(classify(5, version_value), LSM_DAEMON_PROCESS_LOCAL_CLI);
    PASS();
}

TEST(daemon_bootstrap_internal_roles_never_take_client_leases) {
    static char build[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char *daemon[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, NULL};
    char *worker[] = {"logan-spine-mcp", "cli", "--index-worker", "--index-worker-build", build,
                      "index_repository",    "{}",  "--response-out", "/tmp/response",        NULL};
    char *malformed_worker[] = {"logan-spine-mcp", "cli", "--index-worker",
                                "index_repository",    "{}",  NULL};
    char *reserved_user_value[] = {"logan-spine-mcp", "cli", "search_code", "--query",
                                   "--index-worker",      NULL};
    char *hook[] = {"logan-spine-mcp", "hook-augment", NULL};
    ASSERT_EQ(classify(2, daemon), LSM_DAEMON_PROCESS_DAEMON);
    ASSERT_EQ(classify(9, worker), LSM_DAEMON_PROCESS_WORKER);
    ASSERT_EQ(classify(5, malformed_worker), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(5, reserved_user_value), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(2, hook), LSM_DAEMON_PROCESS_HOOK_CLIENT);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_DAEMON));
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_WORKER));
    ASSERT_TRUE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_HOOK_CLIENT));
    PASS();
}

TEST(daemon_bootstrap_rejects_ambiguous_internal_daemon_argv) {
    char *missing[] = {NULL};
    char *mixed[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, "cli", NULL};
    ASSERT_EQ(classify(0, missing), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(3, mixed), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_INVALID));
    PASS();
}

TEST(daemon_bootstrap_uses_one_stable_per_account_endpoint) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "stable-endpoint"));
    lsm_daemon_ipc_endpoint_t *second = lsm_daemon_bootstrap_endpoint_new(fixture.parent);
    ASSERT_NOT_NULL(second);
    ASSERT_STR_EQ(lsm_daemon_ipc_endpoint_address(fixture.endpoint),
                  lsm_daemon_ipc_endpoint_address(second));
    lsm_daemon_ipc_endpoint_free(second);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

/* #1574/#1621: the shipped build must be able to relocate the rendezvous when
 * the default ancestry (%LOCALAPPDATA%, /private/tmp) cannot pass the
 * private-directory walk — otherwise every command fails, `config list`
 * included, and the operator cannot reconfigure their way out. LSM_RUNTIME_DIR
 * moves WHERE the rendezvous lives; it never relaxes HOW it is checked, so a
 * value that cannot be a private runtime parent must be refused rather than
 * silently replaced by the default. An explicit parent — the compile-time test
 * seam, the lifecycle guards' isolated namespace — keeps precedence over it. */
TEST(daemon_bootstrap_runtime_dir_env_relocates_rendezvous) {
    char override_parent[BOOTSTRAP_TEST_PATH_CAP] = {0};
    char canonical_override[BOOTSTRAP_TEST_PATH_CAP] = {0};
    char canonical_explicit[BOOTSTRAP_TEST_PATH_CAP] = {0};
    char relocated_runtime[BOOTSTRAP_TEST_PATH_CAP] = {0};
    char explicit_runtime[BOOTSTRAP_TEST_PATH_CAP] = {0};
    char unusable[BOOTSTRAP_TEST_PATH_CAP] = {0};
    int written = snprintf(override_parent, sizeof(override_parent),
                           "%s/lsm-bootstrap-runtime-env-XXXXXX", lsm_tmpdir());
    if (written <= 0 || written >= (int)sizeof(override_parent) || !lsm_mkdtemp(override_parent)) {
        FAIL("could not create the override runtime parent");
    }
    written = snprintf(unusable, sizeof(unusable), "%s/absent/nested", override_parent);
    bool prepared =
        written > 0 && written < (int)sizeof(unusable) &&
        lsm_canonical_path(override_parent, canonical_override, sizeof(canonical_override)) != 0 &&
        lsm_setenv("LSM_RUNTIME_DIR", override_parent, 1) == 0;

    /* NULL parent == every product call site: daemon, MCP client, local CLI,
     * index worker, activation. */
    lsm_daemon_ipc_endpoint_t *relocated =
        prepared ? lsm_daemon_bootstrap_endpoint_new(NULL) : NULL;
    const char *relocated_dir = relocated ? lsm_daemon_ipc_endpoint_runtime_dir(relocated) : NULL;
    if (relocated_dir) {
        (void)snprintf(relocated_runtime, sizeof(relocated_runtime), "%s", relocated_dir);
    }

    /* Same environment, explicit parent: the caller still wins. */
    bootstrap_endpoint_fixture_t fixture = {0};
    bool explicit_started = prepared && bootstrap_endpoint_fixture_start(&fixture, "runtime-env");
    bool explicit_canonical =
        explicit_started &&
        lsm_canonical_path(fixture.parent, canonical_explicit, sizeof(canonical_explicit)) != 0;
    if (explicit_started) {
        (void)snprintf(explicit_runtime, sizeof(explicit_runtime), "%s", fixture.runtime_dir);
    }

    /* A named parent that cannot pass validation is refused, never ignored. */
    bool unusable_set = prepared && lsm_setenv("LSM_RUNTIME_DIR", unusable, 1) == 0;
    lsm_daemon_ipc_endpoint_t *refused =
        unusable_set ? lsm_daemon_bootstrap_endpoint_new(NULL) : NULL;

    /* Restore before asserting: a failed assertion returns immediately, and a
     * leaked LSM_RUNTIME_DIR would follow every later suite in this process. */
    (void)lsm_unsetenv("LSM_RUNTIME_DIR");
    lsm_daemon_ipc_endpoint_free(refused);
    lsm_daemon_ipc_endpoint_free(relocated);
    if (relocated_runtime[0] != '\0') {
        (void)lsm_rmdir(relocated_runtime);
    }
    if (explicit_started) {
        bootstrap_endpoint_fixture_finish(&fixture);
    }
    (void)lsm_rmdir(override_parent);

    ASSERT_TRUE(prepared);
    ASSERT_TRUE(explicit_started);
    ASSERT_TRUE(explicit_canonical);
    ASSERT_TRUE(unusable_set);
    ASSERT_NOT_NULL(relocated);
    ASSERT_TRUE(bootstrap_path_has_parent(relocated_runtime, canonical_override));
    ASSERT_TRUE(bootstrap_path_has_parent(explicit_runtime, canonical_explicit));
    ASSERT_FALSE(bootstrap_path_has_parent(explicit_runtime, canonical_override));
    ASSERT_NULL(refused);
    PASS();
}

TEST(daemon_bootstrap_launches_only_exact_detached_hidden_role) {
    lsm_daemon_bootstrap_launch_spec_t spec;
    ASSERT_TRUE(lsm_daemon_bootstrap_launch_spec_init("/tmp/lsm exact", &spec));
    ASSERT_EQ(spec.argc, 2U);
    ASSERT_STR_EQ(spec.executable_path, "/tmp/lsm exact");
    ASSERT_STR_EQ(spec.argv[0], "/tmp/lsm exact");
    ASSERT_STR_EQ(spec.argv[1], LSM_DAEMON_INTERNAL_ARG);
    ASSERT_NULL(spec.argv[2]);
    ASSERT_TRUE(spec.detached);
    ASSERT_FALSE(spec.inherit_standard_handles);
    ASSERT_FALSE(spec.use_shell);
    PASS();
}

TEST(daemon_bootstrap_permanent_daemon_argv_is_byte_exact) {
    char *permanent[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, LSM_DAEMON_PERMANENT_ARG,
                         NULL};
    char *reordered[] = {"logan-spine-mcp", LSM_DAEMON_PERMANENT_ARG, LSM_DAEMON_INTERNAL_ARG,
                         NULL};
    char *repeated[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, LSM_DAEMON_INTERNAL_ARG,
                        NULL};
    char *extended[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, LSM_DAEMON_PERMANENT_ARG,
                        "extra", NULL};
    char *wrong_flag[] = {"logan-spine-mcp", LSM_DAEMON_INTERNAL_ARG, "--permanent", NULL};
    ASSERT_EQ(classify(3, permanent), LSM_DAEMON_PROCESS_DAEMON);
    ASSERT_EQ(classify(3, reordered), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(3, repeated), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(4, extended), LSM_DAEMON_PROCESS_INVALID);
    ASSERT_EQ(classify(3, wrong_flag), LSM_DAEMON_PROCESS_INVALID);
    PASS();
}

TEST(daemon_bootstrap_daemon_ctl_token_routes_after_cli) {
    char *start[] = {"logan-spine-mcp", "daemon", "start", NULL};
    char *stop[] = {"logan-spine-mcp", "daemon", "stop", NULL};
    char *status[] = {"logan-spine-mcp", "daemon", "status", NULL};
    char *help[] = {"logan-spine-mcp", "daemon", "--help", NULL};
    /* `daemon` after `cli` is opaque tool input, never a control command. */
    char *opaque[] = {"logan-spine-mcp", "cli", "search_code", "daemon", "start", NULL};
    ASSERT_EQ(classify(3, start), LSM_DAEMON_PROCESS_DAEMON_CTL);
    ASSERT_EQ(classify(3, stop), LSM_DAEMON_PROCESS_DAEMON_CTL);
    ASSERT_EQ(classify(3, status), LSM_DAEMON_PROCESS_DAEMON_CTL);
    ASSERT_EQ(classify(3, help), LSM_DAEMON_PROCESS_STATELESS);
    ASSERT_EQ(classify(5, opaque), LSM_DAEMON_PROCESS_LOCAL_CLI);
    ASSERT_FALSE(lsm_daemon_process_role_requires_client(LSM_DAEMON_PROCESS_DAEMON_CTL));
    PASS();
}

TEST(daemon_bootstrap_permanent_launch_spec_is_exact) {
    lsm_daemon_bootstrap_launch_spec_t spec;
    ASSERT_TRUE(lsm_daemon_bootstrap_launch_spec_init_permanent("/tmp/lsm exact", &spec));
    ASSERT_EQ(spec.argc, 3U);
    ASSERT_STR_EQ(spec.argv[0], "/tmp/lsm exact");
    ASSERT_STR_EQ(spec.argv[1], LSM_DAEMON_INTERNAL_ARG);
    ASSERT_STR_EQ(spec.argv[2], LSM_DAEMON_PERMANENT_ARG);
    ASSERT_NULL(spec.argv[3]);
    ASSERT_TRUE(spec.detached);
    ASSERT_FALSE(spec.inherit_standard_handles);
    ASSERT_FALSE(spec.use_shell);
    PASS();
}

TEST(daemon_bootstrap_stateless_roles_bypass_every_daemon_operation) {
    bootstrap_fake_ops_t fake = {0};
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_STATELESS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_BYPASSED);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_BYPASSED);
    ASSERT_EQ(atomic_load(&fake.cohort_acquire_count), 0);
    ASSERT_EQ(atomic_load(&fake.probe_count), 0);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    ASSERT_EQ(atomic_load(&fake.diagnostic_count), 0);
    PASS();
}

TEST(daemon_bootstrap_cohort_conflict_is_visible_before_probe_or_spawn) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "cohort-conflict"));
    bootstrap_fake_ops_t fake = {0};
    fake.forced_cohort = LSM_VERSION_COHORT_CONFLICT;
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_B);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 50,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONFLICT);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONFLICT);
    ASSERT_EQ(atomic_load(&fake.cohort_acquire_count), 1);
    ASSERT_EQ(atomic_load(&fake.cohort_release_count), 0);
    ASSERT_EQ(atomic_load(&fake.probe_count), 0);
    ASSERT_EQ(atomic_load(&fake.lock_attempt_count), 0);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    ASSERT_EQ(atomic_load(&fake.diagnostic_count), 1);
    ASSERT_NOT_NULL(strstr(fake.diagnostic, "conflicting LSM process is active"));
    ASSERT_NOT_NULL(strstr(fake.diagnostic, "Close all LSM sessions and commands"));
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

TEST(daemon_bootstrap_existing_exact_daemon_connects_without_spawn) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "existing"));
    bootstrap_fake_ops_t fake = {0};
    atomic_store(&fake.available, true);
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 50,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_NOT_NULL(result.client);
    ASSERT_FALSE(result.daemon_spawned);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

TEST(daemon_bootstrap_conflict_is_visible_and_never_spawns) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "conflict"));
    bootstrap_fake_ops_t fake = {0};
    fake.forced_probe = LSM_DAEMON_BOOTSTRAP_PROBE_CONFLICT;
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_B);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 50,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONFLICT);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONFLICT);
    ASSERT_NULL(result.client);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    ASSERT_EQ(atomic_load(&fake.diagnostic_count), 1);
    ASSERT_NOT_NULL(strstr(fake.diagnostic, "conflicting versions"));
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

TEST(daemon_bootstrap_terminal_generation_that_never_exits_is_not_replaced) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "terminal"));
    bootstrap_fake_ops_t fake = {0};
    fake.forced_probe = LSM_DAEMON_BOOTSTRAP_PROBE_TERMINAL;
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_HOOK_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_SHORT_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_FAILED);
    ASSERT_EQ(atomic_load(&fake.lock_held), 0);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

/* RED for final-session/new-session overlap: STOPPING is a temporary state of
 * the previous same-build generation. Once that generation disappears, the
 * already-running bootstrap attempt must serialize and become the new first
 * client instead of forcing the coding agent to restart its MCP process. */
TEST(daemon_bootstrap_terminal_then_absent_spawns_replacement) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "terminal-absent"));
    bootstrap_fake_ops_t fake = {0};
    atomic_store(&fake.terminal_probes_remaining, 1);
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_TRUE(result.daemon_spawned);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 1);
    ASSERT(atomic_load(&fake.lock_attempt_count) >= 1);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

TEST(daemon_bootstrap_reserved_generation_becomes_connectable_without_spawn) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "reserved-connect"));
    bootstrap_fake_ops_t fake = {0};
    atomic_store(&fake.reserved_probes_remaining, 2);
    atomic_store(&fake.connect_after_reserved, true);
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_FALSE(result.daemon_spawned);
    ASSERT_EQ(atomic_load(&fake.lock_attempt_count), 0);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 0);
    ASSERT(atomic_load(&fake.probe_count) >= 3);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

/* RED for the equivalent race when the old listener vanishes without first
 * returning the explicit STOPPING response. Startup serialization decides
 * whether absence is now safe; a historical RESERVED sample is not sticky. */
TEST(daemon_bootstrap_reserved_then_absent_spawns_replacement) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "reserved-absent"));
    bootstrap_fake_ops_t fake = {0};
    atomic_store(&fake.reserved_probes_remaining, 1);
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_TRUE(result.daemon_spawned);
    ASSERT(atomic_load(&fake.lock_attempt_count) >= 1);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 1);
    ASSERT(atomic_load(&fake.probe_count) > 1);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

/* RED for the native Windows lock order: after spawn, the generation claim or
 * lifetime reservation becomes visible before the client can connect. Daemon
 * participant teardown needs startup ownership, so bootstrap must release its
 * handoff once that generation is observable. */
TEST(daemon_bootstrap_releases_handoff_when_spawned_generation_is_reserved) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "spawn-admission"));
    bootstrap_fake_ops_t fake = {0};
    atomic_store(&fake.connect_requires_unlocked, true);
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    ASSERT_EQ(lsm_daemon_bootstrap_execute_with_ops(&config, &ops, &result),
              LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(result.status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_TRUE(result.daemon_spawned);
    ASSERT_NOT_NULL(result.client);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 1);
    ASSERT_EQ(atomic_load(&fake.lock_held), 0);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

TEST(daemon_bootstrap_rejected_connect_is_reserved_and_never_unavailable) {
    lsm_daemon_runtime_connect_result_t capacity = {0};
    capacity.status = LSM_DAEMON_RUNTIME_CONNECT_REJECTED;
    snprintf(capacity.message, sizeof(capacity.message), "LSM daemon connection capacity reached");
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&capacity, 1),
              LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED);
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&capacity, 0),
              LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED);
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&capacity, -1),
              LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED);

    lsm_daemon_runtime_connect_result_t stopping = capacity;
    snprintf(stopping.message, sizeof(stopping.message), "LSM daemon is stopping");
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&stopping, 1),
              LSM_DAEMON_BOOTSTRAP_PROBE_TERMINAL);

    lsm_daemon_runtime_connect_result_t absent = {0};
    absent.status = LSM_DAEMON_RUNTIME_CONNECT_ERROR;
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&absent, 1),
              LSM_DAEMON_BOOTSTRAP_PROBE_RESERVED);
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&absent, 0),
              LSM_DAEMON_BOOTSTRAP_PROBE_UNAVAILABLE);
    ASSERT_EQ(lsm_daemon_bootstrap_classify_failed_connect(&absent, -1),
              LSM_DAEMON_BOOTSTRAP_PROBE_ERROR);
    PASS();
}

TEST(daemon_bootstrap_concurrent_first_clients_spawn_one_daemon) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "startup-race"));
    bootstrap_fake_ops_t fake = {0};
    lsm_daemon_bootstrap_ops_t ops = bootstrap_fake_callbacks(&fake);
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = "/tmp/lsm",
        .connect_timeout_ms = 10,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    atomic_int ready = 0;
    atomic_bool go = false;
    bootstrap_thread_call_t calls[2] = {
        {.config = &config, .ops = &ops, .ready = &ready, .go = &go},
        {.config = &config, .ops = &ops, .ready = &ready, .go = &go},
    };
    lsm_thread_t threads[2];
    ASSERT_EQ(lsm_thread_create(&threads[0], 0, bootstrap_thread_execute, &calls[0]), 0);
    ASSERT_EQ(lsm_thread_create(&threads[1], 0, bootstrap_thread_execute, &calls[1]), 0);
    uint64_t ready_deadline = lsm_now_ms() + BOOTSTRAP_TEST_TIMEOUT_MS;
    while (atomic_load(&ready) != 2 && lsm_now_ms() < ready_deadline) {
        const struct timespec pause = {0, 1000000L};
        (void)lsm_nanosleep(&pause, NULL);
    }
    atomic_store(&go, true);
    ASSERT_EQ(lsm_thread_join(&threads[0]), 0);
    ASSERT_EQ(lsm_thread_join(&threads[1]), 0);
    ASSERT_EQ(calls[0].status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(calls[1].status, LSM_DAEMON_BOOTSTRAP_CONNECTED);
    ASSERT_EQ(atomic_load(&fake.spawn_count), 1);
    ASSERT_TRUE(calls[0].result.daemon_spawned != calls[1].result.daemon_spawned);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}

#ifdef __APPLE__
/* RED: the old double-fork returned success before its grandchild attempted
 * posix_spawn, hiding an immediate launch error behind the full timeout. */
TEST(daemon_bootstrap_darwin_launch_failure_is_synchronous) {
    bootstrap_endpoint_fixture_t fixture;
    ASSERT_TRUE(bootstrap_endpoint_fixture_start(&fixture, "darwin-missing"));
    lsm_daemon_build_identity_t identity = bootstrap_identity("2.4.0", BOOTSTRAP_BUILD_A);
    char missing[BOOTSTRAP_TEST_PATH_CAP];
    int written = snprintf(missing, sizeof(missing), "%s/definitely-missing-lsm", fixture.parent);
    ASSERT(written > 0 && written < (int)sizeof(missing));
    lsm_daemon_bootstrap_config_t config = {
        .role = LSM_DAEMON_PROCESS_MCP_CLIENT,
        .endpoint = fixture.endpoint,
        .identity = &identity,
        .executable_path = missing,
        .connect_timeout_ms = 1,
        .startup_timeout_ms = BOOTSTRAP_TEST_TIMEOUT_MS,
    };
    lsm_daemon_bootstrap_result_t result;
    uint64_t started = lsm_now_ms();
    ASSERT_EQ(lsm_daemon_bootstrap_execute(&config, &result), LSM_DAEMON_BOOTSTRAP_FAILED);
    uint64_t elapsed = lsm_now_ms() - started;
    ASSERT_FALSE(result.daemon_spawned);
    ASSERT(elapsed < BOOTSTRAP_TEST_TIMEOUT_MS / 2U);
    bootstrap_endpoint_fixture_finish(&fixture);
    PASS();
}
#endif

SUITE(daemon_bootstrap) {
    RUN_TEST(daemon_bootstrap_classifies_default_and_ui_as_mcp_clients);
    RUN_TEST(daemon_bootstrap_classifies_stateless_commands_without_client);
    RUN_TEST(daemon_bootstrap_classifies_config_as_coordinated_local_cli);
    RUN_TEST(daemon_bootstrap_cli_help_is_stateless_but_tool_calls_are_local);
    RUN_TEST(daemon_bootstrap_cli_arguments_cannot_reclassify_the_process);
    RUN_TEST(daemon_bootstrap_internal_roles_never_take_client_leases);
    RUN_TEST(daemon_bootstrap_rejects_ambiguous_internal_daemon_argv);
    RUN_TEST(daemon_bootstrap_uses_one_stable_per_account_endpoint);
    RUN_TEST(daemon_bootstrap_runtime_dir_env_relocates_rendezvous);
    RUN_TEST(daemon_bootstrap_launches_only_exact_detached_hidden_role);
    RUN_TEST(daemon_bootstrap_permanent_daemon_argv_is_byte_exact);
    RUN_TEST(daemon_bootstrap_daemon_ctl_token_routes_after_cli);
    RUN_TEST(daemon_bootstrap_permanent_launch_spec_is_exact);
    RUN_TEST(daemon_bootstrap_stateless_roles_bypass_every_daemon_operation);
    RUN_TEST(daemon_bootstrap_cohort_conflict_is_visible_before_probe_or_spawn);
    RUN_TEST(daemon_bootstrap_existing_exact_daemon_connects_without_spawn);
    RUN_TEST(daemon_bootstrap_conflict_is_visible_and_never_spawns);
    RUN_TEST(daemon_bootstrap_terminal_generation_that_never_exits_is_not_replaced);
    RUN_TEST(daemon_bootstrap_terminal_then_absent_spawns_replacement);
    RUN_TEST(daemon_bootstrap_reserved_generation_becomes_connectable_without_spawn);
    RUN_TEST(daemon_bootstrap_reserved_then_absent_spawns_replacement);
    RUN_TEST(daemon_bootstrap_releases_handoff_when_spawned_generation_is_reserved);
    RUN_TEST(daemon_bootstrap_rejected_connect_is_reserved_and_never_unavailable);
    RUN_TEST(daemon_bootstrap_concurrent_first_clients_spawn_one_daemon);
#ifdef __APPLE__
    RUN_TEST(daemon_bootstrap_darwin_launch_failure_is_synchronous);
#endif
}
