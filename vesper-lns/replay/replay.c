#include "replay.h"
#include "../rng/rng.h"
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

/* Return the index for a node label, creating a new entry if needed.
 * Returns -1 if the node table is full. */
static int vc_node_index(replay_log_t *log, const char *label)
{
    int i;
    if (!label || label[0] == '\0') return -1;
    for (i = 0; i < log->vc_node_count; i++) {
        if (strncmp(log->vc_labels[i], label, 32) == 0) return i;
    }
    if (log->vc_node_count >= REPLAY_MAX_NODES) return -1;
    i = log->vc_node_count++;
    strncpy(log->vc_labels[i], label, 31);
    log->vc_labels[i][31] = '\0';
    return i;
}

/* Increment the logical clock for 'node' and snapshot the full vc[] into ev */
static void vc_tick(replay_log_t *log, replay_event_t *ev, const char *node)
{
    int idx = vc_node_index(log, node);
    int i;

    if (idx >= 0) {
        log->node_vc[idx]++;
    }
    /* Snapshot current state */
    for (i = 0; i < REPLAY_MAX_NODES; i++) {
        ev->vc[i] = log->node_vc[i];
    }
}

/* Scan backwards from ev->seq to find the most recent PROPAGATE event
 * whose .to label matches 'node'.  Returns that event's seq, or
 * REPLAY_NO_PARENT if not found. */
static uint32_t find_causal_parent(const replay_log_t *log,
                                   uint32_t current_seq,
                                   const char *node)
{
    int i;
    for (i = (int)current_seq - 1; i >= 0; i--) {
        const replay_event_t *ev = &log->events[i];
        if (ev->type == REPLAY_PROPAGATE &&
            strncmp(ev->data.propagate.to, node, 32) == 0) {
            return ev->seq;
        }
    }
    return REPLAY_NO_PARENT;
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
    log->count         = 0;
    log->active        = 1;
    log->vc_node_count = 0;
    memset(log->node_vc,   0, sizeof(log->node_vc));
    memset(log->vc_labels, 0, sizeof(log->vc_labels));
    /* Snapshot the centralized RNG state for bit-identical replay */
    log->rng_state = rng_state_get();
    printf("[REPLAY] Recording started (seed=%u, rng_state=0x%016llx)\n",
           log->seed, (unsigned long long)log->rng_state);
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

    ev->type = REPLAY_HOP;
    ev->seq  = (uint32_t)(log->count - 1);
    if (node) strncpy(ev->node, node, sizeof(ev->node) - 1);

    /* Causal model: find most recent PROPAGATE that reached this node */
    ev->causal_parent_seq = find_causal_parent(log, ev->seq, node);

    /* Vector clock: advance this node's logical time */
    vc_tick(log, ev, node);

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

    ev->type              = REPLAY_FORWARD;
    ev->seq               = (uint32_t)(log->count - 1);
    ev->causal_parent_seq = REPLAY_NO_PARENT;
    if (src) strncpy(ev->node, src, sizeof(ev->node) - 1);
    if (dst) strncpy(ev->data.forward.dst, dst,
                     sizeof(ev->data.forward.dst) - 1);
    ev->data.forward.success = success;

    vc_tick(log, ev, src);
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

    ev->type              = REPLAY_PROPAGATE;
    ev->seq               = (uint32_t)(log->count - 1);
    ev->causal_parent_seq = REPLAY_NO_PARENT;
    if (from) strncpy(ev->node, from, sizeof(ev->node) - 1);
    if (from) strncpy(ev->data.propagate.from, from,
                      sizeof(ev->data.propagate.from) - 1);
    if (to)   strncpy(ev->data.propagate.to, to,
                      sizeof(ev->data.propagate.to) - 1);
    ev->data.propagate.decision = decision;
    ev->data.propagate.observed = observed;

    vc_tick(log, ev, from);
}

/* ---------------------------------------------------------------------------
 * Causal query
 * ------------------------------------------------------------------------- */

const replay_event_t *replay_find_cause(const replay_log_t *log, uint32_t seq)
{
    const replay_event_t *ev;

    if (!log || (int)seq >= log->count) return NULL;

    ev = &log->events[seq];
    if (ev->causal_parent_seq == REPLAY_NO_PARENT) return NULL;
    if (ev->causal_parent_seq >= (uint32_t)log->count) return NULL;

    return &log->events[ev->causal_parent_seq];
}

/* ---------------------------------------------------------------------------
 * Human-readable dump
 * ------------------------------------------------------------------------- */

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
                    "decision=%s  enc=%s  retries=%d",
                    ev->data.hop.hop_num, ev->data.hop.total_hops,
                    ev->data.hop.intent.flags,
                    ev->data.hop.metrics.latency_ms,
                    ev->data.hop.metrics.packet_loss * 100.0f,
                    ev->data.hop.metrics.bandwidth_mbps,
                    ev->data.hop.decision.use_udp ? "UDP" : "TCP",
                    ev->data.hop.decision.use_encryption ? "ON" : "OFF",
                    ev->data.hop.decision.retry_count);
            if (ev->causal_parent_seq != REPLAY_NO_PARENT) {
                fprintf(fp, "  caused_by=%u", ev->causal_parent_seq);
            }
            fprintf(fp, "\n");
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

