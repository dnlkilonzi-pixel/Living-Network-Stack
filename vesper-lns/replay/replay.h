#ifndef REPLAY_H
#define REPLAY_H

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include <stdint.h>
#include <stdio.h>

#define REPLAY_MAX_EVENTS  512

/* Number of distinct nodes tracked by the vector clock.
 * Must match MAX_NODES in network.h; kept separate to avoid a circular
 * include between replay/ and network/. */
#define REPLAY_MAX_NODES   8

/* Sentinel: causal_parent_seq == REPLAY_NO_PARENT means no causal parent */
#define REPLAY_NO_PARENT   UINT32_MAX

/* ---------------------------------------------------------------------------
 * Event types
 * ------------------------------------------------------------------------- */

typedef enum {
    REPLAY_HOP       = 0,   /* one routing hop was processed */
    REPLAY_FORWARD   = 1,   /* a full network_forward() call started/ended */
    REPLAY_PROPAGATE = 2    /* a decision_packet_t was propagated */
} replay_event_type_t;

/* ---------------------------------------------------------------------------
 * A single recorded event
 *
 * causal_parent_seq — seq of the most recent PROPAGATE event that caused
 *   a decision change at this node, or REPLAY_NO_PARENT if this hop's
 *   decision arose purely from local metrics.
 *
 * vc[] — snapshot of the vector clock at the moment this event fired.
 *   vc[i] is the logical-time of node i (by insertion order in the log).
 * ------------------------------------------------------------------------- */

typedef struct {
    replay_event_type_t type;
    uint32_t            seq;                      /* monotonic event counter */
    char                node[32];                 /* originating node label  */

    /* Causal model */
    uint32_t            causal_parent_seq;        /* REPLAY_NO_PARENT = root */
    uint32_t            vc[REPLAY_MAX_NODES];     /* vector clock snapshot   */

    union {
        /* REPLAY_HOP */
        struct {
            int           hop_num;
            int           total_hops;
            intent_t      intent;    /* input: intent fed into resolve_intent */
            net_metrics_t metrics;   /* input: metrics fed into mutate()      */
            decision_t    decision;  /* output: post-mutation decision        */
        } hop;

        /* REPLAY_FORWARD */
        struct {
            char dst[32];
            int  success;
        } forward;

        /* REPLAY_PROPAGATE */
        struct {
            char          from[32];
            char          to[32];
            decision_t    decision;
            net_metrics_t observed;
        } propagate;
    } data;
} replay_event_t;

/* ---------------------------------------------------------------------------
 * The replay log
 * ------------------------------------------------------------------------- */

typedef struct {
    replay_event_t events[REPLAY_MAX_EVENTS];
    int            count;
    unsigned int   seed;      /* srand()-compatible seed (legacy) */
    int            active;    /* 1 = currently recording */

    /* Centralized RNG state (xorshift64) snapshotted at replay_start() */
    uint64_t       rng_state;

    /* Per-node vector clocks (indexed by label insertion order) */
    uint32_t       node_vc[REPLAY_MAX_NODES];
    char           vc_labels[REPLAY_MAX_NODES][32];
    int            vc_node_count;
} replay_log_t;

/* Initialise an empty log with the given seed (must match rng_seed() in use) */
void replay_init(replay_log_t *log, unsigned int seed);

/* Start / stop recording */
void replay_start(replay_log_t *log);
void replay_stop(replay_log_t *log);

/* Record a HOP event.  Called once per node in network_forward(). */
void replay_record_hop(replay_log_t *log, const char *node,
                       int hop_num, int total_hops,
                       intent_t intent, net_metrics_t metrics,
                       decision_t decision);

/* Record the start/end of a network_forward() call */
void replay_record_forward(replay_log_t *log, const char *src,
                           const char *dst, int success);

/* Record one decision propagation step */
void replay_record_propagate(replay_log_t *log, const char *from,
                             const char *to,
                             decision_t decision,
                             net_metrics_t observed);

/* Causal query: return the event that caused event[seq], or NULL if root.
 * Walks causal_parent_seq chain back one step. */
const replay_event_t *replay_find_cause(const replay_log_t *log,
                                        uint32_t seq);

/* Dump the full trace in human-readable form to fp */
void replay_dump(const replay_log_t *log, FILE *fp);

/* Write the full trace as NDJSON to the file at path.
 * Each event is one JSON-object line (machine-parseable).
 * Returns 0 on success, -1 on failure. */
int replay_dump_file(const replay_log_t *log, const char *path);

/* Re-execute the log: restores the RNG state, then for each HOP event
 * re-calls resolve_intent() + mutate() with the stored inputs, verifying
 * the result matches the recorded decision.  Proves determinism. */
void replay_run(const replay_log_t *log);

#endif /* REPLAY_H */
