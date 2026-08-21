/* test_agent_profiles.c — Canonical Scout/Verify/Audit renderer contracts. */
#include "test_framework.h"

#include <cli/agent_profiles.h>
#include <yyjson/yyjson.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    lsm_graph_profile_dialect_t dialect;
    const char *syntax_fragment;
    const char *read_fragment;
    const char *grep_fragment;
} direct_dialect_expectation_t;

static const direct_dialect_expectation_t direct_dialects[] = {
    {LSM_GRAPH_DIALECT_CLAUDE, "permissionMode: plan", "  - Read\n", "  - Grep\n"},
    {LSM_GRAPH_DIALECT_CODEX, "sandbox_mode = \"read-only\"", "read", "grep"},
    {LSM_GRAPH_DIALECT_GEMINI, "kind: local", "  - read_file\n", "  - grep_search\n"},
    {LSM_GRAPH_DIALECT_QWEN, "approvalMode: plan", "  - read_file\n", "  - grep_search\n"},
    {LSM_GRAPH_DIALECT_COPILOT, "logan-spine-mcp/check_index_coverage", "  - read\n",
     "source read/grep fallback"},
    {LSM_GRAPH_DIALECT_OPENCODE, "  \"*\": deny", "  read: allow", "  grep: allow"},
    {LSM_GRAPH_DIALECT_KILO, "mode: subagent", "  read: allow", "  grep: allow"},
    {LSM_GRAPH_DIALECT_KIRO, "\"includeMcpJson\": false", "\"read\"", "\"grep\""},
    {LSM_GRAPH_DIALECT_JUNIE, "mcpServers: [\"logan-spine-", "\"Read\"", "\"Grep\""},
    {LSM_GRAPH_DIALECT_QODER, "mcp__logan-spine-mcp__check_index_coverage",
     "tools: Read,Grep,Glob,mcp__logan-spine-mcp__", "Read,Grep"},
    {LSM_GRAPH_DIALECT_CODEBUDDY, "permissionMode: plan", "tools: Read,Grep,Glob,", "Read,Grep"},
    {LSM_GRAPH_DIALECT_FACTORY, "mcp__logan-spine-mcp__check_index_coverage",
     "tools: [\"Read\", \"LS\", \"Grep\", \"Glob\"", "source read/grep fallback"},
    {LSM_GRAPH_DIALECT_VIBE, "agent_type = \"subagent\"", "\"read_file\"", "\"grep_search\""},
};

static const lsm_graph_profile_dialect_t handoff_only_dialects[] = {
    LSM_GRAPH_DIALECT_AUGMENT,
    LSM_GRAPH_DIALECT_CURSOR,
    LSM_GRAPH_DIALECT_ROVO,
    LSM_GRAPH_DIALECT_POCHI,
};

static int profile_has_mutator(const char *profile) {
    static const char *const mutators[] = {
        "index_repository",
        "delete_project",
        "manage_adr",
        "ingest_traces",
    };
    for (size_t i = 0U; i < sizeof(mutators) / sizeof(mutators[0]); i++) {
        if (strstr(profile, mutators[i])) {
            return 1;
        }
    }
    return 0;
}

TEST(agent_profiles_stable_tier_identity) {
    ASSERT_STR_EQ(lsm_graph_tier_slug(LSM_GRAPH_TIER_SCOUT), "logan-spine-scout");
    ASSERT_STR_EQ(lsm_graph_tier_slug(LSM_GRAPH_TIER_VERIFY), "logan-spine");
    ASSERT_STR_EQ(lsm_graph_tier_slug(LSM_GRAPH_TIER_AUDIT), "logan-spine-auditor");
    ASSERT_STR_EQ(lsm_graph_tier_display_name(LSM_GRAPH_TIER_SCOUT), "Logan Spine Scout");
    ASSERT_STR_EQ(lsm_graph_tier_display_name(LSM_GRAPH_TIER_VERIFY), "Logan Spine Verify");
    ASSERT_STR_EQ(lsm_graph_tier_display_name(LSM_GRAPH_TIER_AUDIT), "Logan Spine Auditor");
    ASSERT_TRUE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_CLAUDE));
    ASSERT_TRUE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_KIRO));
    ASSERT_FALSE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_AUGMENT));
    ASSERT_FALSE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_CURSOR));
    ASSERT_FALSE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_ROVO));
    ASSERT_FALSE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_POCHI));
    ASSERT_FALSE(lsm_graph_dialect_direct_capable(LSM_GRAPH_DIALECT_COUNT));
    ASSERT_NULL(lsm_graph_tier_slug(LSM_GRAPH_TIER_COUNT));
    ASSERT_NULL(lsm_graph_tier_display_name(LSM_GRAPH_TIER_COUNT));
    PASS();
}

