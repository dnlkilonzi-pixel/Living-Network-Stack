#include "semantic.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/* Record a violation if there is still room in the output array.
 * Always increments *total so callers can detect truncation. */
static void record_violation(semantic_violation_t     *out,
                              int                       max_count,
                              int                      *total,
                              uint32_t                  hop_seq,
                              const char               *node,
                              semantic_violation_type_t type,
                              float                     observed,
                              float                     allowed)
{
    if (*total < max_count) {
        semantic_violation_t *v = &out[*total];
        v->hop_seq  = hop_seq;
        v->type     = type;
        v->observed = observed;
        v->allowed  = allowed;
        strncpy(v->node, node ? node : "", sizeof(v->node) - 1);
        v->node[sizeof(v->node) - 1] = '\0';
    }
    (*total)++;
}

/* Compute an objective score for a proto_comparison_t entry.
 * Higher is always better. */
static float compute_score(const proto_comparison_t *p,
                            opt_objective_t           obj)
{
    if (p->hop_count == 0) return -1.0f;

    switch (obj) {
    case OBJECTIVE_MINIMIZE_LATENCY:
        /* Invert latency: lower latency → higher score */
        return (p->avg_latency_ms > 0.0f)
               ? 10000.0f / p->avg_latency_ms
               : 10000.0f;

    case OBJECTIVE_MINIMIZE_LOSS:
        /* Invert loss: lower loss → higher score */
        return 1.0f - p->avg_loss;

    case OBJECTIVE_MAXIMIZE_THROUGHPUT:
        return p->avg_bandwidth_mbps;

    case OBJECTIVE_NONE:
    default:
        /* Default: penalise violations, then prefer lower latency */
        return (p->violation_count == 0 ? 1000.0f : 0.0f)
               + (p->avg_latency_ms > 0.0f
                  ? 1000.0f / p->avg_latency_ms
                  : 1000.0f);
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int semantic_verify_session(const replay_log_t      *log,
                             const intent_semantics_t *semantics,
                             semantic_violation_t     *violations_out,
                             int                       max_count)
{
    int total = 0;
    int i;

    if (!log || !semantics) return 0;

    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];
        float                 lat, loss;
        int                   use_udp;

        if (ev->type != REPLAY_HOP) continue;

        lat     = ev->data.hop.metrics.latency_ms;
        loss    = ev->data.hop.metrics.packet_loss;
        use_udp = ev->data.hop.decision.use_udp;

        /* --- Latency constraint --- */
        if (semantics->max_latency_ms > 0.0f &&
            lat > semantics->max_latency_ms) {
            record_violation(violations_out, max_count, &total,
                             ev->seq, ev->node,
                             VIOLATION_LATENCY_EXCEEDED,
                             lat, semantics->max_latency_ms);
        }

        /* --- Loss constraint --- */
        if (semantics->max_loss > 0.0f &&
            loss > semantics->max_loss) {
            record_violation(violations_out, max_count, &total,
                             ev->seq, ev->node,
                             VIOLATION_LOSS_EXCEEDED,
                             loss, semantics->max_loss);
        }

        /* --- Delivery guarantee vs protocol choice ---
         * AT_LEAST_ONCE and EXACTLY_ONCE require reliable delivery.
         * UDP (fire-and-forget) breaks these guarantees. */
        if (semantics->guarantee >= GUARANTEE_AT_LEAST_ONCE && use_udp) {
            record_violation(violations_out, max_count, &total,
                             ev->seq, ev->node,
                             VIOLATION_GUARANTEE_BROKEN,
                             1.0f /* UDP chosen */,
                             0.0f /* TCP required */);
        }
    }

    return total;
}

