#include "intent.h"
#include <stdio.h>

decision_t resolve_intent(intent_t intent)
{
    decision_t d;
    d.use_udp       = 0;
    d.use_encryption = 0;
    d.retry_count   = 3;
    d.exec_packet   = 0;

    if (intent.flags & INTENT_LOW_LATENCY) {
        /* Low-latency prefers UDP with minimal retries */
        d.use_udp     = 1;
        d.retry_count = 1;
    }

    if (intent.flags & INTENT_HIGH_THROUGHPUT) {
        /* High-throughput prefers UDP for bulk data */
        d.use_udp     = 1;
        d.retry_count = 2;
    }

    if (intent.flags & INTENT_HIGH_SECURITY) {
        /* Security mandates encryption; use reliable TCP */
        d.use_udp        = 0;
        d.use_encryption = 1;
        d.retry_count    = 5;
    }

    /* HIGH_SECURITY overrides LOW_LATENCY's UDP preference when both set */
    if ((intent.flags & INTENT_HIGH_SECURITY) &&
        (intent.flags & INTENT_LOW_LATENCY)) {
        d.use_udp = 0;   /* security wins */
    }

    return d;
}

void intent_print(intent_t intent)
{
    printf("[INTENT] flags=0x%02x (", intent.flags);
    if (intent.flags & INTENT_LOW_LATENCY)     printf("LOW_LATENCY ");
    if (intent.flags & INTENT_HIGH_THROUGHPUT) printf("HIGH_THROUGHPUT ");
    if (intent.flags & INTENT_HIGH_SECURITY)   printf("HIGH_SECURITY ");
    printf(") priority=%u ttl_ms=%u\n", intent.priority, intent.ttl_ms);
}

void decision_print(const decision_t *d)
{
    printf("[DECISION] protocol=%s encryption=%s retries=%d exec_packet=%s\n",
           d->use_udp        ? "UDP" : "TCP",
           d->use_encryption ? "ON"  : "OFF",
           d->retry_count,
           d->exec_packet    ? "YES" : "NO");
}