TEST(agent_profiles_direct_dialects_are_coverage_aware_and_read_only) {
    for (size_t i = 0U; i < sizeof(direct_dialects) / sizeof(direct_dialects[0]); i++) {
        const direct_dialect_expectation_t *expectation = &direct_dialects[i];
        for (int tier = 0; tier < (int)LSM_GRAPH_TIER_COUNT; tier++) {
            const char *binary = expectation->dialect == LSM_GRAPH_DIALECT_KIRO ||
                                         expectation->dialect == LSM_GRAPH_DIALECT_CODEX
                                     ? "/opt/logan spine/lsm"
                                     : NULL;
            char *profile = lsm_render_graph_profile(expectation->dialect, (lsm_graph_tier_t)tier,
                                                     LSM_GRAPH_ACCESS_DIRECT, binary);
            if (!profile) {
                FAIL("every documented direct dialect must render all three tiers");
            }
            int valid = strstr(profile, "logan-spine") != NULL &&
                        strstr(profile, "check_index_coverage") != NULL &&
                        strstr(profile, expectation->syntax_fragment) != NULL &&
                        strstr(profile, expectation->read_fragment) != NULL &&
                        strstr(profile, expectation->grep_fragment) != NULL &&
                        strstr(profile, "source read/grep fallback") != NULL &&
                        !profile_has_mutator(profile);
            free(profile);
            if (!valid) {
                FAIL("direct profiles must expose coverage plus source fallback and omit mutators");
            }
        }
    }
    PASS();
}

TEST(agent_profiles_tiers_encode_distinct_evidence_budgets) {
    char *scout = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CLAUDE, LSM_GRAPH_TIER_SCOUT,
                                           LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *verify = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CLAUDE, LSM_GRAPH_TIER_VERIFY,
                                            LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *audit = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CLAUDE, LSM_GRAPH_TIER_AUDIT,
                                           LSM_GRAPH_ACCESS_DIRECT, NULL);
    int valid = scout && verify && audit && strstr(scout, "3-4 narrow graph calls") &&
                strstr(scout, "positive, provisional") && strstr(scout, "all/none claims") &&
                !strstr(scout, "mcp__logan-spine-mcp__query_graph") &&
                !strstr(scout, "mcp__logan-spine-mcp__detect_changes") &&
                strstr(verify, "default tier") && strstr(verify, "task-directed evidence") &&
                strstr(verify, "scope coverage before negative claims") &&
                strstr(verify, "mcp__logan-spine-mcp__query_graph") &&
                strstr(verify, "mcp__logan-spine-mcp__detect_changes") &&
                strstr(audit, "bounded scope") && strstr(audit, "current graph generation") &&
                strstr(audit, "complete relevant pagination") && strstr(audit, "scope coverage") &&
                strstr(audit, "source fallback") &&
                strstr(audit, "mcp__logan-spine-mcp__query_graph") &&
                strstr(audit, "mcp__logan-spine-mcp__detect_changes");
    free(scout);
    free(verify);
    free(audit);
    ASSERT_TRUE(valid);
    PASS();
}

