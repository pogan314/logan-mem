/*
 * dump_verify.c — Post-dump plausibility gate (#334).
 */
#include "foundation/dump_verify.h"
#include "foundation/constants.h"
#include "foundation/log.h"
#include "foundation/platform.h"

#include <stdlib.h>
#include <string.h>

bool lsm_dump_verify_is_degraded(int committed_nodes, int persisted_nodes, double ratio,
                                 int min_floor) {
    if (ratio <= 0.0) {
        return false;
    }
    if (committed_nodes < 0) {
        return false;
    }
    if (committed_nodes <= min_floor) {
        return false;
    }
    if (persisted_nodes < 0) {
        return true;
    }
    return (double)persisted_nodes < (double)committed_nodes * ratio;
}

double lsm_dump_verify_min_ratio(void) {
    char buf[LSM_SZ_32];
    if (lsm_safe_getenv("LSM_DUMP_VERIFY_MIN_RATIO", buf, sizeof(buf), NULL) != NULL) {
        char *end = NULL;
        double r = strtod(buf, &end);
        if (end != buf && r >= 0.0 && r <= 1.0) {
            return r;
        }
        lsm_log_warn("dump_verify.env.invalid", "value", buf, "fallback", "0.5");
    }
    return LSM_DUMP_VERIFY_DEFAULT_RATIO;
}
