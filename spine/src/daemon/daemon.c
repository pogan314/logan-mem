/*
 * daemon.c — Process-local coordination and wire framing for the LSM daemon.
 */
#include "daemon/daemon.h"

#include "foundation/compat_thread.h"

#include <stdlib.h>
#include <string.h>

typedef struct lsm_daemon_subscription {
    lsm_daemon_subscription_id_t id;
    lsm_daemon_client_id_t client_id;
    struct lsm_daemon_subscription *next;
} lsm_daemon_subscription_t;

typedef struct lsm_daemon_client {
    lsm_daemon_client_id_t id;
    uint64_t last_heartbeat_ms;
    struct lsm_daemon_client *next;
} lsm_daemon_client_t;

typedef struct lsm_daemon_job {
    char *project_key;
    lsm_daemon_job_state_t state;
    lsm_daemon_subscription_t *subscriptions;
    size_t subscription_count;
    bool cancel_callback_inflight;
    bool detached;
    struct lsm_daemon_job *next;
    struct lsm_daemon_job *action_next;
} lsm_daemon_job_t;

typedef struct lsm_daemon_watch {
    char *project_key;
    lsm_daemon_subscription_t *subscriptions;
    size_t subscription_count;
    struct lsm_daemon_watch *next;
    struct lsm_daemon_watch *action_next;
} lsm_daemon_watch_t;

struct lsm_daemon_coordinator {
    lsm_mutex_t mutex;
    lsm_daemon_client_t *clients;
    lsm_daemon_job_t *jobs;
    lsm_daemon_watch_t *watches;
    size_t client_count;
    /* See lsm_daemon_coordinator_set_permanent. */
    bool permanent;
    size_t job_count;
    size_t watch_count;
    size_t callback_count;
    uint64_t lease_timeout_ms;
    lsm_daemon_client_id_t last_client_id;
    lsm_daemon_subscription_id_t last_subscription_id;
    lsm_daemon_coordinator_state_t state;
    lsm_daemon_coordinator_hooks_t hooks;
};

typedef struct {
    lsm_daemon_job_t *jobs;
    lsm_daemon_watch_t *watches;
    lsm_daemon_job_cancel_fn cancel_job;
    lsm_daemon_watch_release_fn release_watch;
    void *context;
} lsm_daemon_callback_batch_t;

enum {
    FRAME_MAGIC_0 = 0,
    FRAME_MAGIC_1 = 1,
    FRAME_MAGIC_2 = 2,
    FRAME_MAGIC_3 = 3,
    FRAME_VERSION = 4,
    FRAME_TYPE = 5,
    FRAME_FLAGS_HI = 6,
    FRAME_FLAGS_LO = 7,
    FRAME_LENGTH_3 = 8,
    FRAME_LENGTH_2 = 9,
    FRAME_LENGTH_1 = 10,
    FRAME_LENGTH_0 = 11,
};

static bool frame_type_valid(lsm_daemon_frame_type_t type) {
    return type == LSM_DAEMON_FRAME_REQUEST || type == LSM_DAEMON_FRAME_RESPONSE;
}

static char *daemon_string_dup(const char *value) {
    size_t length = strlen(value);
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, value, length + 1);
    }
    return copy;
}

static void free_subscriptions(lsm_daemon_subscription_t *subscription) {
    while (subscription) {
        lsm_daemon_subscription_t *next = subscription->next;
        free(subscription);
        subscription = next;
    }
}

static void free_job(lsm_daemon_job_t *job) {
    if (job) {
        free_subscriptions(job->subscriptions);
        free(job->project_key);
        free(job);
    }
}

static void free_watch(lsm_daemon_watch_t *watch) {
    if (watch) {
        free_subscriptions(watch->subscriptions);
        free(watch->project_key);
        free(watch);
    }
}

static lsm_daemon_client_id_t issue_client_id_locked(lsm_daemon_coordinator_t *coordinator) {
    if (coordinator->last_client_id == UINT64_MAX) {
        return LSM_DAEMON_CLIENT_ID_INVALID;
    }
    coordinator->last_client_id++;
    return coordinator->last_client_id;
}

