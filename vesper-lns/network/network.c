#include "network.h"
#include "../pvm/pvm.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Optional Phase 3 instrumentation hooks (NULL = disabled)
 * ------------------------------------------------------------------------- */

static replay_log_t *g_replay   = NULL;
static obs_stats_t  *g_observer = NULL;

void network_set_replay(replay_log_t *log)   { g_replay   = log; }
void network_set_observer(obs_stats_t  *obs) { g_observer = obs; }

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/* BFS to find the shortest directed path from src to dst.
 * path[] is filled with node indices; *path_len receives the length.
 * Returns 0 on success, -1 if no path exists. */
static int bfs_path(const lns_network_t *net, int src, int dst,
                    int *path, int *path_len, int max_path)
{
    int visited[MAX_NODES];
    int prev[MAX_NODES];
    int queue[MAX_NODES];
    int head = 0, tail = 0;
    int i;

    for (i = 0; i < MAX_NODES; i++) {
        visited[i] = 0;
        prev[i]    = -1;
    }

    visited[src]   = 1;
    queue[tail++]  = src;

    while (head < tail) {
        int curr = queue[head++];

        if (curr == dst) {
            /* Reconstruct path by walking prev[] backwards */
            int tmp[MAX_NODES];
            int len  = 0;
            int node = dst;
            while (node != -1 && len < MAX_NODES) {
                tmp[len++] = node;
                node       = prev[node];
            }
            *path_len = (len < max_path) ? len : max_path;
            for (i = 0; i < *path_len; i++) {
                path[i] = tmp[*path_len - 1 - i];
            }
            return 0;
        }

        for (i = 0; i < net->edge_count; i++) {
            if (net->edges[i].from_idx == curr) {
                int next = net->edges[i].to_idx;
                if (!visited[next]) {
                    visited[next] = 1;
                    prev[next]    = curr;
                    queue[tail++] = next;
                }
            }
        }
    }
    return -1;
}

/* Apply influence from a node's logged decisions to an in-progress decision.
 * This implements "distributed intelligence without ML": a node learns from
 * its neighbours' observed conditions before committing to a decision. */
static void apply_neighbor_influence(decision_t *d, const lns_node_t *node)
{
    int i;
    for (i = 0; i < node->decision_log_count && i < MAX_DECISION_LOG; i++) {
        const decision_packet_t *dp = &node->decision_log[i % MAX_DECISION_LOG];
        if (!dp->valid) continue;

        if (dp->observed.latency_ms > 150.0f && !d->use_encryption) {
            printf("  [NETWORK] Neighbor influence from \"%.*s\": "
                   "high latency %.1f ms -> biasing UDP\n",
                   NODE_ID_LEN, (const char *)dp->source.id,
                   dp->observed.latency_ms);
            d->use_udp = 1;
        }

        if (dp->observed.packet_loss > 0.10f) {
            printf("  [NETWORK] Neighbor influence from \"%.*s\": "
                   "high loss %.1f%% -> +1 retry\n",
                   NODE_ID_LEN, (const char *)dp->source.id,
                   dp->observed.packet_loss * 100.0f);
            d->retry_count++;
        }
    }
}

