/*
 * sdk/examples/example_replay.c
 *
 * SDK replay-debugger usage:
 *   - Build a topology, run a forward
 *   - Dump human-readable trace to stdout
 *   - Save NDJSON trace to a .lnslog file
 *   - Verify determinism (re-execute and assert all decisions match)
 *
 * Compile (after `make lib`):
 *   gcc -std=c99 -I../../ -o example_replay example_replay.c \
 *       ../../liblns.a
 */

#include "../lns_sdk.h"
#include <stdio.h>
#include <string.h>

#define LOG_PATH "/tmp/lns_trace.lnslog"

int main(void)
{
    lns_session_t *s;
    intent_t       intent;
    char           payload[] = "edge-compute: job_id=42 fn=resize_image";
    obs_stats_t    snap;
    int            rc;

    /* -----------------------------------------------------------------------
     * 1. Session + topology
     * -------------------------------------------------------------------- */
    s = lns_session_create(7777);
    if (!s) return 1;

    lns_node_add(s, "ingress",    80.0f, 0.04f, 40.0f);
    lns_node_add(s, "processor",  25.0f, 0.01f, 120.0f);
    lns_node_add(s, "egress",     15.0f, 0.008f, 180.0f);

    lns_node_connect(s, "ingress",   "processor");
    lns_node_connect(s, "processor", "egress");

    /* -----------------------------------------------------------------------
     * 2. Send with HIGH_THROUGHPUT intent
     * -------------------------------------------------------------------- */
    intent.flags    = INTENT_HIGH_THROUGHPUT;
    intent.priority = 5;
    intent.ttl_ms   = 2000;

    rc = lns_forward(s, "ingress", "egress", intent,
                  payload, strlen(payload));

    printf("\n[EXAMPLE] Send result: %s\n", rc == 0 ? "OK" : "FAIL");

    /* -----------------------------------------------------------------------
     * 3. Dump human-readable trace to stdout
     * -------------------------------------------------------------------- */
    lns_replay_dump(s, stdout);

    /* -----------------------------------------------------------------------
     * 4. Save NDJSON trace to file
     * -------------------------------------------------------------------- */
    if (lns_replay_save(s, LOG_PATH) == 0) {
        printf("[EXAMPLE] NDJSON trace saved to: %s\n", LOG_PATH);
    }

    /* -----------------------------------------------------------------------
     * 5. Verify determinism (re-runs all hop decisions, checks for matches)
     * -------------------------------------------------------------------- */
    lns_replay_verify(s);

    /* -----------------------------------------------------------------------
     * 6. Show a stats snapshot (programmatic access, no printf inside SDK)
     * -------------------------------------------------------------------- */
    snap = lns_observe_snapshot(s);
    printf("[EXAMPLE] Total hops: %u  |  UDP: %u  |  TCP: %u\n",
           snap.total_hops, snap.udp_hops, snap.tcp_hops);
    if (snap.convergence_count > 0) {
        printf("[EXAMPLE] Convergence events: %u | Avg convergence time: %llu ms\n",
               snap.convergence_count,
               (unsigned long long)(snap.convergence_time_ms
                                    / snap.convergence_count));
    }

    /* -----------------------------------------------------------------------
     * 7. Clean up
     * -------------------------------------------------------------------- */
    lns_session_destroy(s);
    return rc == 0 ? 0 : 1;
}
