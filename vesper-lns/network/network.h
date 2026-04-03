#ifndef NETWORK_H
#define NETWORK_H

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../identity_layer/identity.h"
#include "../replay/replay.h"
#include "../observer/observer.h"
#include <stddef.h>

#define MAX_NODES          8
#define MAX_EDGES          16
#define MAX_DECISION_LOG   8

/* ---------------------------------------------------------------------------
 * Decision Packet – shared between nodes for distributed intelligence
 * ------------------------------------------------------------------------- */

typedef struct {
    decision_t    decision;   /* what the source node decided */
    net_metrics_t observed;   /* metrics the source node observed */
    node_id_t     source;     /* which node produced this packet */
    int           valid;      /* 1 = slot is occupied */
} decision_packet_t;

/* ---------------------------------------------------------------------------
 * Single node in the simulated multi-node network
 * ------------------------------------------------------------------------- */

typedef struct {
    node_id_t         id;
    net_metrics_t     metrics;             /* base (configured) metrics      */
    stress_state_t    stress;              /* mutable congestion/queue state */
    char              label[32];
    decision_packet_t decision_log[MAX_DECISION_LOG]; /* ring buffer */
    int               decision_log_count;             /* total logged so far */
} lns_node_t;

/* ---------------------------------------------------------------------------
 * Directed edge between two nodes (by index)
 * ------------------------------------------------------------------------- */

typedef struct {
    int from_idx;
    int to_idx;
} network_edge_t;

/* ---------------------------------------------------------------------------
 * The simulated multi-node network
 * ------------------------------------------------------------------------- */

typedef struct {
    lns_node_t     nodes[MAX_NODES];
    int            node_count;
    network_edge_t edges[MAX_EDGES];
    int            edge_count;
} lns_network_t;

/* Initialise an empty network */
void network_init(lns_network_t *net);

/* Add a node; returns its index, or -1 on failure */
int  network_add_node(lns_network_t *net, const char *label,
                      net_metrics_t metrics);

/* Add a directed edge from→to */
void network_add_edge(lns_network_t *net, int from_idx, int to_idx);

/* Forward a message along the shortest path from src to dst.
 * Each intermediate hop independently resolves intent and mutates based
 * on its own metrics plus any learned neighbor decisions.
 * Returns 0 on success, -1 on failure. */
int  network_forward(lns_network_t *net, int src_idx, int dst_idx,
                     void *data, size_t len, intent_t intent);

/* Propagate decisions: each node broadcasts its latest decision packet
 * to directly-connected neighbours, enabling emergent behavior
 * convergence without a central controller. */
void network_propagate_decisions(lns_network_t *net);

/* Print topology and current state of all nodes */
void network_print(const lns_network_t *net);

/* ---------------------------------------------------------------------------
 * Phase 3: attach optional instrumentation to the network
 * ------------------------------------------------------------------------- */

/* Attach a replay log.  When set, network_forward() and
 * network_propagate_decisions() record events into it.
 * Pass NULL to detach. */
void network_set_replay(replay_log_t *log);

/* Attach a system observer.  When set, hop / forward / propagation
 * statistics are accumulated into it automatically.
 * Pass NULL to detach. */
void network_set_observer(obs_stats_t *obs);

#endif /* NETWORK_H */
