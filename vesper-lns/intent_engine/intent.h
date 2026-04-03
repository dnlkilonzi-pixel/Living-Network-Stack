#ifndef INTENT_H
#define INTENT_H

#include <stddef.h>
#include <stdint.h>

/* Intent flags - can be combined with bitwise OR */
#define INTENT_LOW_LATENCY      (1 << 0)
#define INTENT_HIGH_THROUGHPUT  (1 << 1)
#define INTENT_HIGH_SECURITY    (1 << 2)

typedef struct {
    uint32_t flags;        /* Combination of INTENT_* flags */
    uint32_t priority;     /* 0 = default, higher = more urgent */
    uint32_t ttl_ms;       /* Time-to-live in milliseconds (0 = no limit) */
} intent_t;

/* Resolution decision produced by the intent engine */
typedef struct {
    int use_udp;           /* 1 = UDP, 0 = TCP */
    int use_encryption;    /* 1 = enable encryption simulation */
    int retry_count;       /* Number of retransmission attempts */
    int exec_packet;       /* 1 = treat payload as executable packet */
} decision_t;

/* Resolve an intent into a concrete protocol decision */
decision_t resolve_intent(intent_t intent);

/* Pretty-print an intent */
void intent_print(intent_t intent);

/* Pretty-print a decision */
void decision_print(const decision_t *d);

#endif /* INTENT_H */
