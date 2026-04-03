#ifndef IDENTITY_H
#define IDENTITY_H

#include "../intent_engine/intent.h"
#include <stdint.h>
#include <stddef.h>

#define NODE_ID_LEN  32   /* bytes */

/* Opaque 32-byte node identifier */
typedef struct {
    uint8_t id[NODE_ID_LEN];
} node_id_t;

/* A resolved transport address (IPv4 + port, as strings for simplicity) */
typedef struct {
    char addr[64];   /* e.g. "192.168.1.10" */
    uint16_t port;
    int valid;       /* 0 = lookup failed */
} transport_addr_t;

/* Initialise the identity subsystem (loads mock routing table) */
void identity_init(void);

/* Lookup the transport address for a node identifier.
 * Returns a transport_addr_t with valid=0 when not found. */
transport_addr_t identity_lookup(node_id_t target);

/* Send data to a node by identity, resolving the address internally.
 * Returns 0 on success, -1 on failure. */
int lns_send_to(node_id_t target, void *data, size_t len, intent_t intent);

/* Utility: create a node_id_t from a null-terminated ASCII label (for demo) */
node_id_t node_id_from_label(const char *label);

/* Pretty-print a node identifier */
void node_id_print(node_id_t id);

#endif /* IDENTITY_H */
