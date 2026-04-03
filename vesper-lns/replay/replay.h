#ifndef REPLAY_H
#define REPLAY_H

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include <stdint.h>
#include <stdio.h>

#define REPLAY_MAX_EVENTS  512

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
 * The union stores the minimal inputs needed to re-execute deterministically:
 *   - HOP:       intent + metrics → sufficient to call resolve_intent()+mutate()
 *   - FORWARD:   source/dest labels and success flag
 *   - PROPAGATE: source/dest labels and the decision + observed metrics
 * ------------------------------------------------------------------------- */

typedef struct {
    replay_event_type_t type;
    uint32_t            seq;         /* monotonic event counter */
    char                node[32];    /* originating node label */

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
    unsigned int   seed;     /* srand() seed active during recording */
    int            active;   /* 1 = currently recording */
} replay_log_t;

/* Initialise an empty log with the given seed (must match srand() in use) */
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

/* Dump the full trace in human-readable form to fp */
void replay_dump(const replay_log_t *log, FILE *fp);

/* Write the full trace as NDJSON to the file at path.
 * Each event is one JSON-object line (machine-parseable).
 * Returns 0 on success, -1 on failure. */
int replay_dump_file(const replay_log_t *log, const char *path);

/* Re-execute the log: sets srand(log->seed), then for each HOP event
 * re-calls resolve_intent() + mutate() with the stored inputs, verifying
 * the result matches the recorded decision.  Proves determinism. */
void replay_run(const replay_log_t *log);

#endif /* REPLAY_H */