static lsm_daemon_subscription_id_t issue_subscription_id_locked(
    lsm_daemon_coordinator_t *coordinator) {
    if (coordinator->last_subscription_id == UINT64_MAX) {
        return LSM_DAEMON_SUBSCRIPTION_ID_INVALID;
    }
    coordinator->last_subscription_id++;
    return coordinator->last_subscription_id;
}

static lsm_daemon_client_t *find_client_locked(lsm_daemon_coordinator_t *coordinator,
                                               lsm_daemon_client_id_t client_id) {
    for (lsm_daemon_client_t *client = coordinator->clients; client; client = client->next) {
        if (client->id == client_id) {
            return client;
        }
    }
    return NULL;
}

static lsm_daemon_job_t *find_job_locked(lsm_daemon_coordinator_t *coordinator,
                                         const char *project_key) {
    for (lsm_daemon_job_t *job = coordinator->jobs; job; job = job->next) {
        if (strcmp(job->project_key, project_key) == 0) {
            return job;
        }
    }
    return NULL;
}

static lsm_daemon_watch_t *find_watch_locked(lsm_daemon_coordinator_t *coordinator,
                                             const char *project_key) {
    for (lsm_daemon_watch_t *watch = coordinator->watches; watch; watch = watch->next) {
        if (strcmp(watch->project_key, project_key) == 0) {
            return watch;
        }
    }
    return NULL;
}

static bool remove_subscription_locked(lsm_daemon_subscription_t **subscriptions,
                                       size_t *subscription_count, lsm_daemon_client_id_t client_id,
                                       lsm_daemon_subscription_id_t subscription_id) {
    lsm_daemon_subscription_t **cursor = subscriptions;
    while (*cursor) {
        lsm_daemon_subscription_t *subscription = *cursor;
        if (subscription->id == subscription_id && subscription->client_id == client_id) {
            *cursor = subscription->next;
            free(subscription);
            (*subscription_count)--;
            return true;
        }
        cursor = &subscription->next;
    }
    return false;
}

static void remove_client_subscriptions_locked(lsm_daemon_subscription_t **subscriptions,
                                               size_t *subscription_count,
                                               lsm_daemon_client_id_t client_id) {
    lsm_daemon_subscription_t **cursor = subscriptions;
    while (*cursor) {
        lsm_daemon_subscription_t *subscription = *cursor;
        if (subscription->client_id == client_id) {
            *cursor = subscription->next;
            free(subscription);
            (*subscription_count)--;
        } else {
            cursor = &subscription->next;
        }
    }
}

static void callback_batch_init_locked(lsm_daemon_coordinator_t *coordinator,
                                       lsm_daemon_callback_batch_t *batch) {
    memset(batch, 0, sizeof(*batch));
    batch->cancel_job = coordinator->hooks.cancel_job;
    batch->release_watch = coordinator->hooks.release_watch;
    batch->context = coordinator->hooks.context;
}

static void request_job_cancel_locked(lsm_daemon_coordinator_t *coordinator, lsm_daemon_job_t *job,
                                      lsm_daemon_callback_batch_t *batch) {
    if (job->subscription_count != 0 || job->state != LSM_DAEMON_JOB_RUNNING) {
        return;
    }
    job->state = LSM_DAEMON_JOB_CANCEL_REQUESTED;
    if (batch->cancel_job) {
        job->cancel_callback_inflight = true;
        job->action_next = batch->jobs;
        batch->jobs = job;
        coordinator->callback_count++;
    }
}

static void queue_watch_release_locked(lsm_daemon_coordinator_t *coordinator,
                                       lsm_daemon_watch_t *watch,
                                       lsm_daemon_callback_batch_t *batch) {
    watch->action_next = batch->watches;
    batch->watches = watch;
    if (batch->release_watch) {
        coordinator->callback_count++;
    }
}