TEST(agent_profiles_handoff_requires_parent_evidence_without_child_mcp) {
    for (int dialect = 0; dialect < (int)LSM_GRAPH_DIALECT_COUNT; dialect++) {
        for (int tier = 0; tier < (int)LSM_GRAPH_TIER_COUNT; tier++) {
            char *profile =
                lsm_render_graph_profile((lsm_graph_profile_dialect_t)dialect,
                                         (lsm_graph_tier_t)tier, LSM_GRAPH_ACCESS_HANDOFF, NULL);
            if (!profile) {
                FAIL("every dialect must be able to render a parent-handoff profile");
            }
            int valid = strstr(profile, "parent agent must supply") &&
                        strstr(profile, "coverage evidence") &&
                        strstr(profile, "must not call or claim access to MCP") &&
                        !strstr(profile, "mcpServers") &&
                        !strstr(profile, "mcp__logan-spine-mcp__") &&
                        !strstr(profile, "mcp_logan-spine-mcp_") &&
                        !strstr(profile, "@logan-spine-mcp/") &&
                        !strstr(profile, "logan-spine-mcp/");
            free(profile);
            if (!valid) {
                FAIL("handoff profiles must require parent coverage and expose no child MCP");
            }
        }
    }
    PASS();
}

TEST(agent_profiles_handoff_only_dialects_fail_closed_for_direct_access) {
    for (size_t i = 0U; i < sizeof(handoff_only_dialects) / sizeof(handoff_only_dialects[0]); i++) {
        char *profile = lsm_render_graph_profile(handoff_only_dialects[i], LSM_GRAPH_TIER_VERIFY,
                                                 LSM_GRAPH_ACCESS_DIRECT, "/opt/lsm");
        ASSERT_NULL(profile);
    }
    PASS();
}

