#ifndef SEMANTIC_H
#define SEMANTIC_H

/*
 * semantic/semantic.h — Semantic Protocol Reasoning Layer
 *
 * This layer adds three capabilities on top of the existing replay log:
 *
 *  1. Runtime verification   — Did the protocol chosen at each hop actually
 *                              satisfy the declared intent_semantics_t?
 *                              Each constraint (latency budget, loss budget,
 *                              delivery guarantee) is checked against the
 *                              metrics recorded in the replay log.
 *
 *  2. Violation report       — A semantic_violation_t describes exactly which
 *                              hop violated which constraint and by how much.
 *
 *  3. Protocol comparison    — Given the same deterministic replay, compare
 *                              all registered protocol choices (TCP/UDP/…)
 *                              under identical stress conditions and rank them
 *                              against the declared objective.
 *
 * All functions are pure readers of the replay log: they never modify any
 * runtime state and are safe to call after a session ends.
 */

#include "../intent_engine/intent.h"
#include "../replay/replay.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Violation types
 * ------------------------------------------------------------------------- */

typedef enum {
    VIOLATION_NONE              = 0,
    VIOLATION_LATENCY_EXCEEDED  = 1, /* observed latency_ms > max_latency_ms  */
    VIOLATION_LOSS_EXCEEDED     = 2, /* observed packet_loss > max_loss        */
    VIOLATION_GUARANTEE_BROKEN  = 3  /* UDP chosen when guarantee > BEST_EFFORT*/
} semantic_violation_type_t;

/* A single constraint violation recorded at one hop */
typedef struct {
    uint32_t                  hop_seq;       /* replay_event_t.seq of the hop */
    char                      node[32];      /* label of the offending node   */
    semantic_violation_type_t type;
    float                     observed;      /* what was actually seen        */
    float                     allowed;       /* the declared budget/limit     */
} semantic_violation_t;

/* Maximum violations returned in one call */
#define SEMANTIC_MAX_VIOLATIONS 128

/* ---------------------------------------------------------------------------
 * Per-protocol aggregate outcome (for protocol comparison)
 * ------------------------------------------------------------------------- */

typedef struct {
    char     proto_name[16];    /* "UDP" or "TCP"                             */
    uint32_t hop_count;         /* number of hops using this protocol         */
    float    avg_latency_ms;    /* mean observed latency                      */
    float    avg_loss;          /* mean observed packet loss                  */
    float    avg_bandwidth_mbps;/* mean observed bandwidth                    */
    uint32_t violation_count;   /* hops that violated at least one constraint */
    float    objective_score;   /* higher = better for the declared objective */
} proto_comparison_t;

/* Result of a semantic comparison across all protocols seen in the log */
typedef struct {
    proto_comparison_t entries[2]; /* [0]=UDP, [1]=TCP                       */
    int                count;      /* number of entries populated (0, 1, or 2)*/
    int                winner_idx; /* index into entries[] with best score;
                                    * -1 = tie or no data                    */
} semantic_comparison_result_t;

/* ---------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/*
 * Scan every HOP event in 'log' and check it against 'semantics'.
 * Violations are written into violations_out[] (up to max_count entries).
 * Returns the total number of violations found (may exceed max_count if the
 * array was too small — the return value tells the caller how many there were).
 */
int semantic_verify_session(const replay_log_t      *log,
                             const intent_semantics_t *semantics,
                             semantic_violation_t     *violations_out,
                             int                       max_count);

/*
 * Compare TCP vs UDP outcomes in 'log' against 'semantics'.
 * Fills result and returns 0 on success, -1 if the log is empty.
 */
int semantic_compare_protocols(const replay_log_t          *log,
                                const intent_semantics_t    *semantics,
                                semantic_comparison_result_t *result);

/* Print a human-readable violation report (all violations in the array) */
void semantic_print_violations(const semantic_violation_t *violations,
                                int count);

/* Print a side-by-side protocol comparison table */
void semantic_print_comparison(const semantic_comparison_result_t *result);

#endif /* SEMANTIC_H */
