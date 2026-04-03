/*
 * sdk/lns_sdk.h — Programmable Network Runtime SDK
 *
 * Public façade for the Living Network Stack (LNS).
 * Include this single header; link against liblns.a.
 *
 * Typical usage:
 *
 *   lns_session_t *s = lns_session_create(42);
 *   lns_node_add(s, "edge-A", 20.0f, 0.01f, 100.0f);
 *   lns_node_add(s, "edge-B", 35.0f, 0.02f,  80.0f);
 *   lns_node_add(s, "core-C", 10.0f, 0.005f,200.0f);
 *   lns_node_connect(s, "edge-A", "edge-B");
 *   lns_node_connect(s, "edge-B", "core-C");
 *
 *   intent_t intent = { INTENT_LOW_LATENCY, 10, 500 };
 *   char payload[] = "hello";
 *   lns_forward(s, "edge-A", "core-C", intent, payload, sizeof(payload)-1);
 *
 *   lns_observe(s);               // prints dashboard
 *   lns_replay_dump(s, stdout);   // human-readable trace
 *   lns_replay_verify(s);         // re-run + assert determinism
 *   lns_replay_save(s, "run.lnslog"); // NDJSON file
 *
 *   lns_session_destroy(s);
 */

#ifndef LNS_SDK_H
#define LNS_SDK_H

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../observer/observer.h"
#include "../replay/replay.h"
#include "../network/network.h"
#include "../bus/bus.h"
#include <stdio.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Opaque session handle — owns all runtime state for one LNS instance.
 * Do not access fields directly; use the SDK functions below.
 * ------------------------------------------------------------------------- */

typedef struct lns_session {
    lns_network_t net;
    replay_log_t  replay;
    obs_stats_t   obs;
    bus_t         bus;
    unsigned int  seed;
} lns_session_t;

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

/* Create a new session seeded with 'seed'.
 * Initialises the network, replay log, observer and message bus.
 * Attaches the replay log and observer to the network automatically.
 * Returns a heap-allocated session; caller must call lns_session_destroy(). */
lns_session_t *lns_session_create(unsigned int seed);

/* Tear down the session: detaches instrumentation, shuts down the bus,
 * and frees the session memory. */
void lns_session_destroy(lns_session_t *session);

/* ---------------------------------------------------------------------------
 * Topology builder
 * ------------------------------------------------------------------------- */

/* Add a node with the given label and initial network conditions.
 * Returns the node's integer index, or -1 on failure. */
int lns_node_add(lns_session_t *session, const char *label,
                 float latency_ms, float packet_loss, float bandwidth_mbps);

/* Connect 'from_label' → 'to_label' with a directed edge.
 * Returns 0 on success, -1 if either label is not found. */
int lns_node_connect(lns_session_t *session,
                     const char *from_label, const char *to_label);

/* ---------------------------------------------------------------------------
 * Intent-driven send
 * ------------------------------------------------------------------------- */

/* Forward 'data' (len bytes) from src_label to dst_label using the given
 * intent.  The network resolves the shortest path, mutates per-hop decisions,
 * and records every event into the session's replay log and observer.
 * Returns 0 on success, -1 on failure. */
int lns_forward(lns_session_t *session,
                const char *src_label, const char *dst_label,
                intent_t intent, void *data, size_t len);

/* ---------------------------------------------------------------------------
 * Observability
 * ------------------------------------------------------------------------- */

/* Print the global system observer dashboard to stdout. */
void lns_observe(const lns_session_t *session);

/* Return a snapshot of the current observer statistics. */
obs_stats_t lns_observe_snapshot(const lns_session_t *session);

/* ---------------------------------------------------------------------------
 * Replay debugger
 * ------------------------------------------------------------------------- */

/* Dump the recorded trace in human-readable form to fp. */
void lns_replay_dump(const lns_session_t *session, FILE *fp);

/* Re-execute the trace deterministically and verify all decisions match.
 * Prints a MATCH / MISMATCH report to stdout. */
void lns_replay_verify(const lns_session_t *session);

/* Write the trace as NDJSON to path (one JSON object per line).
 * Returns 0 on success, -1 on I/O failure. */
int lns_replay_save(const lns_session_t *session, const char *path);

#endif /* LNS_SDK_H */