TEST(agent_profiles_server_level_dialects_hard_enforce_read_only_tools) {
    char *junie_scout = lsm_render_graph_profile(LSM_GRAPH_DIALECT_JUNIE, LSM_GRAPH_TIER_SCOUT,
                                                 LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *junie = lsm_render_graph_profile(LSM_GRAPH_DIALECT_JUNIE, LSM_GRAPH_TIER_VERIFY,
                                           LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *qoder = lsm_render_graph_profile(LSM_GRAPH_DIALECT_QODER, LSM_GRAPH_TIER_VERIFY,
                                           LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *factory = lsm_render_graph_profile(LSM_GRAPH_DIALECT_FACTORY, LSM_GRAPH_TIER_VERIFY,
                                             LSM_GRAPH_ACCESS_DIRECT, NULL);
    ASSERT_NOT_NULL(junie_scout);
    ASSERT_NOT_NULL(junie);
    ASSERT_NOT_NULL(qoder);
    ASSERT_NOT_NULL(factory);
    ASSERT(strstr(junie_scout, "mcpServers: [\"logan-spine-scout\"]") != NULL);
    ASSERT(strstr(junie, "mcpServers: [\"logan-spine-analysis\"]") != NULL);
    ASSERT(strstr(junie, "hard-enforces the analysis tool profile") != NULL);
    ASSERT(strstr(qoder, "mcp__logan-spine-mcp__check_index_coverage") != NULL);
    ASSERT(strstr(factory, "mcp__logan-spine-mcp__check_index_coverage") != NULL);
    ASSERT(strstr(qoder, "mcpServers:") != NULL);
    ASSERT(strstr(qoder, "logan-spine-mcp") != NULL);
    ASSERT(strstr(factory, "mcpServers") == NULL);
    ASSERT(strstr(junie, "instruction-enforced") == NULL);
    ASSERT(strstr(qoder, "instruction-enforced") == NULL);
    ASSERT(strstr(factory, "instruction-enforced") == NULL);
    ASSERT(!profile_has_mutator(junie));
    ASSERT(!profile_has_mutator(qoder));
    ASSERT(!profile_has_mutator(factory));
    free(junie_scout);
    free(junie);
    free(qoder);
    free(factory);
    PASS();
}

TEST(agent_profiles_kiro_is_valid_json_and_escapes_binary_path) {
    const char *binary = "/opt/lsm path/\"quoted\"";
    char *profile = lsm_render_graph_profile(LSM_GRAPH_DIALECT_KIRO, LSM_GRAPH_TIER_AUDIT,
                                             LSM_GRAPH_ACCESS_DIRECT, binary);
    ASSERT_NOT_NULL(profile);
    yyjson_doc *doc = yyjson_read(profile, strlen(profile), 0);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    yyjson_val *servers = root ? yyjson_obj_get(root, "mcpServers") : NULL;
    yyjson_val *server = servers ? yyjson_obj_get(servers, "logan-spine-mcp") : NULL;
    yyjson_val *command = server ? yyjson_obj_get(server, "command") : NULL;
    yyjson_val *args = server ? yyjson_obj_get(server, "args") : NULL;
    yyjson_val *profile_flag = args && yyjson_is_arr(args) ? yyjson_arr_get(args, 0U) : NULL;
    yyjson_val *profile_name = args && yyjson_is_arr(args) ? yyjson_arr_get(args, 1U) : NULL;
    yyjson_val *tools = root ? yyjson_obj_get(root, "tools") : NULL;
    int valid = root && yyjson_is_obj(root) && command && yyjson_is_str(command) &&
                strcmp(yyjson_get_str(command), binary) == 0 && args && yyjson_is_arr(args) &&
                yyjson_arr_size(args) == 2U && profile_flag && yyjson_is_str(profile_flag) &&
                strcmp(yyjson_get_str(profile_flag), "--tool-profile") == 0 && profile_name &&
                yyjson_is_str(profile_name) &&
                strcmp(yyjson_get_str(profile_name), "analysis") == 0 && tools &&
                yyjson_is_arr(tools) &&
                strstr(profile, "@logan-spine-mcp/check_index_coverage") != NULL;
    yyjson_doc_free(doc);
    free(profile);
    ASSERT_TRUE(valid);
    PASS();
}

TEST(agent_profiles_codex_declares_transport_and_escapes_binary_path) {
    const char *binary = "C:\\lsm bin\\logan-spine-mcp.exe";
    char *scout = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CODEX, LSM_GRAPH_TIER_SCOUT,
                                           LSM_GRAPH_ACCESS_DIRECT, binary);
    char *verify = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CODEX, LSM_GRAPH_TIER_VERIFY,
                                            LSM_GRAPH_ACCESS_DIRECT, binary);
    ASSERT_NOT_NULL(scout);
    ASSERT_NOT_NULL(verify);
    int valid = strstr(scout, "[mcp_servers.logan-spine-mcp]\n"
                              "command = \"C:\\\\lsm bin\\\\logan-spine-mcp.exe\"\n"
                              "args = [\"--tool-profile=scout\"]\n"
                              "enabled_tools = [") != NULL &&
                strstr(verify, "command = \"C:\\\\lsm bin\\\\logan-spine-mcp.exe\"\n"
                               "args = [\"--tool-profile=analysis\"]\n") != NULL;
    free(scout);
    free(verify);
    ASSERT_TRUE(valid);
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_CODEX, LSM_GRAPH_TIER_VERIFY,
                                         LSM_GRAPH_ACCESS_DIRECT, NULL));
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_CODEX, LSM_GRAPH_TIER_VERIFY,
                                         LSM_GRAPH_ACCESS_DIRECT, ""));
    char *handoff = lsm_render_graph_profile(LSM_GRAPH_DIALECT_CODEX, LSM_GRAPH_TIER_VERIFY,
                                             LSM_GRAPH_ACCESS_HANDOFF, NULL);
    ASSERT_NOT_NULL(handoff);
    ASSERT_TRUE(strstr(handoff, "[mcp_servers.") == NULL);
    free(handoff);
    char *rc1 = lsm_render_graph_profile_codex_rc1(LSM_GRAPH_TIER_VERIFY);
    ASSERT_NOT_NULL(rc1);
    ASSERT_TRUE(strstr(rc1, "[mcp_servers.logan-spine-mcp]\nenabled_tools = [") != NULL);
    ASSERT_TRUE(strstr(rc1, "command = ") == NULL);
    free(rc1);
    PASS();
}

