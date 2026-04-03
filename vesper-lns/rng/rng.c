#include "rng.h"

/* ---------------------------------------------------------------------------
 * xorshift64 global state
 * Zero is an illegal state for xorshift64 – seed guards against it.
 * ------------------------------------------------------------------------- */

static uint64_t g_rng_state = 1u;

/* ---------------------------------------------------------------------------
 * xorshift64 core – period 2^64 - 1, passes BigCrush
 * ------------------------------------------------------------------------- */

static uint64_t xorshift64(void)
{
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 7;
    g_rng_state ^= g_rng_state << 17;
    return g_rng_state;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void rng_seed(uint64_t seed)
{
    g_rng_state = (seed == 0u) ? 1u : seed;
}

uint64_t rng_state_get(void)
{
    return g_rng_state;
}

void rng_state_set(uint64_t state)
{
    g_rng_state = (state == 0u) ? 1u : state;
}

float rng_float(void)
{
    /* Use the upper 24 bits of a 32-bit word for the mantissa */
    uint32_t bits = (uint32_t)(xorshift64() >> 32);
    return (float)(bits >> 8) * (1.0f / (float)(1 << 24));
}

int rng_int_range(int lo, int hi)
{
    uint32_t range;
    if (lo >= hi) return lo;
    range = (uint32_t)(hi - lo + 1);
    return lo + (int)((uint32_t)(xorshift64() >> 32) % range);
}