static void callback_batch_run(lsm_daemon_coordinator_t *coordinator,
                               lsm_daemon_callback_batch_t *batch) {
    lsm_daemon_job_t *job = batch->jobs;
    while (job) {
        lsm_daemon_job_t *next = job->action_next;
        batch->cancel_job(job->project_key, batch->context);

        lsm_mutex_lock(&coordinator->mutex);
        coordinator->callback_count--;
        job->cancel_callback_inflight = false;
        bool detached = job->detached;
        lsm_mutex_unlock(&coordinator->mutex);
        if (detached) {
            free_job(job);
        }
        job = next;
    }

    lsm_daemon_watch_t *watch = batch->watches;
    while (watch) {
        lsm_daemon_watch_t *next = watch->action_next;
        if (batch->release_watch) {
            batch->release_watch(watch->project_key, batch->context);
            lsm_mutex_lock(&coordinator->mutex);
            coordinator->callback_count--;
            lsm_mutex_unlock(&coordinator->mutex);
        }
        free_watch(watch);
        watch = next;
    }
}

static void release_client_resources_locked(lsm_daemon_coordinator_t *coordinator,
                                            lsm_daemon_client_id_t client_id,
                                            lsm_daemon_callback_batch_t *batch) {
    for (lsm_daemon_job_t *job = coordinator->jobs; job; job = job->next) {
        remove_client_subscriptions_locked(&job->subscriptions, &job->subscription_count,
                                           client_id);
        request_job_cancel_locked(coordinator, job, batch);
    }

    lsm_daemon_watch_t **watch_cursor = &coordinator->watches;
    while (*watch_cursor) {
        lsm_daemon_watch_t *watch = *watch_cursor;
        remove_client_subscriptions_locked(&watch->subscriptions, &watch->subscription_count,
                                           client_id);
        if (watch->subscription_count == 0) {
            *watch_cursor = watch->next;
            watch->next = NULL;
            coordinator->watch_count--;
            queue_watch_release_locked(coordinator, watch, batch);
        } else {
            watch_cursor = &watch->next;
        }
    }
}

static void release_client_locked(lsm_daemon_coordinator_t *coordinator,
                                  lsm_daemon_client_t *client, lsm_daemon_callback_batch_t *batch) {
    release_client_resources_locked(coordinator, client->id, batch);
    free(client);
    coordinator->client_count--;
    if (coordinator->client_count == 0 && !coordinator->permanent) {
        coordinator->state = LSM_DAEMON_COORDINATOR_STOPPING;
    }
}

static bool terminal_job_locked(lsm_daemon_coordinator_t *coordinator, const char *project_key,
                                bool require_cancellation, lsm_daemon_job_t **free_after_unlock) {
    lsm_daemon_job_t **cursor = &coordinator->jobs;
    while (*cursor && strcmp((*cursor)->project_key, project_key) != 0) {
        cursor = &(*cursor)->next;
    }
    if (!*cursor || (require_cancellation && (*cursor)->state == LSM_DAEMON_JOB_RUNNING)) {
        return false;
    }

    lsm_daemon_job_t *job = *cursor;
    *cursor = job->next;
    job->next = NULL;
    job->detached = true;
    coordinator->job_count--;
    free_subscriptions(job->subscriptions);
    job->subscriptions = NULL;
    job->subscription_count = 0;
    if (!job->cancel_callback_inflight) {
        *free_after_unlock = job;
    }
    return true;
}

void lsm_daemon_coordinator_set_permanent(lsm_daemon_coordinator_t *coordinator, bool permanent) {
    if (!coordinator) {
        return;
    }
    lsm_mutex_lock(&coordinator->mutex);
    coordinator->permanent = permanent;
    lsm_mutex_unlock(&coordinator->mutex);
}

lsm_daemon_coordinator_t *lsm_daemon_coordinator_new(uint64_t lease_timeout_ms) {
    lsm_daemon_coordinator_t *coordinator = calloc(1, sizeof(*coordinator));
    if (!coordinator) {
        return NULL;
    }
    lsm_mutex_init(&coordinator->mutex);
    coordinator->lease_timeout_ms = lease_timeout_ms;
    coordinator->state = LSM_DAEMON_COORDINATOR_RUNNING;
    return coordinator;
}

