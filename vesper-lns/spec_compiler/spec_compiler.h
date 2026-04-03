#ifndef SPEC_COMPILER_H
#define SPEC_COMPILER_H

/*
 * spec_compiler/spec_compiler.h — Formal Spec Compiler
 *
 * Translates an (intent_t, intent_semantics_t) pair into a compact,
 * runnable constraint program (spec_program_t).  The result can be:
 *
 *   1. Printed as symbolic verification rules  — spec_print_program()
 *   2. Executed once per hop at runtime        — spec_check()
 *   3. Used as inline halt-on-violation        — spec_assert_hop()
 *   4. Swept over an entire replay log         — spec_check_session()
 *
 * Compilation is deterministic and pure: the same (intent, semantics) pair
 * always produces the same program.  Check / assert are also pure reads
 * except for incrementing violation_count inside spec_constraint_t.
 *
 * Design notes
 * ============
 * Each spec_constraint_t is a flat struct with one or more typed check flags
 * plus a threshold value.  There are no function pointers — all checks are
 * dispatched through a fixed decision tree in spec_check() — which keeps the
 * ABI simple, the structs serialisable, and the code free of call-through
 * pointer issues.
 *
 * Compiled rules map from intent as follows:
 *
 *   INTENT_LOW_LATENCY        → latency_ms  < 50 ms (default budget)
 *   INTENT_HIGH_THROUGHPUT    → bandwidth_mbps > 10 Mbps
 *   INTENT_HIGH_SECURITY      → use_encryption == 1
 *   intent.ttl_ms > 0         → latency_ms < ttl_ms
 *   semantics.max_latency_ms  → latency_ms  < max_latency_ms
 *   semantics.max_loss        → packet_loss < max_loss
 *   semantics.guarantee ≥ AT_LEAST_ONCE → use_udp == 0  (TCP required)
 */

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../replay/replay.h"
#include <stdint.h>

/* Maximum number of constraints in one compiled program */
#define SPEC_MAX_CONSTRAINTS  16

/* ---------------------------------------------------------------------------
 * A single compiled constraint
 * ------------------------------------------------------------------------- */

typedef struct {
    char     name[48];          /* human-readable rule name                  */
    char     rule_text[96];     /* symbolic rule, e.g. "latency_ms < 50.00"  */
    float    threshold;         /* numeric threshold (meaning depends on type)*/
    int      check_latency;     /* 1 → threshold is a latency ceiling (ms)   */
    int      check_loss;        /* 1 → threshold is a loss ceiling [0,1]     */
    int      check_bandwidth;   /* 1 → threshold is a bandwidth floor (Mbps) */
    int      require_tcp;       /* 1 → use_udp must be 0                     */
    int      require_encrypt;   /* 1 → use_encryption must be 1              */
    uint32_t violation_count;   /* incremented each time this rule fails      */
} spec_constraint_t;

/* ---------------------------------------------------------------------------
 * A compiled spec program
 * ------------------------------------------------------------------------- */

typedef struct {
    spec_constraint_t  constraints[SPEC_MAX_CONSTRAINTS];
    int                count;             /* populated constraints           */
    intent_t           source_intent;     /* intent_t this was compiled from */
    intent_semantics_t source_semantics;  /* semantics this was compiled from*/
} spec_program_t;

/* ---------------------------------------------------------------------------
 * Outcome of a single spec_check() call
 * ------------------------------------------------------------------------- */

typedef struct {
    int passed;                        /* 1 = all constraints satisfied      */
    int failed_count;                  /* number of failing constraints       */
    int failed_idx[SPEC_MAX_CONSTRAINTS]; /* indices of failing constraints  */
} spec_check_result_t;

/* ---------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/*
 * Compile an (intent, semantics) pair into a runnable spec_program_t.
 * program_out is fully initialised (all violation counters zeroed).
 * Returns the number of constraints compiled (>= 0), or -1 on bad args.
 */
int spec_compile(intent_t           intent,
                 intent_semantics_t semantics,
                 spec_program_t    *program_out);

/*
 * Evaluate one hop's (metrics, decision) against every constraint in the
 * compiled program.  Increments violation_count on each failing constraint.
 * Returns the check result; never modifies metrics or decision.
 */
spec_check_result_t spec_check(spec_program_t   *program,
                                net_metrics_t     metrics,
                                const decision_t *decision);

/*
 * Inline assertion wrapper around spec_check().
 * Prints one line to stderr for each failing constraint.
 * Returns 1 if all constraints passed, 0 if any failed.
 */
int spec_assert_hop(spec_program_t   *program,
                    net_metrics_t     metrics,
                    const decision_t *decision,
                    const char       *node_label);

/*
 * Sweep the spec program over every HOP event in the replay log.
 * Calls spec_check() for each hop; violation_count fields accumulate.
 * Returns the total number of hops that violated at least one constraint.
 */
int spec_check_session(spec_program_t   *program,
                        const replay_log_t *log);

/* Print the compiled program as human-readable symbolic rules */
void spec_print_program(const spec_program_t *program);

/* Print the result of a single spec_check() call */
void spec_print_check_result(const spec_check_result_t *result,
                              const spec_program_t      *program);

/* Print a cumulative violation summary across all spec_check() calls */
void spec_print_violations(const spec_program_t *program);

#endif /* SPEC_COMPILER_H */