TEST(agent_profiles_vibe_uses_matching_prompt_identifier_and_contract) {
    for (int tier = 0; tier < (int)LSM_GRAPH_TIER_COUNT; tier++) {
        const char *slug = lsm_graph_tier_slug((lsm_graph_tier_t)tier);
        char *profile = lsm_render_graph_profile(LSM_GRAPH_DIALECT_VIBE, (lsm_graph_tier_t)tier,
                                                 LSM_GRAPH_ACCESS_DIRECT, NULL);
        char *prompt = lsm_render_graph_prompt((lsm_graph_tier_t)tier, LSM_GRAPH_ACCESS_DIRECT);
        int valid = profile && prompt && strstr(profile, slug) &&
                    strstr(profile, "system_prompt_id") && strstr(prompt, "check_index_coverage") &&
                    strstr(prompt, "source read/grep fallback");
        free(profile);
        free(prompt);
        if (!valid) {
            FAIL("Vibe profile and canonical prompt must share the tier slug and contract");
        }
    }
    PASS();
}

TEST(agent_profiles_render_deterministically_and_reject_invalid_inputs) {
    char *first = lsm_render_graph_profile(LSM_GRAPH_DIALECT_QWEN, LSM_GRAPH_TIER_VERIFY,
                                           LSM_GRAPH_ACCESS_DIRECT, NULL);
    char *second = lsm_render_graph_profile(LSM_GRAPH_DIALECT_QWEN, LSM_GRAPH_TIER_VERIFY,
                                            LSM_GRAPH_ACCESS_DIRECT, NULL);
    ASSERT_NOT_NULL(first);
    ASSERT_NOT_NULL(second);
    ASSERT_STR_EQ(first, second);
    free(first);
    free(second);
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_COUNT, LSM_GRAPH_TIER_VERIFY,
                                         LSM_GRAPH_ACCESS_DIRECT, NULL));
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_CLAUDE, LSM_GRAPH_TIER_COUNT,
                                         LSM_GRAPH_ACCESS_DIRECT, NULL));
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_CLAUDE, LSM_GRAPH_TIER_VERIFY,
                                         LSM_GRAPH_ACCESS_COUNT, NULL));
    ASSERT_NULL(lsm_render_graph_profile(LSM_GRAPH_DIALECT_KIRO, LSM_GRAPH_TIER_VERIFY,
                                         LSM_GRAPH_ACCESS_DIRECT, NULL));
    ASSERT_NULL(lsm_render_graph_prompt(LSM_GRAPH_TIER_COUNT, LSM_GRAPH_ACCESS_DIRECT));
    ASSERT_NULL(lsm_render_graph_prompt(LSM_GRAPH_TIER_VERIFY, LSM_GRAPH_ACCESS_COUNT));
    PASS();
}

SUITE(agent_profiles) {
    RUN_TEST(agent_profiles_stable_tier_identity);
    RUN_TEST(agent_profiles_direct_dialects_are_coverage_aware_and_read_only);
    RUN_TEST(agent_profiles_tiers_encode_distinct_evidence_budgets);
    RUN_TEST(agent_profiles_handoff_requires_parent_evidence_without_child_mcp);
    RUN_TEST(agent_profiles_handoff_only_dialects_fail_closed_for_direct_access);
    RUN_TEST(agent_profiles_server_level_dialects_hard_enforce_read_only_tools);
    RUN_TEST(agent_profiles_kiro_is_valid_json_and_escapes_binary_path);
    RUN_TEST(agent_profiles_codex_declares_transport_and_escapes_binary_path);
    RUN_TEST(agent_profiles_vibe_uses_matching_prompt_identifier_and_contract);
    RUN_TEST(agent_profiles_render_deterministically_and_reject_invalid_inputs);
}
