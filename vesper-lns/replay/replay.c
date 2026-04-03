#include "replay.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

static const char *event_type_str(replay_event_type_t t)
{
    switch (t) {
    case REPLAY_HOP:       return "HOP";
    case REPLAY_FORWARD:   return "FORWARD";
    case REPLAY_PROPAGATE: return "PROPAGATE";
    default:               return "UNKNOWN";
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void replay_init(replay_log_t *log, unsigned int seed)
{
    memset(log, 0, sizeof(*log));
    log->seed   = seed;
    log->active = 0;
}

void replay_start(replay_log_t *log)
{
    if (log->active) return;
    log->count  = 0;
    log->active = 1;
    printf("[REPLAY] Recording started (seed=%u)\n", log->seed);
}

void replay_stop(replay_log_t *log)
{
    if (!log->active) return;
    log->active = 0;
    printf("[REPLAY] Recording stopped (%d events captured)\n", log->count);
}

void replay_record_hop(replay_log_t *log, const char *node,
                       int hop_num, int total_hops,
                       intent_t intent, net_metrics_t metrics,
                       decision_t decision)
{
    replay_event_t *ev;

    if (!log || !log->active || log->count >= REPLAY_MAX_EVENTS) return;

    ev = &log->events[log->count++];
    memset(ev, 0, sizeof(*ev));

    ev->type         = REPLAY_HOP;
    ev->seq          = (uint32_t)(log->count - 1);
    if (node) strncpy(ev->node, node, sizeof(ev->node) - 1);

    ev->data.hop.hop_num    = hop_num;
    ev->data.hop.total_hops = total_hops;
    ev->data.hop.intent     = intent;
    ev->data.hop.metrics    = metrics;
    ev->data.hop.decision   = decision;
}

void replay_record_forward(replay_log_t *log, const char *src,
                           const char *dst, int success)
{
    replay_event_t *ev;

    if (!log || !log->active || log->count >= REPLAY_MAX_EVENTS) return;

    ev = &log->events[log->count++];
    memset(ev, 0, sizeof(*ev));

    ev->type = REPLAY_FORWARD;
    ev->seq  = (uint32_t)(log->count - 1);
    if (src) strncpy(ev->node, src, sizeof(ev->node) - 1);
    if (dst) strncpy(ev->data.forward.dst, dst,
                     sizeof(ev->data.forward.dst) - 1);
    ev->data.forward.success = success;
}

void replay_record_propagate(replay_log_t *log, const char *from,
                             const char *to,
                             decision_t decision,
                             net_metrics_t observed)
{
    replay_event_t *ev;

    if (!log || !log->active || log->count >= REPLAY_MAX_EVENTS) return;

    ev = &log->events[log->count++];
    memset(ev, 0, sizeof(*ev));

    ev->type = REPLAY_PROPAGATE;
    ev->seq  = (uint32_t)(log->count - 1);
    if (from) strncpy(ev->node, from, sizeof(ev->node) - 1);
    if (from) strncpy(ev->data.propagate.from, from,
                      sizeof(ev->data.propagate.from) - 1);
    if (to)   strncpy(ev->data.propagate.to, to,
                      sizeof(ev->data.propagate.to) - 1);
    ev->data.propagate.decision = decision;
    ev->data.propagate.observed = observed;
}

void replay_dump(const replay_log_t *log, FILE *fp)
{
    int i;

    fprintf(fp, "\n[REPLAY] ===== Trace Dump  "
            "(%d events, seed=%u) =====\n",
            log->count, log->seed);

    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];

        fprintf(fp, "[REPLAY] [%03u] %-10s  node=%-20s  ",
                ev->seq, event_type_str(ev->type), ev->node);

        switch (ev->type) {
        case REPLAY_HOP:
            fprintf(fp,
                    "hop=%d/%d  intent_flags=0x%02x  "
                    "latency=%.1fms  loss=%.1f%%  bw=%.1fMbps  "
                    "decision=%s  enc=%s  retries=%d\n",
                    ev->data.hop.hop_num, ev->data.hop.total_hops,
                    ev->data.hop.intent.flags,
                    ev->data.hop.metrics.latency_ms,
                    ev->data.hop.metrics.packet_loss * 100.0f,
                    ev->data.hop.metrics.bandwidth_mbps,
                    ev->data.hop.decision.use_udp ? "UDP" : "TCP",
                    ev->data.hop.decision.use_encryption ? "ON" : "OFF",
                    ev->data.hop.decision.retry_count);
            break;

        case REPLAY_FORWARD:
            fprintf(fp, "dst=%-20s  result=%s\n",
                    ev->data.forward.dst,
                    ev->data.forward.success ? "OK" : "FAIL");
            break;

        case REPLAY_PROPAGATE:
            fprintf(fp, "from=%-16s  to=%-16s  "
                    "proto=%s  loss=%.1f%%  latency=%.1fms\n",
                    ev->data.propagate.from, ev->data.propagate.to,
                    ev->data.propagate.decision.use_udp ? "UDP" : "TCP",
                    ev->data.propagate.observed.packet_loss * 100.0f,
                    ev->data.propagate.observed.latency_ms);
            break;

        default:
            fprintf(fp, "\n");
            break;
        }
    }

    fprintf(fp, "[REPLAY] ===== End of trace =====\n\n");
}