/* ---------------------------------------------------------------------------
 * NDJSON dump
 * ------------------------------------------------------------------------- */

static void write_vc_json(FILE *fp, const uint32_t *vc, int n)
{
    int i;
    fprintf(fp, "[");
    for (i = 0; i < n; i++) {
        if (i) fprintf(fp, ",");
        fprintf(fp, "%u", vc[i]);
    }
    fprintf(fp, "]");
}

int replay_dump_file(const replay_log_t *log, const char *path)
{
    FILE *fp;
    int   i;

    if (!log || !path) return -1;

    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[REPLAY] Failed to open log file: %s\n", path);
        return -1;
    }

    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];

        switch (ev->type) {
        case REPLAY_HOP:
            fprintf(fp,
                    "{\"seq\":%u,\"type\":\"HOP\",\"node\":\"%s\","
                    "\"hop\":%d,\"total\":%d,"
                    "\"intent_flags\":%u,"
                    "\"latency\":%.3f,\"loss\":%.6f,\"bw\":%.3f,"
                    "\"proto\":\"%s\",\"enc\":%s,\"retries\":%d,",
                    ev->seq, ev->node,
                    ev->data.hop.hop_num, ev->data.hop.total_hops,
                    ev->data.hop.intent.flags,
                    ev->data.hop.metrics.latency_ms,
                    ev->data.hop.metrics.packet_loss,
                    ev->data.hop.metrics.bandwidth_mbps,
                    ev->data.hop.decision.use_udp ? "UDP" : "TCP",
                    ev->data.hop.decision.use_encryption ? "true" : "false",
                    ev->data.hop.decision.retry_count);
            if (ev->causal_parent_seq == REPLAY_NO_PARENT) {
                fprintf(fp, "\"causal_parent\":null,");
            } else {
                fprintf(fp, "\"causal_parent\":%u,", ev->causal_parent_seq);
            }
            fprintf(fp, "\"vc\":");
            write_vc_json(fp, ev->vc, REPLAY_MAX_NODES);
            fprintf(fp, "}\n");
            break;

        case REPLAY_FORWARD:
            fprintf(fp,
                    "{\"seq\":%u,\"type\":\"FORWARD\",\"src\":\"%s\","
                    "\"dst\":\"%s\",\"success\":%s,\"vc\":",
                    ev->seq, ev->node,
                    ev->data.forward.dst,
                    ev->data.forward.success ? "true" : "false");
            write_vc_json(fp, ev->vc, REPLAY_MAX_NODES);
            fprintf(fp, "}\n");
            break;

        case REPLAY_PROPAGATE:
            fprintf(fp,
                    "{\"seq\":%u,\"type\":\"PROPAGATE\","
                    "\"from\":\"%s\",\"to\":\"%s\","
                    "\"proto\":\"%s\",\"loss\":%.6f,\"latency\":%.3f,"
                    "\"vc\":",
                    ev->seq,
                    ev->data.propagate.from, ev->data.propagate.to,
                    ev->data.propagate.decision.use_udp ? "UDP" : "TCP",
                    ev->data.propagate.observed.packet_loss,
                    ev->data.propagate.observed.latency_ms);
            write_vc_json(fp, ev->vc, REPLAY_MAX_NODES);
            fprintf(fp, "}\n");
            break;

        default:
            break;
        }
    }

    fclose(fp);
    printf("[REPLAY] Log written to: %s  (%d events)\n", path, log->count);
    return 0;
}

/* ---------------------------------------------------------------------------
 * Deterministic replay / verification
 * ------------------------------------------------------------------------- */

void replay_run(const replay_log_t *log)
{
    int mismatches = 0;
    int hops_run   = 0;
    int i;

    printf("\n[REPLAY] ===== Deterministic Replay  "
           "(seed=%u, rng_state=0x%016llx) =====\n",
           log->seed, (unsigned long long)log->rng_state);

    /* Restore the exact RNG state that was active at recording start.
     * This makes simulate_metrics() produce bit-identical draws on replay. */
    rng_state_set(log->rng_state);

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
                       "proto=%s  enc=%s  retries=%d  -> MATCH",
                       ev->seq, hop_num, total_hops, ev->node,
                       replayed.use_udp ? "UDP" : "TCP",
                       replayed.use_encryption ? "ON" : "OFF",
                       replayed.retry_count);
                if (ev->causal_parent_seq != REPLAY_NO_PARENT) {
                    printf("  (caused_by=%u)", ev->causal_parent_seq);
                }
                printf("\n");
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
