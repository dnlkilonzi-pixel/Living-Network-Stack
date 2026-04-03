/*
 * sdk/examples/example_basic.c
 *
 * Minimal SDK usage: build a 3-node topology, send one packet,
 * then print the observability dashboard.
 *
 * Compile (after `make lib`):
 *   gcc -std=c99 -I../../ -o example_basic example_basic.c \
 *       ../../liblns.a
 */

#include "../lns_sdk.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    lns_session_t *s;
    intent_t       intent;
    char           payload[] = "fintech-tx: amount=9999.00 currency=USD";
    int            rc;

    /* -----------------------------------------------------------------------
     * 1. Create a session (seed controls random metric simulation)
     * -------------------------------------------------------------------- */
    s = lns_session_create(2024);
    if (!s) return 1;

    /* -----------------------------------------------------------------------
     * 2. Build topology: edge-A → relay-B → core-C
     *    latency_ms  packet_loss  bandwidth_mbps
     * -------------------------------------------------------------------- */
    lns_node_add(s, "edge-A",  45.0f, 0.02f,  70.0f);
    lns_node_add(s, "relay-B", 30.0f, 0.05f,  50.0f);
    lns_node_add(s, "core-C",  10.0f, 0.005f, 200.0f);

    lns_node_connect(s, "edge-A",  "relay-B");
    lns_node_connect(s, "relay-B", "core-C");

    /* -----------------------------------------------------------------------
     * 3. Send with LOW_LATENCY intent
     * -------------------------------------------------------------------- */
    intent.flags    = INTENT_LOW_LATENCY;
    intent.priority = 10;
    intent.ttl_ms   = 500;

    rc = lns_forward(s, "edge-A", "core-C", intent,
                  payload, strlen(payload));

    printf("\n[EXAMPLE] Send result: %s\n", rc == 0 ? "OK" : "FAIL");

    /* -----------------------------------------------------------------------
     * 4. Print the observability dashboard
     * -------------------------------------------------------------------- */
    lns_observe(s);

    /* -----------------------------------------------------------------------
     * 5. Clean up
     * -------------------------------------------------------------------- */
    lns_session_destroy(s);
    return rc == 0 ? 0 : 1;
}
