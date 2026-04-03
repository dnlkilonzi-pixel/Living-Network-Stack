#include "synth.h"
#include "../rng/rng.h"
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Internal: predict effective metrics without any I/O
 *
 * Replicates the core arithmetic from stress_update() in a pure function so
 * the scoring inner loop produces no console output.
 * ------------------------------------------------------------------------- */

static net_metrics_t predict_metrics(const proto_config_t *cfg,
                                     const stress_state_t *stress,
                                     net_metrics_t         base,
                                     int                   bytes_per_pkt)
{
    net_metrics_t m = base;
    float queue, saturation, jitter, drain, loss_add;
    size_t bytes;
    float elapsed_ms = (float)(cfg->retransmit_delay_ms > 0
                                ? cfg->retransmit_delay_ms : 1);
    float bw_mbps = base.bandwidth_mbps > 0.0f ? base.bandwidth_mbps : 1.0f;

    bytes = (bytes_per_pkt > 0)
            ? (size_t)bytes_per_pkt
            : (size_t)cfg->packet_size;

    /* Scale send volume by congestion window */
    bytes *= (size_t)(cfg->congestion_window > 0 ? cfg->congestion_window : 1);

    /* Replicate stress_update() queue/drain arithmetic on a local copy */
    drain = (bw_mbps * 1.0e6f / 8.0f) * (elapsed_ms / 1000.0f);
    queue = stress->queue_bytes + (float)bytes - drain;
    if (queue < 0.0f) queue = 0.0f;
    if (queue > (float)STRESS_MAX_QUEUE_BYTES)
        queue = (float)STRESS_MAX_QUEUE_BYTES;

    saturation = queue / (float)STRESS_MAX_QUEUE_BYTES;

    /* Jitter EMA update (one step) */
    jitter = stress->jitter_ms * 0.9f + (saturation * 50.0f) * 0.1f;

    /* Apply effects */
    m.latency_ms += jitter;

    loss_add = saturation * 0.30f;
    m.packet_loss += loss_add;
    if (m.packet_loss > 1.0f) m.packet_loss = 1.0f;

    m.bandwidth_mbps = base.bandwidth_mbps * (1.0f - saturation * 0.8f);
    if (m.bandwidth_mbps < 0.1f) m.bandwidth_mbps = 0.1f;

    return m;
}

/* ---------------------------------------------------------------------------
 * Internal: score one candidate
 * ------------------------------------------------------------------------- */

static float score_candidate(const proto_config_t     *cfg,
                              const stress_state_t     *stress,
                              net_metrics_t             base,
                              const intent_semantics_t *sem,
                              int                       bytes_per_pkt,
                              int                      *violates_out)
{
    float score = SYNTH_SCORE_PERFECT;
    net_metrics_t eff;

    eff = predict_metrics(cfg, stress, base, bytes_per_pkt);

    *violates_out = 0;

    /* --- Latency constraint --- */
    if (sem->max_latency_ms > 0.0f && eff.latency_ms > sem->max_latency_ms) {
        float excess = eff.latency_ms - sem->max_latency_ms;
        score -= 300.0f * (excess / sem->max_latency_ms);
        *violates_out = 1;
    }

    /* --- Loss constraint --- */
    if (sem->max_loss > 0.0f && eff.packet_loss > sem->max_loss) {
        float excess = eff.packet_loss - sem->max_loss;
        score -= 400.0f * (excess / sem->max_loss);
        *violates_out = 1;
    }

    /* --- Objective bonus (direction-of-improvement reward) --- */
    switch (sem->objective) {
    case OBJECTIVE_MINIMIZE_LATENCY:
        if (eff.latency_ms > 0.0f)
            score += 200.0f / eff.latency_ms;
        break;
    case OBJECTIVE_MINIMIZE_LOSS:
        score += 200.0f * (1.0f - eff.packet_loss);
        break;
    case OBJECTIVE_MAXIMIZE_THROUGHPUT:
        score += eff.bandwidth_mbps * 2.0f;
        break;
    default:
        break;
    }

    /* --- Delivery-guarantee penalty for large retransmit delay --- */
    if (sem->guarantee >= GUARANTEE_AT_LEAST_ONCE) {
        /* Faster retransmission improves delivery reliability */
        score += 100.0f / (float)(cfg->retransmit_delay_ms > 0
                                  ? cfg->retransmit_delay_ms : 1);
    }

    return (score < 0.0f) ? 0.0f : score;
}

/* ---------------------------------------------------------------------------
 * Internal: produce a mutant of the base config
 *
 * Each field is perturbed by a random fraction in [−pct, +pct].
 * Values are clamped to physically meaningful ranges.
 * ------------------------------------------------------------------------- */

