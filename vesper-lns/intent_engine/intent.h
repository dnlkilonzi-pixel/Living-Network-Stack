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

/* ---------------------------------------------------------------------------
 * Semantic intent declarations
 *
 * These structures extend the basic intent_t with formal guarantees and
 * constraints that the chosen protocol MUST satisfy.  They feed into the
 * Semantic Protocol Reasoning Layer (semantic/semantic.h), which verifies
 * observed outcomes against declared semantics after each session.
 * ------------------------------------------------------------------------- */

/* Delivery guarantee that must be honoured */
typedef enum {
    GUARANTEE_BEST_EFFORT   = 0, /* no delivery assurance (fire-and-forget)  */
    GUARANTEE_AT_LEAST_ONCE = 1, /* must not drop; duplicates tolerated       */
    GUARANTEE_EXACTLY_ONCE  = 2  /* no drops AND no duplicates (TCP-grade)    */
} delivery_guarantee_t;

/* Optimisation objective for the session */
typedef enum {
    OBJECTIVE_NONE                = 0, /* no preference                       */
    OBJECTIVE_MINIMIZE_LATENCY    = 1, /* prefer lowest observed latency      */
    OBJECTIVE_MINIMIZE_LOSS       = 2, /* prefer lowest packet-loss path      */
    OBJECTIVE_MAXIMIZE_THROUGHPUT = 3  /* prefer highest effective bandwidth  */
} opt_objective_t;

/* Semantic constraints attached to an intent.
 * Combine with a plain intent_t to give the semantic reasoning layer
 * enough information to verify and compare protocol choices. */
typedef struct {
    delivery_guarantee_t guarantee;      /* required delivery guarantee       */
    float                max_latency_ms; /* latency budget; 0 = unconstrained */
    float                max_loss;       /* loss budget [0,1]; 0 = unconstrained */
    opt_objective_t      objective;      /* what to optimise for              */
} intent_semantics_t;

#endif /* INTENT_H */