void lsm_daemon_coordinator_free(lsm_daemon_coordinator_t *coordinator) {
    if (!coordinator) {
        return;
    }

    lsm_daemon_client_t *client = coordinator->clients;
    while (client) {
        lsm_daemon_client_t *next = client->next;
        free(client);
        client = next;
    }

    lsm_daemon_job_t *job = coordinator->jobs;
    while (job) {
        lsm_daemon_job_t *next = job->next;
        free_job(job);
        job = next;
    }

    lsm_daemon_watch_t *watch = coordinator->watches;
    while (watch) {
        lsm_daemon_watch_t *next = watch->next;
        free_watch(watch);
        watch = next;
    }
    lsm_mutex_destroy(&coordinator->mutex);
    free(coordinator);
}

bool lsm_daemon_coordinator_set_hooks(lsm_daemon_coordinator_t *coordinator,
                                      const lsm_daemon_coordinator_hooks_t *hooks) {
    if (!coordinator || !hooks) {
        return false;
    }
    lsm_mutex_lock(&coordinator->mutex);
    coordinator->hooks = *hooks;
    lsm_mutex_unlock(&coordinator->mutex);
    return true;
}

lsm_daemon_coordinator_state_t lsm_daemon_coordinator_state(lsm_daemon_coordinator_t *coordinator) {
    if (!coordinator) {
        return LSM_DAEMON_COORDINATOR_STOPPING;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_coordinator_state_t state = coordinator->state;
    lsm_mutex_unlock(&coordinator->mutex);
    return state;
}

lsm_daemon_client_id_t lsm_daemon_client_connected(lsm_daemon_coordinator_t *coordinator,
                                                   uint64_t now_ms) {
    if (!coordinator) {
        return LSM_DAEMON_CLIENT_ID_INVALID;
    }

    lsm_daemon_client_t *client = malloc(sizeof(*client));
    if (!client) {
        return LSM_DAEMON_CLIENT_ID_INVALID;
    }

    lsm_mutex_lock(&coordinator->mutex);
    if (coordinator->state != LSM_DAEMON_COORDINATOR_RUNNING) {
        lsm_mutex_unlock(&coordinator->mutex);
        free(client);
        return LSM_DAEMON_CLIENT_ID_INVALID;
    }
    lsm_daemon_client_id_t client_id = issue_client_id_locked(coordinator);
    if (client_id == LSM_DAEMON_CLIENT_ID_INVALID) {
        lsm_mutex_unlock(&coordinator->mutex);
        free(client);
        return LSM_DAEMON_CLIENT_ID_INVALID;
    }
    client->id = client_id;
    client->last_heartbeat_ms = now_ms;
    client->next = coordinator->clients;
    coordinator->clients = client;
    coordinator->client_count++;
    lsm_mutex_unlock(&coordinator->mutex);
    return client_id;
}

bool lsm_daemon_client_disconnected(lsm_daemon_coordinator_t *coordinator,
                                    lsm_daemon_client_id_t client_id, uint64_t now_ms) {
    (void)now_ms;
    if (!coordinator || client_id == LSM_DAEMON_CLIENT_ID_INVALID) {
        return false;
    }

    lsm_daemon_callback_batch_t batch;
    lsm_mutex_lock(&coordinator->mutex);
    callback_batch_init_locked(coordinator, &batch);
    lsm_daemon_client_t **cursor = &coordinator->clients;
    while (*cursor && (*cursor)->id != client_id) {
        cursor = &(*cursor)->next;
    }
    if (!*cursor) {
        lsm_mutex_unlock(&coordinator->mutex);
        return false;
    }
    lsm_daemon_client_t *client = *cursor;
    *cursor = client->next;
    release_client_locked(coordinator, client, &batch);
    lsm_mutex_unlock(&coordinator->mutex);

    callback_batch_run(coordinator, &batch);
    return true;
}

bool lsm_daemon_client_heartbeat(lsm_daemon_coordinator_t *coordinator,
                                 lsm_daemon_client_id_t client_id, uint64_t now_ms) {
    if (!coordinator || client_id == LSM_DAEMON_CLIENT_ID_INVALID) {
        return false;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_client_t *client = find_client_locked(coordinator, client_id);
    bool found = client != NULL;
    if (client && now_ms > client->last_heartbeat_ms) {
        client->last_heartbeat_ms = now_ms;
    }
    lsm_mutex_unlock(&coordinator->mutex);
    return found;
}

size_t lsm_daemon_expire_leases(lsm_daemon_coordinator_t *coordinator, uint64_t now_ms) {
    if (!coordinator) {
        return 0;
    }

    size_t expired_count = 0;
    lsm_daemon_callback_batch_t batch;
    lsm_mutex_lock(&coordinator->mutex);
    callback_batch_init_locked(coordinator, &batch);
    lsm_daemon_client_t **cursor = &coordinator->clients;
    while (*cursor) {
        lsm_daemon_client_t *client = *cursor;
        bool expired = now_ms >= client->last_heartbeat_ms &&
                       now_ms - client->last_heartbeat_ms >= coordinator->lease_timeout_ms;
        if (!expired) {
            cursor = &client->next;
            continue;
        }
        *cursor = client->next;
        release_client_locked(coordinator, client, &batch);
        expired_count++;
    }
    lsm_mutex_unlock(&coordinator->mutex);

    callback_batch_run(coordinator, &batch);
    return expired_count;
}

size_t lsm_daemon_active_clients(lsm_daemon_coordinator_t *coordinator) {
    if (!coordinator) {
        return 0;
    }
    lsm_mutex_lock(&coordinator->mutex);
    size_t count = coordinator->client_count;
    lsm_mutex_unlock(&coordinator->mutex);
    return count;
}

lsm_daemon_subscription_result_t lsm_daemon_job_subscribe(
    lsm_daemon_coordinator_t *coordinator, lsm_daemon_client_id_t client_id,
    const char *project_key, lsm_daemon_subscription_id_t *subscription_id) {
    if (subscription_id) {
        *subscription_id = LSM_DAEMON_SUBSCRIPTION_ID_INVALID;
    }
    if (!coordinator || !subscription_id || !project_key || project_key[0] == '\0' ||
        client_id == LSM_DAEMON_CLIENT_ID_INVALID) {
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }

    lsm_mutex_lock(&coordinator->mutex);
    if (coordinator->state != LSM_DAEMON_COORDINATOR_RUNNING ||
        !find_client_locked(coordinator, client_id)) {
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }
    lsm_daemon_job_t *job = find_job_locked(coordinator, project_key);
    if (job && job->state != LSM_DAEMON_JOB_RUNNING) {
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }

    bool started = job == NULL;
    lsm_daemon_job_t *new_job = NULL;
    char *key_copy = NULL;
    lsm_daemon_subscription_t *subscription = malloc(sizeof(*subscription));
    if (started) {
        new_job = calloc(1, sizeof(*new_job));
        key_copy = daemon_string_dup(project_key);
    }
    if (!subscription || (started && (!new_job || !key_copy))) {
        free(subscription);
        free(new_job);
        free(key_copy);
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }

    lsm_daemon_subscription_id_t id = issue_subscription_id_locked(coordinator);
    if (id == LSM_DAEMON_SUBSCRIPTION_ID_INVALID) {
        free(subscription);
        free(new_job);
        free(key_copy);
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }
    if (started) {
        new_job->project_key = key_copy;
        new_job->state = LSM_DAEMON_JOB_RUNNING;
        new_job->next = coordinator->jobs;
        coordinator->jobs = new_job;
        coordinator->job_count++;
        job = new_job;
    }
    subscription->id = id;
    subscription->client_id = client_id;
    subscription->next = job->subscriptions;
    job->subscriptions = subscription;
    job->subscription_count++;
    *subscription_id = id;
    lsm_mutex_unlock(&coordinator->mutex);
    return started ? LSM_DAEMON_SUBSCRIPTION_STARTED : LSM_DAEMON_SUBSCRIPTION_JOINED;
}

lsm_daemon_subscription_result_t lsm_daemon_watch_subscribe(
    lsm_daemon_coordinator_t *coordinator, lsm_daemon_client_id_t client_id,
    const char *project_key, lsm_daemon_subscription_id_t *subscription_id) {
    if (subscription_id) {
        *subscription_id = LSM_DAEMON_SUBSCRIPTION_ID_INVALID;
    }
    if (!coordinator || !subscription_id || !project_key || project_key[0] == '\0' ||
        client_id == LSM_DAEMON_CLIENT_ID_INVALID) {
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }

    lsm_mutex_lock(&coordinator->mutex);
    if (coordinator->state != LSM_DAEMON_COORDINATOR_RUNNING ||
        !find_client_locked(coordinator, client_id)) {
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }
    lsm_daemon_watch_t *watch = find_watch_locked(coordinator, project_key);
    bool started = watch == NULL;
    lsm_daemon_watch_t *new_watch = NULL;
    char *key_copy = NULL;
    lsm_daemon_subscription_t *subscription = malloc(sizeof(*subscription));
    if (started) {
        new_watch = calloc(1, sizeof(*new_watch));
        key_copy = daemon_string_dup(project_key);
    }
    if (!subscription || (started && (!new_watch || !key_copy))) {
        free(subscription);
        free(new_watch);
        free(key_copy);
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }

    lsm_daemon_subscription_id_t id = issue_subscription_id_locked(coordinator);
    if (id == LSM_DAEMON_SUBSCRIPTION_ID_INVALID) {
        free(subscription);
        free(new_watch);
        free(key_copy);
        lsm_mutex_unlock(&coordinator->mutex);
        return LSM_DAEMON_SUBSCRIPTION_REJECTED;
    }
    if (started) {
        new_watch->project_key = key_copy;
        new_watch->next = coordinator->watches;
        coordinator->watches = new_watch;
        coordinator->watch_count++;
        watch = new_watch;
    }
    subscription->id = id;
    subscription->client_id = client_id;
    subscription->next = watch->subscriptions;
    watch->subscriptions = subscription;
    watch->subscription_count++;
    *subscription_id = id;
    lsm_mutex_unlock(&coordinator->mutex);
    return started ? LSM_DAEMON_SUBSCRIPTION_STARTED : LSM_DAEMON_SUBSCRIPTION_JOINED;
}

bool lsm_daemon_job_unsubscribe(lsm_daemon_coordinator_t *coordinator,
                                lsm_daemon_client_id_t client_id,
                                lsm_daemon_subscription_id_t subscription_id) {
    if (!coordinator || client_id == LSM_DAEMON_CLIENT_ID_INVALID ||
        subscription_id == LSM_DAEMON_SUBSCRIPTION_ID_INVALID) {
        return false;
    }

    lsm_daemon_callback_batch_t batch;
    lsm_mutex_lock(&coordinator->mutex);
    callback_batch_init_locked(coordinator, &batch);
    if (!find_client_locked(coordinator, client_id)) {
        lsm_mutex_unlock(&coordinator->mutex);
        return false;
    }
    bool removed = false;
    for (lsm_daemon_job_t *job = coordinator->jobs; job; job = job->next) {
        removed = remove_subscription_locked(&job->subscriptions, &job->subscription_count,
                                             client_id, subscription_id);
        if (removed) {
            request_job_cancel_locked(coordinator, job, &batch);
            break;
        }
    }
    lsm_mutex_unlock(&coordinator->mutex);
    callback_batch_run(coordinator, &batch);
    return removed;
}

bool lsm_daemon_watch_unsubscribe(lsm_daemon_coordinator_t *coordinator,
                                  lsm_daemon_client_id_t client_id,
                                  lsm_daemon_subscription_id_t subscription_id) {
    if (!coordinator || client_id == LSM_DAEMON_CLIENT_ID_INVALID ||
        subscription_id == LSM_DAEMON_SUBSCRIPTION_ID_INVALID) {
        return false;
    }

    lsm_daemon_callback_batch_t batch;
    lsm_mutex_lock(&coordinator->mutex);
    callback_batch_init_locked(coordinator, &batch);
    if (!find_client_locked(coordinator, client_id)) {
        lsm_mutex_unlock(&coordinator->mutex);
        return false;
    }
    bool removed = false;
    lsm_daemon_watch_t **cursor = &coordinator->watches;
    while (*cursor) {
        lsm_daemon_watch_t *watch = *cursor;
        removed = remove_subscription_locked(&watch->subscriptions, &watch->subscription_count,
                                             client_id, subscription_id);
        if (!removed) {
            cursor = &watch->next;
            continue;
        }
        if (watch->subscription_count == 0) {
            *cursor = watch->next;
            watch->next = NULL;
            coordinator->watch_count--;
            queue_watch_release_locked(coordinator, watch, &batch);
        }
        break;
    }
    lsm_mutex_unlock(&coordinator->mutex);
    callback_batch_run(coordinator, &batch);
    return removed;
}

size_t lsm_daemon_job_subscribers(lsm_daemon_coordinator_t *coordinator, const char *project_key) {
    if (!coordinator || !project_key) {
        return 0;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_job_t *job = find_job_locked(coordinator, project_key);
    size_t count = job ? job->subscription_count : 0;
    lsm_mutex_unlock(&coordinator->mutex);
    return count;
}

size_t lsm_daemon_watch_subscribers(lsm_daemon_coordinator_t *coordinator,
                                    const char *project_key) {
    if (!coordinator || !project_key) {
        return 0;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_watch_t *watch = find_watch_locked(coordinator, project_key);
    size_t count = watch ? watch->subscription_count : 0;
    lsm_mutex_unlock(&coordinator->mutex);
    return count;
}

size_t lsm_daemon_active_jobs(lsm_daemon_coordinator_t *coordinator) {
    if (!coordinator) {
        return 0;
    }
    lsm_mutex_lock(&coordinator->mutex);
    size_t count = coordinator->job_count;
    lsm_mutex_unlock(&coordinator->mutex);
    return count;
}

size_t lsm_daemon_active_watches(lsm_daemon_coordinator_t *coordinator) {
    if (!coordinator) {
        return 0;
    }
    lsm_mutex_lock(&coordinator->mutex);
    size_t count = coordinator->watch_count;
    lsm_mutex_unlock(&coordinator->mutex);
    return count;
}

lsm_daemon_job_state_t lsm_daemon_job_state(lsm_daemon_coordinator_t *coordinator,
                                            const char *project_key) {
    if (!coordinator || !project_key) {
        return LSM_DAEMON_JOB_NONE;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_job_t *job = find_job_locked(coordinator, project_key);
    lsm_daemon_job_state_t state = job ? job->state : LSM_DAEMON_JOB_NONE;
    lsm_mutex_unlock(&coordinator->mutex);
    return state;
}

bool lsm_daemon_job_reaping(lsm_daemon_coordinator_t *coordinator, const char *project_key) {
    if (!coordinator || !project_key) {
        return false;
    }
    lsm_mutex_lock(&coordinator->mutex);
    lsm_daemon_job_t *job = find_job_locked(coordinator, project_key);
    bool transitioned = job && job->state == LSM_DAEMON_JOB_CANCEL_REQUESTED;
    if (transitioned) {
        job->state = LSM_DAEMON_JOB_REAPING;
    }
    lsm_mutex_unlock(&coordinator->mutex);
    return transitioned;
}

bool lsm_daemon_job_reaped(lsm_daemon_coordinator_t *coordinator, const char *project_key,
                           uint64_t now_ms) {
    (void)now_ms;
    if (!coordinator || !project_key) {
        return false;
    }
    lsm_daemon_job_t *free_after_unlock = NULL;
    lsm_mutex_lock(&coordinator->mutex);
    bool removed = terminal_job_locked(coordinator, project_key, true, &free_after_unlock);
    lsm_mutex_unlock(&coordinator->mutex);
    free_job(free_after_unlock);
    return removed;
}

bool lsm_daemon_job_completed(lsm_daemon_coordinator_t *coordinator, const char *project_key,
                              uint64_t now_ms) {
    (void)now_ms;
    if (!coordinator || !project_key) {
        return false;
    }
    lsm_daemon_job_t *free_after_unlock = NULL;
    lsm_mutex_lock(&coordinator->mutex);
    bool removed = terminal_job_locked(coordinator, project_key, false, &free_after_unlock);
    lsm_mutex_unlock(&coordinator->mutex);
    free_job(free_after_unlock);
    return removed;
}

bool lsm_daemon_should_exit(lsm_daemon_coordinator_t *coordinator, uint64_t now_ms) {
    (void)now_ms;
    if (!coordinator) {
        return false;
    }
    lsm_mutex_lock(&coordinator->mutex);
    bool should_exit = coordinator->state == LSM_DAEMON_COORDINATOR_STOPPING &&
                       coordinator->client_count == 0 && coordinator->job_count == 0 &&
                       coordinator->watch_count == 0 && coordinator->callback_count == 0;
    lsm_mutex_unlock(&coordinator->mutex);
    return should_exit;
}

bool lsm_daemon_frame_header_encode(uint8_t header[LSM_DAEMON_FRAME_HEADER_SIZE],
                                    lsm_daemon_frame_type_t type, uint16_t flags, uint32_t length) {
    if (!header || !frame_type_valid(type) || length > LSM_DAEMON_MAX_FRAME_SIZE) {
        return false;
    }
    header[FRAME_MAGIC_0] = 'C';
    header[FRAME_MAGIC_1] = 'B';
    header[FRAME_MAGIC_2] = 'M';
    header[FRAME_MAGIC_3] = 'D';
    header[FRAME_VERSION] = LSM_DAEMON_RENDEZVOUS_FRAME_VERSION;
    header[FRAME_TYPE] = (uint8_t)type;
    header[FRAME_FLAGS_HI] = (uint8_t)(flags >> 8);
    header[FRAME_FLAGS_LO] = (uint8_t)flags;
    header[FRAME_LENGTH_3] = (uint8_t)(length >> 24);
    header[FRAME_LENGTH_2] = (uint8_t)(length >> 16);
    header[FRAME_LENGTH_1] = (uint8_t)(length >> 8);
    header[FRAME_LENGTH_0] = (uint8_t)length;
    return true;
}

bool lsm_daemon_frame_header_decode(const uint8_t header[LSM_DAEMON_FRAME_HEADER_SIZE],
                                    lsm_daemon_frame_t *frame) {
    if (!header || !frame || header[FRAME_MAGIC_0] != 'C' || header[FRAME_MAGIC_1] != 'B' ||
        header[FRAME_MAGIC_2] != 'M' || header[FRAME_MAGIC_3] != 'D' ||
        header[FRAME_VERSION] != LSM_DAEMON_RENDEZVOUS_FRAME_VERSION) {
        return false;
    }

    lsm_daemon_frame_type_t type = (lsm_daemon_frame_type_t)header[FRAME_TYPE];
    uint32_t length = ((uint32_t)header[FRAME_LENGTH_3] << 24) |
                      ((uint32_t)header[FRAME_LENGTH_2] << 16) |
                      ((uint32_t)header[FRAME_LENGTH_1] << 8) | (uint32_t)header[FRAME_LENGTH_0];
    if (!frame_type_valid(type) || length > LSM_DAEMON_MAX_FRAME_SIZE) {
        return false;
    }

    frame->type = type;
    frame->flags =
        (uint16_t)(((uint16_t)header[FRAME_FLAGS_HI] << 8) | (uint16_t)header[FRAME_FLAGS_LO]);
    frame->length = length;
    return true;
}