static proto_config_t mutate_config(const proto_config_t *base)
{
    proto_config_t c = *base;
    float delta;

    /* window_size: ±15%, [512, 1 MB] */
    delta = base->window_size * 0.15f * (rng_float() * 2.0f - 1.0f);
    c.window_size = (int)(base->window_size + delta);
    if (c.window_size < 512)     c.window_size = 512;
    if (c.window_size > 1048576) c.window_size = 1048576;

    /* retransmit_delay_ms: ±25%, [10 ms, 5 s] */
    delta = (float)base->retransmit_delay_ms * 0.25f * (rng_float() * 2.0f - 1.0f);
    c.retransmit_delay_ms = (int)((float)base->retransmit_delay_ms + delta);
    if (c.retransmit_delay_ms < 10)   c.retransmit_delay_ms = 10;
    if (c.retransmit_delay_ms > 5000) c.retransmit_delay_ms = 5000;

    /* packet_size: ±15%, [64, 65535] */
    delta = (float)base->packet_size * 0.15f * (rng_float() * 2.0f - 1.0f);
    c.packet_size = (int)((float)base->packet_size + delta);
    if (c.packet_size < 64)    c.packet_size = 64;
    if (c.packet_size > 65535) c.packet_size = 65535;

    /* congestion_window: ±30%, [1, 64] */
    delta = (float)base->congestion_window * 0.30f * (rng_float() * 2.0f - 1.0f);
    c.congestion_window = (int)((float)base->congestion_window + delta);
    if (c.congestion_window < 1)  c.congestion_window = 1;
    if (c.congestion_window > 64) c.congestion_window = 64;

    return c;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

synth_config_t synth_config_default(void)
{
    synth_config_t cfg;

    stress_init(&cfg.stress);

    cfg.base_metrics.latency_ms     = 50.0f;
    cfg.base_metrics.packet_loss    = 0.02f;
    cfg.base_metrics.bandwidth_mbps = 100.0f;

    cfg.semantics.guarantee      = GUARANTEE_BEST_EFFORT;
    cfg.semantics.max_latency_ms = 0.0f;
    cfg.semantics.max_loss       = 0.0f;
    cfg.semantics.objective      = OBJECTIVE_NONE;

    cfg.generations   = SYNTH_MAX_GENERATIONS;
    cfg.bytes_per_pkt = 0;

    return cfg;
}

int synth_run(const synth_config_t *cfg, synth_result_t *result_out)
{
    int               max_gens, g, p, viol, no_improve_streak;
    synth_candidate_t population[SYNTH_POPULATION_SIZE];
    synth_candidate_t gen_best;

    if (!cfg || !result_out) return -1;

    memset(result_out, 0, sizeof(*result_out));

    max_gens = (cfg->generations > 0 && cfg->generations <= SYNTH_MAX_GENERATIONS)
               ? cfg->generations : SYNTH_MAX_GENERATIONS;

    /* Seed the first generation from proto_config_default() */
    result_out->best.cfg     = proto_config_default();
    result_out->best.score   = score_candidate(&result_out->best.cfg,
                                               &cfg->stress,
                                               cfg->base_metrics,
                                               &cfg->semantics,
                                               cfg->bytes_per_pkt,
                                               &viol);
    result_out->best.violates = viol;
    result_out->best.gen      = 0;

    no_improve_streak = 0;

    for (g = 0; g < max_gens; g++) {

        /* Generate population by mutating current best */
        for (p = 0; p < SYNTH_POPULATION_SIZE; p++) {
            population[p].cfg = mutate_config(&result_out->best.cfg);
            population[p].score = score_candidate(&population[p].cfg,
                                                  &cfg->stress,
                                                  cfg->base_metrics,
                                                  &cfg->semantics,
                                                  cfg->bytes_per_pkt,
                                                  &viol);
            population[p].violates = viol;
            population[p].gen      = g + 1;
        }

        /* Find best in this generation */
        gen_best = population[0];
        for (p = 1; p < SYNTH_POPULATION_SIZE; p++) {
            if (population[p].score > gen_best.score)
                gen_best = population[p];
        }

        result_out->score_history[g] = gen_best.score;
        result_out->generations_run  = g + 1;

        /* Update overall best if improved */
        if (gen_best.score > result_out->best.score) {
            result_out->best  = gen_best;
            no_improve_streak = 0;
        } else {
            no_improve_streak++;
        }

        /* Early termination */
        if (no_improve_streak >= SYNTH_CONVERGENCE_WINDOW) {
            result_out->converged = 1;
            break;
        }
    }

    return 0;
}

void synth_print_result(const synth_result_t *result)
{
    int i;

    printf("\n");
    printf("============================================================\n");
    printf("  [SYNTH] Adversarial Protocol Synthesis Result\n");
    printf("============================================================\n");
    printf("  Generations run : %d%s\n",
           result->generations_run,
           result->converged ? " (converged early)" : "");
    printf("  Best score      : %.2f / %.2f\n",
           result->best.score, SYNTH_SCORE_PERFECT);
    printf("  Best found gen  : %d\n", result->best.gen);
    printf("  Violates        : %s\n",
           result->best.violates ? "YES (semantic constraint broken)" : "NO");
    printf("  -------------------------------------------------------\n");
    printf("  Winning proto_config_t:\n");
    proto_config_print(&result->best.cfg);
    printf("  -------------------------------------------------------\n");
    printf("  Score history (best per generation):\n  ");
    for (i = 0; i < result->generations_run; i++) {
        printf("%.0f", result->score_history[i]);
        if (i < result->generations_run - 1) printf(" → ");
        if ((i + 1) % 8 == 0 && i < result->generations_run - 1)
            printf("\n  ");
    }
    printf("\n");
    printf("============================================================\n\n");
}
