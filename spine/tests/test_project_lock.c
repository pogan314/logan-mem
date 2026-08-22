/* RED contract for daemon/local-CLI cross-process project coordination. */
#include "test_framework.h"
#include "test_helpers.h"

#include "daemon/ipc.h"
#include "daemon/project_lock.h"
#include "foundation/compat_fs.h"
#include "foundation/platform.h"

#include <stdint.h>
#include <stdio.h>

enum { PROJECT_LOCK_TEST_PATH_CAP = 1024 };

static void project_lock_test_release(lsm_project_lock_lease_t **lease) {
    while (lease && *lease && lsm_project_lock_lease_release(lease) != LSM_PRIVATE_FILE_LOCK_OK) {
        lsm_usleep(1000);
    }
}

TEST(project_lock_coordinates_instances_projects_wildcard_and_case_aliases) {
    char runtime_parent[PROJECT_LOCK_TEST_PATH_CAP];
    (void)snprintf(runtime_parent, sizeof(runtime_parent), "%s/lsm-project-lock-XXXXXX",
                   lsm_tmpdir());
    ASSERT_NOT_NULL(lsm_mkdtemp(runtime_parent));

    lsm_daemon_ipc_endpoint_t *endpoint =
        lsm_daemon_ipc_endpoint_new("0123456789abcdef", runtime_parent);
    lsm_project_lock_manager_t *first = lsm_project_lock_manager_new(endpoint);
    lsm_project_lock_manager_t *second = lsm_project_lock_manager_new(endpoint);
    ASSERT_NOT_NULL(endpoint);
    ASSERT_NOT_NULL(first);
    ASSERT_NOT_NULL(second);

    lsm_project_lock_lease_t *foo = NULL;
    lsm_project_lock_lease_t *alias = NULL;
    lsm_project_lock_lease_t *bar = NULL;
    ASSERT_EQ(lsm_project_lock_try_acquire(first, "Foo", &foo), LSM_PRIVATE_FILE_LOCK_OK);
    ASSERT_NOT_NULL(foo);
    ASSERT_EQ(lsm_project_lock_try_acquire(second, "foo", &alias), LSM_PRIVATE_FILE_LOCK_BUSY);
    ASSERT_NULL(alias);
    ASSERT_EQ(lsm_project_lock_acquire(second, "bar", UINT64_MAX, NULL, &bar),
              LSM_PRIVATE_FILE_LOCK_OK);
    ASSERT_NOT_NULL(bar);

    project_lock_test_release(&bar);
    project_lock_test_release(&foo);

    lsm_project_lock_lease_t *all = NULL;
    ASSERT_EQ(lsm_project_lock_acquire(first, "*", UINT64_MAX, NULL, &all),
              LSM_PRIVATE_FILE_LOCK_OK);
    ASSERT_NOT_NULL(all);
    ASSERT_EQ(lsm_project_lock_try_acquire(second, "unrelated", &bar), LSM_PRIVATE_FILE_LOCK_BUSY);
    ASSERT_NULL(bar);

    project_lock_test_release(&all);
    ASSERT_EQ(lsm_project_lock_manager_free(&second), LSM_PRIVATE_FILE_LOCK_OK);
    ASSERT_EQ(lsm_project_lock_manager_free(&first), LSM_PRIVATE_FILE_LOCK_OK);
    ASSERT_NULL(second);
    ASSERT_NULL(first);
    lsm_daemon_ipc_endpoint_free(endpoint);
    (void)th_rmtree(runtime_parent);
    PASS();
}

SUITE(project_lock) {
    RUN_TEST(project_lock_coordinates_instances_projects_wildcard_and_case_aliases);
}