void replay_run(const replay_log_t *log)
{
    int mismatches = 0;
    int hops_run   = 0;
    int i;

    printf("\n[REPLAY] ===== Deterministic Replay  "
           "(seed=%u) =====\n", log->seed);

    /* Restore the same random seed so simulate_metrics() – if called –
     * produces identical results.  The hop events store explicit metrics,
     * so their re-execution is seed-independent; the seed guarantee matters
     * for any lns_send() calls that use simulate_metrics(). */
    srand(log->seed);

    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];

        if (ev->type != REPLAY_HOP) continue;

        {
            decision_t    replayed;
            int           hop_num    = ev->data.hop.hop_num;
            int           total_hops = ev->data.hop.total_hops;
            intent_t      intent     = ev->data.hop.intent;
            net_metrics_t metrics    = ev->data.hop.metrics;
            decision_t    recorded   = ev->data.hop.decision;

            /* Re-execute: same inputs → must yield same decision */
            replayed = resolve_intent(intent);
            mutate(&replayed, metrics);

            hops_run++;

            if (replayed.use_udp        == recorded.use_udp        &&
                replayed.use_encryption == recorded.use_encryption  &&
                replayed.retry_count    == recorded.retry_count) {

                printf("[REPLAY] [%03u] HOP %d/%d @ %-20s  "
                       "proto=%s  enc=%s  retries=%d  -> MATCH\n",
                       ev->seq, hop_num, total_hops, ev->node,
                       replayed.use_udp ? "UDP" : "TCP",
                       replayed.use_encryption ? "ON" : "OFF",
                       replayed.retry_count);
            } else {
                printf("[REPLAY] [%03u] HOP %d/%d @ %-20s  -> MISMATCH!\n",
                       ev->seq, hop_num, total_hops, ev->node);
                printf("[REPLAY]   recorded : proto=%s  enc=%s  retries=%d\n",
                       recorded.use_udp ? "UDP" : "TCP",
                       recorded.use_encryption ? "ON" : "OFF",
                       recorded.retry_count);
                printf("[REPLAY]   replayed : proto=%s  enc=%s  retries=%d\n",
                       replayed.use_udp ? "UDP" : "TCP",
                       replayed.use_encryption ? "ON" : "OFF",
                       replayed.retry_count);
                mismatches++;
            }
        }
    }

    printf("[REPLAY] ===== Replay complete: %d hops re-run, "
           "%d mismatches =====\n",
           hops_run, mismatches);

    if (mismatches == 0) {
        printf("[REPLAY] DETERMINISM VERIFIED: all decisions reproduced "
               "exactly.\n\n");
    } else {
        printf("[REPLAY] WARNING: %d determinism violations detected.\n\n",
               mismatches);
    }
}
