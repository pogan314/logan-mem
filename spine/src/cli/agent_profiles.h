/*
 * agent_profiles.h — Canonical tiered logan-spine agent profiles.
 */
#ifndef LSM_CLI_AGENT_PROFILES_H
#define LSM_CLI_AGENT_PROFILES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LSM_GRAPH_TIER_SCOUT = 0,
    LSM_GRAPH_TIER_VERIFY,
    LSM_GRAPH_TIER_AUDIT,
    LSM_GRAPH_TIER_COUNT
} lsm_graph_tier_t;

typedef enum {
    LSM_GRAPH_ACCESS_DIRECT = 0,
    LSM_GRAPH_ACCESS_HANDOFF,
    LSM_GRAPH_ACCESS_COUNT
} lsm_graph_access_t;

typedef enum {
    LSM_GRAPH_DIALECT_CLAUDE = 0,
    LSM_GRAPH_DIALECT_CODEX,
    LSM_GRAPH_DIALECT_GEMINI,
    LSM_GRAPH_DIALECT_QWEN,
    LSM_GRAPH_DIALECT_COPILOT,
    LSM_GRAPH_DIALECT_OPENCODE,
    LSM_GRAPH_DIALECT_KILO,
    LSM_GRAPH_DIALECT_KIRO,
    LSM_GRAPH_DIALECT_JUNIE,
    LSM_GRAPH_DIALECT_QODER,
    LSM_GRAPH_DIALECT_CODEBUDDY,
    LSM_GRAPH_DIALECT_FACTORY,
    LSM_GRAPH_DIALECT_VIBE,
    LSM_GRAPH_DIALECT_AUGMENT,
    LSM_GRAPH_DIALECT_CURSOR,
    LSM_GRAPH_DIALECT_ROVO,
    LSM_GRAPH_DIALECT_POCHI,
    LSM_GRAPH_DIALECT_COUNT
} lsm_graph_profile_dialect_t;

/* Stable profile identifier. VERIFY intentionally retains "logan-spine". */
const char *lsm_graph_tier_slug(lsm_graph_tier_t tier);
const char *lsm_graph_tier_display_name(lsm_graph_tier_t tier);
bool lsm_graph_dialect_direct_capable(lsm_graph_profile_dialect_t dialect);

/* Returns malloc-owned profile content, or NULL for invalid/unsafe combinations.
 * binary_path is required for direct Kiro and Codex profiles and ignored otherwise. */
char *lsm_render_graph_profile(lsm_graph_profile_dialect_t dialect, lsm_graph_tier_t tier,
                               lsm_graph_access_t access, const char *binary_path);

/* v0.9.1-rc.1 direct Codex rendering (server table without a transport), kept
 * so install/uninstall can recognize and migrate those files. */
char *lsm_render_graph_profile_codex_rc1(lsm_graph_tier_t tier);

/* Vibe stores the behavioral prompt separately from its TOML agent definition.
 * Other integrations may also use this as the canonical contract text. */
char *lsm_render_graph_prompt(lsm_graph_tier_t tier, lsm_graph_access_t access);

#ifdef __cplusplus
}
#endif

#endif /* LSM_CLI_AGENT_PROFILES_H */
