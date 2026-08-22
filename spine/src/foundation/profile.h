/*
 * profile.h — Activatable fine-grained performance profiling.
 *
 * Enable via environment variable: LSM_PROFILE=1 (or any non-empty non-"0" value)
 * Init is called once at program startup (from main.c).
 *
 * When disabled (default), the LSM_PROF_* macros cost one load + branch,
 * effectively zero overhead.
 *
 * Output format (structured log lines, parseable):
 *   level=info msg=prof phase=<pass> sub=<subphase> ms=<N> us=<N> items=<N> rate_per_s=<N>
 *
 * Grep for `msg=prof` to get a full profile report.
 */
#ifndef LSM_PROFILE_H
#define LSM_PROFILE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

/* Runtime-active flag. Set once by lsm_profile_init() from LSM_PROFILE env. */
extern bool lsm_profile_active;

/* ── Scaling probe — single-run superlinearity detector ──────────────
 *
 * Per-pass elapsed time (pass.timing) tells you a pass is slow. It cannot tell
 * you WHY: big corpus, or O(n^2)? Establishing that for #1669 took an 11-corpus
 * A/B across two release binaries, where parallel_resolve turned out to be 87%
 * of a Java index.
 *
 * This makes the same question answerable from ONE run. A pass processing
 * `total` items records cumulative elapsed at 1/8, 1/4, 1/2 and 1 of them, then
 * fits the exponent k in T ~ n^k:
 *
 *     k ~ 1.0  linear      — cost per item is flat
 *     k ~ 1.5  superlinear — e.g. a candidate set that grows with the corpus
 *     k ~ 2.0  quadratic   — every item compared against every other
 *
 * A pass whose k reaches LSM_SCALE_WARN_K logs `scaling.superlinear` at WARN in
 * SHIPPED builds with no flag — that is the point: the next accidental O(n^2)
 * should announce itself in an ordinary user's log rather than needing a
 * two-binary bench to find. The full checkpoint curve is emitted only under
 * LSM_PROFILE / --profile, so normal runs stay quiet.
 *
 * KNOW WHAT THIS DOES NOT CATCH. It measures growth WITHIN one run, so it sees
 * a pass whose per-item cost rises as the run proceeds. It does NOT see the
 * other quadratic shape — per-item work proportional to the WHOLE corpus, where
 * every item costs the same large amount and the in-run curve is therefore
 * flat. #1669 was exactly that shape: cross-file LSP rebuilt a registry from a
 * corpus-scaled def set for every file, measured k=1.86 across corpus SIZES
 * while this probe reported a harmless-looking 1.26 within a single run.
 *
 * For that shape the comparable number is `us_per_item`, which is emitted on
 * every line: it is a property of the corpus, not the machine, so the same
 * pass measured on a small and a large repo tells you immediately whether
 * per-item cost tracks repo size. Two runs, no rebuild, no A/B of binaries.
 *
 * Cost when nothing is wrong: four clock reads per pass, plus one relaxed
 * atomic load and an integer compare per tick. Tick from a site that is already
 * throttled (a progress-log point), never from the innermost loop. */

/* Checkpoints at total/8, total/4, total/2, total. */
enum { LSM_SCALE_CHECKPOINTS = 4 };

/* k at or above this is reported. 1.35 sits clear of measured linear passes
 * (parallel_extract runs ~1.0-1.15 with normal scheduling jitter) while still
 * catching the mild-but-real 1.4-1.5 cases early. */
#define LSM_SCALE_WARN_K 1.35

typedef struct {
    const char *phase;
    long total;
    struct timespec start;
    _Atomic int next_cp;
    long cp_us[LSM_SCALE_CHECKPOINTS];
    long cp_items[LSM_SCALE_CHECKPOINTS];
} lsm_scale_probe_t;

/* Arm a probe for a pass that will process `total` items. Safe with total <= 0
 * (the probe then no-ops). */
void lsm_scale_begin(lsm_scale_probe_t *probe, const char *phase, long total);

/* Record progress. Thread-safe and cheap: only the thread that crosses a
 * checkpoint boundary does any work, and only LSM_SCALE_CHECKPOINTS times. */
void lsm_scale_tick(lsm_scale_probe_t *probe, long done);

/* Fit the exponent and report. Emits `scaling.superlinear` at WARN when
 * k >= LSM_SCALE_WARN_K (always), and `scaling` at INFO with the full curve
 * when profiling is active. */
void lsm_scale_end(lsm_scale_probe_t *probe);

/* The fit itself, as a pure function of two (items, microseconds) points:
 * k such that T ~ n^k. Separated from the probe so the arithmetic is testable
 * without depending on wall-clock timing — a test that had to *produce* a
 * quadratic workload to check this would be measuring the scheduler.
 * Returns -1.0 for degenerate input (non-positive, or no growth in n). */
double lsm_scale_fit_k(long first_n, long first_us, long last_n, long last_us);

/* Initialize profiling — reads LSM_PROFILE env var. Call once at startup. */
void lsm_profile_init(void);

/* Force-enable profiling at runtime (used by CLI --profile flag). */
void lsm_profile_enable(void);

/* Get a high-resolution timestamp. */
void lsm_profile_now(struct timespec *ts);

/* Log elapsed time since `start` for the given phase/subphase.
 * `items` = optional count to compute rate (pass 0 to skip). */
void lsm_profile_log_elapsed(const char *phase, const char *sub, const struct timespec *start,
                             long items);

/* Zero-overhead macros: a single runtime check gates everything. */
#define LSM_PROF_START(var) \
    struct timespec var;    \
    if (lsm_profile_active) \
    lsm_profile_now(&(var))

#define LSM_PROF_END(phase, sub, start_var)                           \
    do {                                                              \
        if (lsm_profile_active) {                                     \
            lsm_profile_log_elapsed((phase), (sub), &(start_var), 0); \
        }                                                             \
    } while (0)

#define LSM_PROF_END_N(phase, sub, start_var, items)                              \
    do {                                                                          \
        if (lsm_profile_active) {                                                 \
            lsm_profile_log_elapsed((phase), (sub), &(start_var), (long)(items)); \
        }                                                                         \
    } while (0)

#endif /* LSM_PROFILE_H */
