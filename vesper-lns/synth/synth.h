#ifndef SYNTH_H
#define SYNTH_H

/*
 * synth/synth.h — Adversarial Protocol Synthesiser
 *
 * Evolves proto_config_t parameters across multiple generations to discover
 * the configuration that best satisfies declared intent_semantics_t
 * constraints under adversarial (worst-case) stress conditions.
 *
 * Algorithm: fixed-size population hillclimber.
 *   - Each generation produces SYNTH_POPULATION_SIZE mutants of the current
 *     best configuration.
 *   - Each mutant is scored by predicting its effective network metrics under
 *     the supplied stress model and checking them against intent_semantics_t.
 *   - The highest-scoring mutant becomes the parent of the next generation.
 *   - Early termination triggers when the score has not improved for
 *     SYNTH_CONVERGENCE_WINDOW consecutive generations.
 *
 * The scoring inner loop is pure (no I/O) so it is safe to call inside a
 * tight simulation.  All random draws go through the centralised rng module
 * so synthesis is deterministic and reproducible from seed alone.
 */

#include "../mutation_engine/mutation.h"
#include "../intent_engine/intent.h"

/* Tuneable constants */
#define SYNTH_POPULATION_SIZE     8   /* mutants per generation               */
#define SYNTH_MAX_GENERATIONS    32   /* upper bound on generations           */
#define SYNTH_CONVERGENCE_WINDOW  4   /* gens without improvement → converged */
#define SYNTH_SCORE_PERFECT   1000.0f /* ideal score ceiling                  */

/* ---------------------------------------------------------------------------
 * A single evaluated candidate
 * ------------------------------------------------------------------------- */

typedef struct {
    proto_config_t cfg;      /* protocol parameter set being evaluated  */
    float          score;    /* fitness score (higher = better)          */
    int            gen;      /* generation in which this was produced    */
    int            violates; /* 1 if any semantic constraint was broken  */
} synth_candidate_t;

/* ---------------------------------------------------------------------------
 * Input configuration for a synthesis run
 * ------------------------------------------------------------------------- */

typedef struct {
    stress_state_t     stress;        /* initial queue/jitter state to test against */
    net_metrics_t      base_metrics;  /* baseline network conditions              */
    intent_semantics_t semantics;     /* constraints the winner must satisfy      */
    int                generations;   /* 0 = use SYNTH_MAX_GENERATIONS            */
    int                bytes_per_pkt; /* simulated bytes per packet (0 = use cfg) */
} synth_config_t;

/* ---------------------------------------------------------------------------
 * Synthesis result
 * ------------------------------------------------------------------------- */

typedef struct {
    synth_candidate_t best;                              /* best overall candidate  */
    int               generations_run;                   /* actual generations done */
    float             score_history[SYNTH_MAX_GENERATIONS]; /* best score per gen  */
    int               converged;    /* 1 = stopped early due to convergence        */
} synth_result_t;

/* ---------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/*
 * Build a synth_config_t with safe defaults:
 *   stress     — empty queue, no jitter
 *   base       — 50 ms / 2% loss / 100 Mbps
 *   semantics  — BEST_EFFORT, no budgets, OBJECTIVE_NONE
 *   generations — SYNTH_MAX_GENERATIONS
 *   bytes_per_pkt — 0 (use candidate's packet_size)
 */
synth_config_t synth_config_default(void);

/*
 * Run the synthesis loop.
 * cfg must be fully initialised (use synth_config_default() + customise).
 * Fills result_out with the best candidate and per-generation history.
 * Returns 0 on success, -1 on bad arguments.
 */
int synth_run(const synth_config_t *cfg, synth_result_t *result_out);

/* Print a concise synthesis report to stdout */
void synth_print_result(const synth_result_t *result);

#endif /* SYNTH_H */
