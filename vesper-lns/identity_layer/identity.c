#include "identity.h"
#include "../core/lns.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Mock routing table
 * Maps a human-readable label (stored in the first NODE_ID_LEN bytes) to
 * an IP address + port.  In a real implementation this would be backed by a
 * DHT or a cryptographic name resolution service.
 * ------------------------------------------------------------------------- */

#define ROUTE_TABLE_SIZE 8

typedef struct {
    uint8_t     id[NODE_ID_LEN];
    char        addr[64];
    uint16_t    port;
} route_entry_t;

static route_entry_t route_table[ROUTE_TABLE_SIZE];
static int           route_count = 0;

static void add_route(const char *label, const char *addr, uint16_t port)
{
    if (route_count >= ROUTE_TABLE_SIZE) return;

    route_entry_t *e = &route_table[route_count++];
    memset(e->id, 0, NODE_ID_LEN);
    strncpy((char *)e->id, label, NODE_ID_LEN - 1);
    strncpy(e->addr, addr, sizeof(e->addr) - 1);
    e->addr[sizeof(e->addr) - 1] = '\0';
    e->port = port;
}

void identity_init(void)
{
    route_count = 0;

    add_route("node-alpha",   "10.0.0.1",  9001);
    add_route("node-beta",    "10.0.0.2",  9002);
    add_route("node-gamma",   "10.0.0.3",  9003);
    add_route("node-delta",   "192.168.1.10", 8080);
    add_route("edge-node-1",  "172.16.0.1",  7001);
    add_route("edge-node-2",  "172.16.0.2",  7002);

    printf("[IDENTITY] Routing table initialised with %d entries\n",
           route_count);
}

transport_addr_t identity_lookup(node_id_t target)
{
    transport_addr_t result;
    memset(&result, 0, sizeof(result));
    result.valid = 0;

    for (int i = 0; i < route_count; i++) {
        if (memcmp(route_table[i].id, target.id, NODE_ID_LEN) == 0) {
            strncpy(result.addr, route_table[i].addr,
                    sizeof(result.addr) - 1);
            result.addr[sizeof(result.addr) - 1] = '\0';
            result.port  = route_table[i].port;
            result.valid = 1;
            return result;
        }
    }
    return result;
}

int lns_send_to(node_id_t target, void *data, size_t len, intent_t intent)
{
    transport_addr_t addr = identity_lookup(target);

    if (!addr.valid) {
        char id_str[NODE_ID_LEN + 1];
        memcpy(id_str, target.id, NODE_ID_LEN);
        id_str[NODE_ID_LEN] = '\0';
        fprintf(stderr,
                "[IDENTITY] Route not found for node \"%s\"\n", id_str);
        return -1;
    }

    printf("[IDENTITY] Resolved node -> %s:%u\n", addr.addr, addr.port);
    return lns_send(data, len, intent);
}

node_id_t node_id_from_label(const char *label)
{
    node_id_t nid;
    memset(nid.id, 0, NODE_ID_LEN);
    if (label) {
        strncpy((char *)nid.id, label, NODE_ID_LEN - 1);
    }
    return nid;
}

void node_id_print(node_id_t id)
{
    /* Print the human-readable label portion */
    char label[NODE_ID_LEN + 1];
    memcpy(label, id.id, NODE_ID_LEN);
    label[NODE_ID_LEN] = '\0';
    printf("[IDENTITY] NodeID: \"%s\"\n", label);
}
