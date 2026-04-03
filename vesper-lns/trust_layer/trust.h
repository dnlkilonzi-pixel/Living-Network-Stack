#ifndef TRUST_H
#define TRUST_H

#include "../identity_layer/identity.h"

/* ---------------------------------------------------------------------------
 * Node capability flags
 * ------------------------------------------------------------------------- */

#define CAP_COMPUTE  (1 << 0)   /* Can run in-network computation */
#define CAP_RELAY    (1 << 1)   /* Can forward packets to other nodes */
#define CAP_STORAGE  (1 << 2)   /* Has persistent storage */
#define CAP_CRYPTO   (1 << 3)   /* Hardware-accelerated cryptography */

/* Trust thresholds */
#define TRUST_LOW_THRESHOLD   30   /* below = untrusted, avoid routing through */
#define TRUST_HIGH_THRESHOLD  70   /* above = highly trusted, prefer for crypto */

/* ---------------------------------------------------------------------------
 * Node identity profile: identity + trust + capability
 * ------------------------------------------------------------------------- */

typedef struct {
    node_id_t id;
    int       trust_score;   /* 0–100 */
    int       capabilities;  /* bitmask of CAP_* flags */
    char      label[32];
} node_profile_t;

/* Initialise the trust registry */
void trust_registry_init(void);

/* Register a node profile */
void trust_registry_add(node_profile_t profile);

/* Lookup a node's profile by identity.  Returns NULL if not found. */
const node_profile_t *trust_lookup(node_id_t id);

/* Returns 1 if the node's trust_score >= min_trust, 0 otherwise */
int trust_check(node_id_t id, int min_trust);

/* Select the best candidate from a list based on required capabilities and
 * minimum trust.  Writes the chosen id into *selected.
 * Returns the list index of the chosen node, or -1 if none qualify. */
int trust_route_select(const node_id_t *candidates, int count,
                       int required_cap, int min_trust,
                       node_id_t *selected);

/* Apply a signed delta to a node's trust score (clamped to [0, 100]) */
void trust_update(node_id_t id, int delta);

/* Pretty-print a single node profile */
void trust_print(const node_profile_t *p);

/* Print the entire trust registry */
void trust_registry_print(void);

#endif /* TRUST_H */