int semantic_compare_protocols(const replay_log_t          *log,
                                const intent_semantics_t    *semantics,
                                semantic_comparison_result_t *result)
{
    int i;

    if (!log || !semantics || !result) return -1;
    if (log->count == 0) return -1;

    memset(result, 0, sizeof(*result));
    result->count      = 2;
    result->winner_idx = -1;

    strncpy(result->entries[0].proto_name, "UDP",
            sizeof(result->entries[0].proto_name) - 1);
    strncpy(result->entries[1].proto_name, "TCP",
            sizeof(result->entries[1].proto_name) - 1);

    /* Accumulate per-protocol sums */
    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];
        proto_comparison_t   *p;

        if (ev->type != REPLAY_HOP) continue;

        p = ev->data.hop.decision.use_udp
            ? &result->entries[0]   /* UDP */
            : &result->entries[1];  /* TCP */

        p->hop_count++;
        p->avg_latency_ms     += ev->data.hop.metrics.latency_ms;
        p->avg_loss           += ev->data.hop.metrics.packet_loss;
        p->avg_bandwidth_mbps += ev->data.hop.metrics.bandwidth_mbps;

        /* Count only violations at this exact hop inline */
        {
            int hop_viol = 0;
            if (semantics->max_latency_ms > 0.0f &&
                ev->data.hop.metrics.latency_ms > semantics->max_latency_ms) {
                hop_viol = 1;
            }
            if (semantics->max_loss > 0.0f &&
                ev->data.hop.metrics.packet_loss > semantics->max_loss) {
                hop_viol = 1;
            }
            if (semantics->guarantee >= GUARANTEE_AT_LEAST_ONCE &&
                ev->data.hop.decision.use_udp) {
                hop_viol = 1;
            }
            p->violation_count += (uint32_t)hop_viol;
        }
    }

    /* Compute averages */
    for (i = 0; i < result->count; i++) {
        proto_comparison_t *p = &result->entries[i];
        if (p->hop_count > 0) {
            p->avg_latency_ms     /= (float)p->hop_count;
            p->avg_loss           /= (float)p->hop_count;
            p->avg_bandwidth_mbps /= (float)p->hop_count;
        }
        p->objective_score = compute_score(p, semantics->objective);
    }

    /* Determine winner */
    if (result->entries[0].hop_count > 0 &&
        result->entries[1].hop_count > 0) {
        result->winner_idx =
            (result->entries[0].objective_score >=
             result->entries[1].objective_score) ? 0 : 1;
    } else if (result->entries[0].hop_count > 0) {
        result->winner_idx = 0;
    } else if (result->entries[1].hop_count > 0) {
        result->winner_idx = 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Pretty-print helpers
 * ------------------------------------------------------------------------- */

static const char *violation_type_str(semantic_violation_type_t t)
{
    switch (t) {
    case VIOLATION_LATENCY_EXCEEDED:  return "LATENCY_EXCEEDED";
    case VIOLATION_LOSS_EXCEEDED:     return "LOSS_EXCEEDED";
    case VIOLATION_GUARANTEE_BROKEN:  return "GUARANTEE_BROKEN";
    default:                          return "UNKNOWN";
    }
}

void semantic_print_violations(const semantic_violation_t *violations,
                                int count)
{
    int i;

    printf("\n");
    printf("============================================================\n");
    printf("  [SEMANTIC] Violation Report  (%d violation%s)\n",
           count, count == 1 ? "" : "s");
    printf("============================================================\n");

    if (count == 0) {
        printf("  No violations — all hops satisfied declared semantics.\n");
    } else {
        for (i = 0; i < count; i++) {
            const semantic_violation_t *v = &violations[i];
            printf("  [%03u] %-20s  %-22s  observed=%.3f  allowed=%.3f\n",
                   v->hop_seq,
                   v->node,
                   violation_type_str(v->type),
                   v->observed,
                   v->allowed);
        }
    }
    printf("============================================================\n\n");
}

void semantic_print_comparison(const semantic_comparison_result_t *result)
{
    int i;

    printf("\n");
    printf("============================================================\n");
    printf("  [SEMANTIC] Protocol Comparison\n");
    printf("============================================================\n");
    printf("  %-10s  %6s  %10s  %8s  %10s  %10s  %8s\n",
           "Protocol", "Hops", "AvgLat(ms)", "AvgLoss%",
           "AvgBW(Mbps)", "Violations", "Score");
    printf("  ----------------------------------------------------------\n");

    for (i = 0; i < result->count; i++) {
        const proto_comparison_t *p = &result->entries[i];
        const char *marker = (result->winner_idx == i) ? " <-- WINNER" : "";

        if (p->hop_count == 0) {
            printf("  %-10s  %6s  (no hops observed)\n",
                   p->proto_name, "-");
            continue;
        }

        printf("  %-10s  %6u  %10.1f  %8.1f  %10.1f  %10u  %8.2f%s\n",
               p->proto_name,
               p->hop_count,
               p->avg_latency_ms,
               p->avg_loss * 100.0f,
               p->avg_bandwidth_mbps,
               p->violation_count,
               p->objective_score,
               marker);
    }

    if (result->winner_idx < 0) {
        printf("  No winner determined (insufficient data or tie).\n");
    }
    printf("============================================================\n\n");
}
