#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Centralized deterministic PRNG (xorshift64)
 *
 * All LNS modules use these functions instead of stdlib rand()/srand() so
 * that a single seed uniquely determines every random draw in the system,
 * enabling bit-identical replay from seed alone.
 * ------------------------------------------------------------------------- */

/* Seed the global PRNG.  seed=0 is treated as 1 (xorshift64 must never
 * hold a zero state). */
void     rng_seed(uint64_t seed);

/* Snapshot / restore the full generator state.
 * Used by replay_start() / replay_run() to bracket a recording. */
uint64_t rng_state_get(void);
void     rng_state_set(uint64_t state);

/* Uniform float in [0.0, 1.0) */
float    rng_float(void);

/* Uniform integer in [lo, hi] (inclusive).  Returns lo if lo >= hi. */
int      rng_int_range(int lo, int hi);

#endif /* RNG_H */
