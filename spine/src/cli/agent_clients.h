/*
 * agent_clients.h — Table-driven agent client MCP installation profiles.
 */
#ifndef LSM_CLI_AGENT_CLIENTS_H
#define LSM_CLI_AGENT_CLIENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LSM_AGENT_CLIENT_QODER = 0,
    LSM_AGENT_CLIENT_KIMI,
    LSM_AGENT_CLIENT_GITLAB_DUO,
    LSM_AGENT_CLIENT_ROVO_DEV,
    LSM_AGENT_CLIENT_AMP,
    LSM_AGENT_CLIENT_DEVIN,
    LSM_AGENT_CLIENT_TABNINE,
    LSM_AGENT_CLIENT_CONTINUE,
    LSM_AGENT_CLIENT_VISUAL_STUDIO,
    LSM_AGENT_CLIENT_TRAE,
    LSM_AGENT_CLIENT_ROO_CODE,
    LSM_AGENT_CLIENT_AMAZON_Q,
    LSM_AGENT_CLIENT_CODEBUDDY,
    LSM_AGENT_CLIENT_IBM_BOB_IDE,
    LSM_AGENT_CLIENT_IBM_BOB_SHELL,
    LSM_AGENT_CLIENT_POCHI,
    LSM_AGENT_CLIENT_PI,
    LSM_AGENT_CLIENT_SOURCEGRAPH_CODY,
    LSM_AGENT_CLIENT_COUNT
} lsm_agent_client_id_t;

typedef enum {
    LSM_AGENT_STABLE = 0,
    LSM_AGENT_CONDITIONAL,
    LSM_AGENT_OPT_IN
} lsm_agent_client_stability_t;

enum {
    LSM_AGENT_CAP_MCP = UINT32_C(1) << 0,
    LSM_AGENT_CAP_INSTRUCTIONS = UINT32_C(1) << 1,
    LSM_AGENT_CAP_SKILL = UINT32_C(1) << 2,
    LSM_AGENT_CAP_AGENT = UINT32_C(1) << 3,
    LSM_AGENT_CAP_HOOK = UINT32_C(1) << 4,
    LSM_AGENT_CAP_PLUGIN = UINT32_C(1) << 5
};

typedef int (*lsm_agent_mcp_edit_fn)(lsm_agent_client_id_t id, const char *config_path,
                                     const char *binary_path);

typedef struct {
    lsm_agent_client_id_t id;
    const char *stable_id;
    const char *display_name;
    lsm_agent_client_stability_t stability;
    uint32_t capabilities;
    const char *detection_command;
    lsm_agent_mcp_edit_fn install_mcp;
    lsm_agent_mcp_edit_fn remove_mcp;
} lsm_agent_client_profile_t;

typedef bool (*lsm_agent_probe_fn)(const char *value, const void *context);

typedef struct {
    const char *home_dir;
    const char *xdg_config_home;
    const char *appdata_dir;
    const char *glab_config_dir;
    const char *kimi_code_home;
    const char *continue_config_path;
    const char *trae_config_path;
    const char *roo_config_path;
    const char *cody_config_path;
    bool is_windows;
    lsm_agent_probe_fn path_exists;
    lsm_agent_probe_fn command_exists;
    const void *probe_context;
} lsm_agent_client_resolve_options_t;

enum {
    LSM_AGENT_EDIT_ERROR = -1,
    LSM_AGENT_EDIT_OK = 0,
    LSM_AGENT_EDIT_FOREIGN = 1,
    LSM_AGENT_EDIT_NOT_APPLICABLE = 2
};

size_t lsm_agent_client_count(void);
const lsm_agent_client_profile_t *lsm_agent_client_at(size_t index);
const lsm_agent_client_profile_t *lsm_agent_client_by_id(lsm_agent_client_id_t id);
const lsm_agent_client_profile_t *lsm_agent_client_by_stable_id(const char *stable_id);

/* Resolves the documented user config path. Returns 0 on success, 1 when a
 * conditional target has no safe active path, and -1 for invalid input or an
 * ambiguous/unsupported configuration. */
int lsm_agent_client_resolve_path(lsm_agent_client_id_t id,
                                  const lsm_agent_client_resolve_options_t *options, char *path_out,
                                  size_t path_out_size);
bool lsm_agent_client_detect(lsm_agent_client_id_t id,
                             const lsm_agent_client_resolve_options_t *options);
bool lsm_agent_client_cleanup_candidate(lsm_agent_client_id_t id,
                                        const lsm_agent_client_resolve_options_t *options);

/* config_path must already have been resolved. The adapter never guesses a
 * target here. Existing same-name foreign entries fail closed with
 * LSM_AGENT_EDIT_FOREIGN. Removal requires the original installed binary path
 * and only removes the still-canonical entry. */
int lsm_agent_client_install_mcp(lsm_agent_client_id_t id, const char *config_path,
                                 const char *binary_path);
int lsm_agent_client_remove_mcp(lsm_agent_client_id_t id, const char *config_path,
                                const char *binary_path);

#ifdef __cplusplus
}
#endif

#endif /* LSM_CLI_AGENT_CLIENTS_H */
