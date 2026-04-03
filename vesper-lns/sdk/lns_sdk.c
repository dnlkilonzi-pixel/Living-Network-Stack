/*
 * sdk/lns_sdk.c — Programmable Network Runtime SDK implementation
 */

#include "lns_sdk.h"
#include "../identity_layer/identity.h"
#include "../rng/rng.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Internal helper: find a node index by label (-1 if not found)
 * ------------------------------------------------------------------------- */

static int find_node(const lns_network_t *net, const char *label)
{
    int i;
    for (i = 0; i < net->node_count; i++) {
        if (strncmp(net->nodes[i].label, label,
                    sizeof(net->nodes[i].label)) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

lns_session_t *lns_session_create(unsigned int seed)
{
    lns_session_t *s = (lns_session_t *)malloc(sizeof(lns_session_t));
    if (!s) {
        fprintf(stderr, "[SDK] lns_session_create: out of memory\n");
        return NULL;
    }

    s->seed = seed;
    rng_seed((uint64_t)seed);

    network_init(&s->net);
    identity_init();
    observer_init(&s->obs);
    replay_init(&s->replay, seed);
    replay_start(&s->replay);
    bus_init(&s->bus);

    /* Attach instrumentation to the network layer */
    network_set_replay(&s->replay);
    network_set_observer(&s->obs);

    printf("[SDK] Session created (seed=%u)\n", seed);
    return s;
}

void lns_session_destroy(lns_session_t *session)
{
    if (!session) return;

    replay_stop(&session->replay);
    network_set_replay(NULL);
    network_set_observer(NULL);
    bus_shutdown(&session->bus);

    free(session);
    printf("[SDK] Session destroyed\n");
}

/* ---------------------------------------------------------------------------
 * Topology builder
 * ------------------------------------------------------------------------- */

int lns_node_add(lns_session_t *session, const char *label,
                 float latency_ms, float packet_loss, float bandwidth_mbps)
{
    net_metrics_t m;

    if (!session || !label) return -1;

    m.latency_ms     = latency_ms;
    m.packet_loss    = packet_loss;
    m.bandwidth_mbps = bandwidth_mbps;

    bus_register(&session->bus, label);
    return network_add_node(&session->net, label, m);
}

int lns_node_connect(lns_session_t *session,
                     const char *from_label, const char *to_label)
{
    int from_idx, to_idx;

    if (!session || !from_label || !to_label) return -1;

    from_idx = find_node(&session->net, from_label);
    to_idx   = find_node(&session->net, to_label);

    if (from_idx < 0 || to_idx < 0) {
        fprintf(stderr, "[SDK] lns_node_connect: node not found (\"%s\" or \"%s\")\n",
                from_label, to_label);
        return -1;
    }

    network_add_edge(&session->net, from_idx, to_idx);
    return 0;
}

/* ---------------------------------------------------------------------------
 * Intent-driven send
 * ------------------------------------------------------------------------- */

int lns_forward(lns_session_t *session,
                const char *src_label, const char *dst_label,
                intent_t intent, void *data, size_t len)
{
    int src_idx, dst_idx;

    if (!session || !src_label || !dst_label) return -1;

    src_idx = find_node(&session->net, src_label);
    dst_idx = find_node(&session->net, dst_label);

    if (src_idx < 0 || dst_idx < 0) {
        fprintf(stderr, "[SDK] lns_send: node not found (\"%s\" or \"%s\")\n",
                src_label, dst_label);
        return -1;
    }

    return network_forward(&session->net, src_idx, dst_idx, data, len, intent);
}

/* ---------------------------------------------------------------------------
 * Observability
 * ------------------------------------------------------------------------- */

void lns_observe(const lns_session_t *session)
{
    if (!session) return;
    observer_report(&session->obs);
}

obs_stats_t lns_observe_snapshot(const lns_session_t *session)
{
    obs_stats_t empty;
    if (!session) {
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return session->obs;
}

/* ---------------------------------------------------------------------------
 * Replay debugger
 * ------------------------------------------------------------------------- */

void lns_replay_dump(const lns_session_t *session, FILE *fp)
{
    if (!session || !fp) return;
    replay_dump(&session->replay, fp);
}

void lns_replay_verify(const lns_session_t *session)
{
    if (!session) return;
    replay_run(&session->replay);
}

int lns_replay_save(const lns_session_t *session, const char *path)
{
    if (!session || !path) return -1;
    return replay_dump_file(&session->replay, path);
}

/* ---------------------------------------------------------------------------
 * Semantic Protocol Reasoning Layer
 * ------------------------------------------------------------------------- */

int lns_semantic_verify(const lns_session_t      *session,
                         const intent_semantics_t *semantics,
                         semantic_violation_t     *violations_out,
                         int                       max_count)
{
    if (!session) return 0;
    return semantic_verify_session(&session->replay, semantics,
                                   violations_out, max_count);
}

int lns_semantic_compare(const lns_session_t          *session,
                          const intent_semantics_t     *semantics,
                          semantic_comparison_result_t *result_out)
{
    if (!session) return -1;
    return semantic_compare_protocols(&session->replay, semantics, result_out);
}

/* ---------------------------------------------------------------------------
 * Adversarial Protocol Synthesiser
 * ------------------------------------------------------------------------- */

int lns_synth_run(const synth_config_t *cfg, synth_result_t *result_out)
{
    return synth_run(cfg, result_out);
}

/* ---------------------------------------------------------------------------
 * Formal Spec Compiler
 * ------------------------------------------------------------------------- */

int lns_spec_compile(intent_t           intent,
                     intent_semantics_t semantics,
                     spec_program_t    *program_out)
{
    return spec_compile(intent, semantics, program_out);
}

int lns_spec_check_session(const lns_session_t *session,
                            spec_program_t      *program)
{
    if (!session) return 0;
    return spec_check_session(program, &session->replay);
}
