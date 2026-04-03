#ifndef MUTATION_H
#define MUTATION_H

#include "../intent_engine/intent.h"

/* Network condition metrics sampled at runtime */
typedef struct {
    float latency_ms;     /* Round-trip latency in milliseconds */
    float packet_loss;    /* Packet loss ratio  [0.0 .. 1.0] */
    float bandwidth_mbps; /* Available bandwidth in Mbps */
} net_metrics_t;

/* Mutate a decision in-place based on observed network conditions.
 * The decision_t must have been produced by resolve_intent() first. */
void mutate(decision_t *d, net_metrics_t metrics);

/* Generate a simulated set of network metrics for testing */
net_metrics_t simulate_metrics(void);

/* Pretty-print a metrics snapshot */
void metrics_print(net_metrics_t m);

#endif /* MUTATION_H */
