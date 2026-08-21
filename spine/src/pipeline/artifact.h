/*
 * artifact.h — Persistent artifact export/import for team sharing.
 *
 * Exports the SQLite knowledge graph as a zstd-compressed artifact
 * to .logan-spine/graph.db.zst in the repository. Teammates
 * can import the artifact to bootstrap their local index instead
 * of running a full pipeline from scratch.
 */
#ifndef LSM_ARTIFACT_H
#define LSM_ARTIFACT_H

#include <stdbool.h>

/* Schema version — increment when DB schema changes (new tables/indexes).
 * Import refuses artifacts with schema_version > current.
 * v2: edges uniqueness widened to (source_id, target_id, type,
 *     local_name_gen) so sibling named imports coexist (#768) — old
 *     binaries cannot upsert against the widened constraint. */
#define LSM_ARTIFACT_SCHEMA_VERSION 2

#define LSM_ARTIFACT_FILENAME "graph.db.zst"
#define LSM_ARTIFACT_META "artifact.json"
#define LSM_ARTIFACT_DIR ".logan-spine"

/* Export quality levels */
enum {
    LSM_ARTIFACT_FAST = 0, /* zstd -3, no index stripping (watcher path) */
    LSM_ARTIFACT_BEST = 1, /* zstd -9 + drop indexes + VACUUM INTO (explicit index) */
};

/* Export DB to .logan-spine/graph.db.zst artifact.
 * quality: LSM_ARTIFACT_FAST or LSM_ARTIFACT_BEST.
 * Creates .logan-spine/ dir, .gitattributes, and artifact.json.
 * Returns 0 on success, -1 on error. */
int lsm_artifact_export(const char *db_path, const char *repo_path, const char *project_name,
                        int quality);

/* Get details for the most recent export failure on this thread.
 * Returns NULL if no export error is recorded. */
const char *lsm_artifact_export_last_error(void);

/* Import artifact from .logan-spine/graph.db.zst to cache_db_path.
 * Decompresses, runs integrity check, recreates indexes.
 * Returns 0 on success, -1 on error. */
int lsm_artifact_import(const char *repo_path, const char *cache_db_path);

/* Check if a compatible artifact exists in repo_path/.logan-spine/.
 * Returns true only if both graph.db.zst and artifact.json exist
 * and schema_version is compatible. */
bool lsm_artifact_exists(const char *repo_path);

/* Get the git commit hash from artifact metadata. Caller must free().
 * Returns NULL if artifact doesn't exist or has no commit field. */
char *lsm_artifact_commit(const char *repo_path);

/* Whether repo_path is safe to interpolate into a double-quoted `git -C "…"` shell
 * command (as artifact.c does via lsm_popen). Rejects quote / backslash / shell
 * substitution metacharacters (lsm_validate_shell_arg); on Windows also rejects the
 * cmd.exe expansion metacharacters % ! ^. Spaces ARE allowed — double quotes handle
 * them on both POSIX sh and cmd.exe (single quotes, which cmd.exe does not honor,
 * were the pre-existing bug). Exposed so the shell-safety contract is unit-tested. */
bool lsm_artifact_repo_path_is_shell_safe(const char *repo_path);

#endif /* LSM_ARTIFACT_H */
