/*
 * dump_verify.h — Post-dump plausibility gate (#334 design b).
 *
 * Compares committed in-memory node counts against persisted SQLite rows
 * after index_repository completes. Nodes-only gate (edges shrink legitimately
 * at dump when endpoints fail to resolve).
 */
#ifndef LSM_DUMP_VERIFY_H
#define LSM_DUMP_VERIFY_H

#include <stdbool.h>

/** Repos with at most this many committed nodes skip the ratio gate. */
enum { LSM_DUMP_VERIFY_MIN_FLOOR = 50 };

/** Default minimum persisted/committed ratio when env is unset. */
#define LSM_DUMP_VERIFY_DEFAULT_RATIO 0.5

/**
 * True when persisted_nodes is implausibly below committed_nodes.
 *
 * Returns false when ratio <= 0 (gate disabled), committed_nodes < 0 (no dump),
 * committed_nodes <= min_floor (sparse repo), or persisted >= committed * ratio.
 * Returns true when persisted_nodes < 0 (count error).
 */
bool lsm_dump_verify_is_degraded(int committed_nodes, int persisted_nodes, double ratio,
                                 int min_floor);

/** Read LSM_DUMP_VERIFY_MIN_RATIO (0..1); invalid/unset -> default 0.5. Set 0 to disable. */
double lsm_dump_verify_min_ratio(void);

#endif /* LSM_DUMP_VERIFY_H */