/* Append a decision to a node's ring-buffer log */
static void node_log_decision(lns_node_t *node, decision_t d,
                              net_metrics_t observed)
{
    int slot              = node->decision_log_count % MAX_DECISION_LOG;
    decision_packet_t *dp = &node->decision_log[slot];

    dp->decision = d;
    dp->observed = observed;
    dp->source   = node->id;
    dp->valid    = 1;
    node->decision_log_count++;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void network_init(lns_network_t *net)
{
    memset(net, 0, sizeof(*net));
    printf("[NETWORK] Initialised empty network\n");
}

int network_add_node(lns_network_t *net, const char *label,
                     net_metrics_t metrics)
{
    int        idx;
    lns_node_t *n;

    if (net->node_count >= MAX_NODES) {
        fprintf(stderr, "[NETWORK] Node limit reached (%d)\n", MAX_NODES);
        return -1;
    }

    idx = net->node_count++;
    n   = &net->nodes[idx];
    memset(n, 0, sizeof(*n));

    strncpy(n->label, label, sizeof(n->label) - 1);
    n->metrics = metrics;
    n->id      = node_id_from_label(label);

    printf("[NETWORK] Added node[%d]: \"%s\"\n", idx, n->label);
    return idx;
}

void network_add_edge(lns_network_t *net, int from_idx, int to_idx)
{
    network_edge_t *e;

    if (net->edge_count >= MAX_EDGES) {
        fprintf(stderr, "[NETWORK] Edge limit reached (%d)\n", MAX_EDGES);
        return;
    }
    if (from_idx < 0 || from_idx >= net->node_count ||
        to_idx   < 0 || to_idx   >= net->node_count) {
        fprintf(stderr, "[NETWORK] Invalid edge indices %d->%d\n",
                from_idx, to_idx);
        return;
    }

    e            = &net->edges[net->edge_count++];
    e->from_idx  = from_idx;
    e->to_idx    = to_idx;

    printf("[NETWORK] Added edge: \"%s\" -> \"%s\"\n",
           net->nodes[from_idx].label, net->nodes[to_idx].label);
}

int network_forward(lns_network_t *net, int src_idx, int dst_idx,
                    void *data, size_t len, intent_t intent)
{
    int path[MAX_NODES];
    int path_len = 0;
    int hop;

    if (src_idx < 0 || src_idx >= net->node_count ||
        dst_idx < 0 || dst_idx >= net->node_count) {
        fprintf(stderr, "[NETWORK] Invalid src/dst indices\n");
        return -1;
    }

    printf("\n[NETWORK] ===== Forward \"%s\" -> \"%s\"  len=%zu =====\n",
           net->nodes[src_idx].label, net->nodes[dst_idx].label, len);

    if (bfs_path(net, src_idx, dst_idx, path, &path_len, MAX_NODES) != 0) {
        fprintf(stderr, "[NETWORK] No path from \"%s\" to \"%s\"\n",
                net->nodes[src_idx].label, net->nodes[dst_idx].label);
        return -1;
    }

    printf("[NETWORK] Path (%d hops): ", path_len);
    for (hop = 0; hop < path_len; hop++) {
        printf("\"%s\"", net->nodes[path[hop]].label);
        if (hop < path_len - 1) printf(" -> ");
    }
    printf("\n");

    /* Process each hop independently */
    for (hop = 0; hop < path_len; hop++) {
        lns_node_t *node = &net->nodes[path[hop]];
        pvm_handle_t *handle;
        decision_t d;
        int rc;

        printf("\n  [HOP %d/%d] Node \"%s\"\n", hop + 1, path_len, node->label);

        /* Step 1: resolve intent into initial decision */
        d = resolve_intent(intent);

        /* Step 2: incorporate learned decisions from neighbors */
        apply_neighbor_influence(&d, node);

        /* Step 3: mutate based on this node's own observed metrics */
        mutate(&d, node->metrics);
        printf("  [HOP %d] Post-mutation ", hop + 1);
        decision_print(&d);

        /* Step 4: log decision for future propagation */
        node_log_decision(node, d, node->metrics);

        /* Step 5: execute via PVM */
        handle = pvm_load(d.use_udp ? "udp" : "tcp");
        if (!handle) {
            fprintf(stderr, "[NETWORK] PVM load failed at hop %d\n", hop + 1);
            return -1;
        }
        rc = pvm_execute(handle, data, len);
        pvm_unload(handle);

        /* Phase 3: record this hop into replay log and observer */
        if (g_replay) {
            replay_record_hop(g_replay, node->label,
                              hop + 1, path_len,
                              intent, node->metrics, d);
        }
        if (g_observer) {
            observer_record_hop(g_observer, d.use_udp, node->metrics);
        }

        if (rc != 0) {
            fprintf(stderr, "[NETWORK] Send failed at hop %d\n", hop + 1);
            if (g_observer) observer_record_forward(g_observer, 0);
            return -1;
        }
    }

    printf("\n[NETWORK] ===== Forward complete =====\n");
    if (g_replay)   replay_record_forward(g_replay,
                                          net->nodes[src_idx].label,
                                          net->nodes[dst_idx].label, 1);
    if (g_observer) observer_record_forward(g_observer, 1);
    return 0;
}

void network_propagate_decisions(lns_network_t *net)
{
    int e;

    printf("\n[NETWORK] ===== Decision Propagation Round =====\n");

    for (e = 0; e < net->edge_count; e++) {
        int        from_idx = net->edges[e].from_idx;
        int        to_idx   = net->edges[e].to_idx;
        lns_node_t *from    = &net->nodes[from_idx];
        lns_node_t *to      = &net->nodes[to_idx];
        int         slot;
        decision_packet_t dp;

        if (from->decision_log_count == 0) {
            printf("[NETWORK] \"%s\" has no decisions to share yet\n",
                   from->label);
            continue;
        }

        /* Share the most recent decision */
        slot = (from->decision_log_count - 1) % MAX_DECISION_LOG;
        dp   = from->decision_log[slot];

        printf("[NETWORK] Propagating: \"%s\" -> \"%s\"  "
               "(proto=%s, loss=%.1f%%, latency=%.1f ms)\n",
               from->label, to->label,
               dp.decision.use_udp ? "UDP" : "TCP",
               dp.observed.packet_loss * 100.0f,
               dp.observed.latency_ms);

        /* Phase 3: record propagation event */
        if (g_replay) {
            replay_record_propagate(g_replay,
                                    from->label, to->label,
                                    dp.decision, dp.observed);
        }

        /* Inject into neighbour's log for future mutation influence */
        to->decision_log[to->decision_log_count % MAX_DECISION_LOG] = dp;
        to->decision_log_count++;
    }

    printf("[NETWORK] ===== Propagation complete =====\n");
    if (g_observer) observer_record_propagation(g_observer);
}

void network_print(const lns_network_t *net)
{
    int i;

    printf("\n[NETWORK] === Network State: %d nodes, %d edges ===\n",
           net->node_count, net->edge_count);

    for (i = 0; i < net->node_count; i++) {
        const lns_node_t *n = &net->nodes[i];
        printf("  [%d] \"%s\"  decisions_logged=%d  ", i, n->label,
               n->decision_log_count);
        metrics_print(n->metrics);
    }

    printf("[NETWORK] Edges:\n");
    for (i = 0; i < net->edge_count; i++) {
        printf("  \"%s\" -> \"%s\"\n",
               net->nodes[net->edges[i].from_idx].label,
               net->nodes[net->edges[i].to_idx].label);
    }
    printf("\n");
}
