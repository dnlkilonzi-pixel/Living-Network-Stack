#include "lns.h"
#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../pvm/pvm.h"
#include "../execution_engine/exec.h"
#include "../rng/rng.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Module state
 * ------------------------------------------------------------------------- */

static int          lns_ready   = 0;
static net_metrics_t lns_metrics = {0.0f, 0.0f, 100.0f};
static int          custom_metrics_set = 0;

/* ---------------------------------------------------------------------------
 * Encryption simulation
 * ------------------------------------------------------------------------- */

static void encrypt_payload(void *data, size_t len)
{
    /* XOR-based placeholder; real code would use AES-GCM / ChaCha20 */
    uint8_t *bytes = (uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        bytes[i] ^= 0xA5u;
    }
    printf("[LNS] Payload encrypted (%zu bytes, XOR simulation)\n", len);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void lns_init(void)
{
    if (lns_ready) return;

    printf("[LNS] Runtime initialising...\n");
    rng_seed(42);   /* deterministic metrics simulation in demo */
    lns_ready = 1;
    printf("[LNS] Runtime ready\n");
}

void lns_set_metrics(net_metrics_t metrics)
{
    lns_metrics        = metrics;
    custom_metrics_set = 1;
    printf("[LNS] Custom metrics injected\n");
    metrics_print(lns_metrics);
}

int lns_send(void *data, size_t len, intent_t intent)
{
    int rc = 0;

    if (!lns_ready) {
        fprintf(stderr, "[LNS] lns_send called before lns_init()\n");
        return -1;
    }
    if (!data || len == 0) {
        fprintf(stderr, "[LNS] lns_send: invalid arguments\n");
        return -1;
    }

    printf("\n[LNS] ===== lns_send  len=%zu =====\n", len);

    /* Step 1 – Resolve intent into an initial decision */
    intent_print(intent);
    decision_t decision = resolve_intent(intent);
    decision_print(&decision);

    /* Step 2 – Sample/inject network metrics and mutate the decision */
    net_metrics_t metrics = custom_metrics_set
                          ? lns_metrics
                          : simulate_metrics();
    metrics_print(metrics);
    mutate(&decision, metrics);
    printf("[LNS] Post-mutation ");
    decision_print(&decision);

    /* Step 3 – Optional encryption */
    void *send_buf = data;
    uint8_t *enc_buf = NULL;

    if (decision.use_encryption) {
        enc_buf = (uint8_t *)malloc(len);
        if (!enc_buf) {
            fprintf(stderr, "[LNS] Encryption buffer allocation failed\n");
            return -1;
        }
        memcpy(enc_buf, data, len);
        encrypt_payload(enc_buf, len);
        send_buf = enc_buf;
    }

    /* Step 4 – Load protocol via PVM and execute with retry loop */
    const char *proto_name = decision.use_udp ? "udp" : "tcp";
    pvm_handle_t *handle   = pvm_load(proto_name);

    if (!handle) {
        fprintf(stderr, "[LNS] Failed to load protocol: %s\n", proto_name);
        free(enc_buf);
        return -1;
    }

    for (int attempt = 0; attempt <= decision.retry_count; attempt++) {
        if (attempt > 0) {
            printf("[LNS] Retry attempt %d/%d\n", attempt,
                   decision.retry_count);
        }
        rc = pvm_execute(handle, send_buf, len);
        if (rc == 0) break;
    }

    pvm_unload(handle);

    /* Step 5 – Free the encrypted buffer if it was allocated */
    if (enc_buf) {
        free(enc_buf);
    }

    printf("[LNS] ===== send complete rc=%d =====\n\n", rc);
    return rc;
}

void lns_shutdown(void)
{
    if (!lns_ready) return;
    printf("[LNS] Runtime shutting down\n");
    lns_ready          = 0;
    custom_metrics_set = 0;
}
