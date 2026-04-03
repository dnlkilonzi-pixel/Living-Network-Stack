#ifndef MUTATION_H
#define MUTATION_H

#include "../intent_engine/intent.h"
#include <stddef.h>

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

/* ---------------------------------------------------------------------------
 * Protocol Configuration – live-evolvable transport parameters
 * ------------------------------------------------------------------------- */

typedef struct {
    int window_size;          /* bytes: TCP receive window / UDP batch size */
    int retransmit_delay_ms;  /* milliseconds between retransmissions */
    int packet_size;          /* target segment / datagram size in bytes */
    int congestion_window;    /* multiplicative congestion factor */
} proto_config_t;

/* Create a protocol configuration pre-loaded with sensible defaults */
proto_config_t proto_config_default(void);

/* Evolve protocol parameters in-place based on observed network metrics.
 * Unlike mutate() which switches protocols, this evolves the parameters
 * of the currently selected protocol dynamically. */
void mutate_proto_config(proto_config_t *cfg, net_metrics_t metrics);

/* Pretty-print a proto_config_t */
void proto_config_print(const proto_config_t *cfg);

/* ---------------------------------------------------------------------------
 * Backpressure / Network Stress Model
 *
 * Tracks per-node queue depth and jitter so that sustained high-rate sending
 * raises latency and loss proportionally to congestion — replacing the
 * previous "perfect network with logging" behaviour.
 * ------------------------------------------------------------------------- */

/* Maximum queue depth modelled per node (bytes) */
#define STRESS_MAX_QUEUE_BYTES  (1024 * 1024)  /* 1 MB soft cap */

typedef struct {
    float queue_bytes;  /* currently queued / in-flight bytes */
    float jitter_ms;    /* smoothed one-way jitter (ms) */
} stress_state_t;

/* Reset a stress state (call once after node creation) */
void stress_init(stress_state_t *s);

/* Update stress state after sending bytes_sent over elapsed_ms at bw_mbps.
 * Derives congestion-aware metrics (latency / loss / effective bandwidth)
 * from base and returns them.  base itself is NOT modified. */
net_metrics_t stress_update(stress_state_t *s, size_t bytes_sent,
                             float bw_mbps, float elapsed_ms,
                             net_metrics_t base);

#endif /* MUTATION_H */
