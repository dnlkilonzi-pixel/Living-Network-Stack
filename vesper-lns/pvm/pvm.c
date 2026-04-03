#include "pvm.h"
#include "../transport/transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Protocol registry
 * ------------------------------------------------------------------------- */

static protocol_ops_t g_registry[PVM_MAX_PROTOCOLS];
static int            g_registry_count = 0;
static int            g_initialized    = 0;

/* Built-in on_tick / on_migrate stubs (no-ops for UDP and TCP) */
static void builtin_on_tick(void *ctx)    { (void)ctx; }
static void builtin_on_migrate(const char *from, const char *to, void *ctx)
{
    (void)ctx;
    printf("[PVM] Migrating: %s -> %s\n", from, to);
}

/* Register the two built-in protocols */
static void pvm_init_builtins(void)
{
    protocol_ops_t udp_ops;
    protocol_ops_t tcp_ops;

    if (g_initialized) return;
    g_initialized = 1;

    memset(&udp_ops, 0, sizeof(udp_ops));
    strncpy(udp_ops.name, "udp", sizeof(udp_ops.name) - 1);
    udp_ops.send       = udp_send;
    udp_ops.on_tick    = builtin_on_tick;
    udp_ops.on_migrate = builtin_on_migrate;
    pvm_register(&udp_ops);

    memset(&tcp_ops, 0, sizeof(tcp_ops));
    strncpy(tcp_ops.name, "tcp", sizeof(tcp_ops.name) - 1);
    tcp_ops.send       = tcp_send;
    tcp_ops.on_tick    = builtin_on_tick;
    tcp_ops.on_migrate = builtin_on_migrate;
    pvm_register(&tcp_ops);
}

/* ---------------------------------------------------------------------------
 * Protocol handle – stores a pointer into the registry
 * ------------------------------------------------------------------------- */

struct protocol_handle {
    char               name[16];
    const protocol_ops_t *ops;   /* points into g_registry[] */
};

/* ---------------------------------------------------------------------------
 * Public API – registry
 * ------------------------------------------------------------------------- */

int pvm_register(const protocol_ops_t *ops)
{
    int i;

    if (!ops) {
        fprintf(stderr, "[PVM] pvm_register: NULL ops\n");
        return -1;
    }
    if (g_registry_count >= PVM_MAX_PROTOCOLS) {
        fprintf(stderr, "[PVM] pvm_register: registry full\n");
        return -1;
    }

    /* Reject duplicate names */
    for (i = 0; i < g_registry_count; i++) {
        if (strncmp(g_registry[i].name, ops->name,
                    sizeof(g_registry[i].name)) == 0) {
            fprintf(stderr, "[PVM] pvm_register: protocol '%s' already registered\n",
                    ops->name);
            return -1;
        }
    }

    g_registry[g_registry_count++] = *ops;
    printf("[PVM] Registered protocol: %s\n", ops->name);
    return 0;
}

/* Internal: find a registered ops by name, or NULL */
static const protocol_ops_t *pvm_find(const char *name)
{
    int i;
    pvm_init_builtins();
    for (i = 0; i < g_registry_count; i++) {
        if (strncmp(g_registry[i].name, name,
                    sizeof(g_registry[i].name)) == 0) {
            return &g_registry[i];
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Public API – handle lifecycle
 * ------------------------------------------------------------------------- */

pvm_handle_t *pvm_load(const char *name)
{
    const protocol_ops_t *ops;
    pvm_handle_t         *h;

    if (!name) {
        fprintf(stderr, "[PVM] pvm_load: NULL protocol name\n");
        return NULL;
    }

    ops = pvm_find(name);
    if (!ops) {
        fprintf(stderr, "[PVM] Unknown protocol: %s\n", name);
        return NULL;
    }

    h = (pvm_handle_t *)malloc(sizeof(pvm_handle_t));
    if (!h) {
        fprintf(stderr, "[PVM] pvm_load: allocation failure\n");
        return NULL;
    }

    strncpy(h->name, name, sizeof(h->name) - 1);
    h->name[sizeof(h->name) - 1] = '\0';
    h->ops = ops;

    printf("[PVM] Loaded protocol: %s\n", h->name);
    return h;
}

int pvm_execute(pvm_handle_t *handle, void *data, size_t len)
{
    if (!handle || !handle->ops || !handle->ops->send) {
        fprintf(stderr, "[PVM] pvm_execute: invalid handle\n");
        return -1;
    }
    printf("[PVM] Executing protocol: %s  len=%zu\n", handle->name, len);
    return handle->ops->send(data, len);
}

int pvm_swap(pvm_handle_t *handle, const char *name)
{
    const protocol_ops_t *new_ops;

    if (!handle || !name) {
        fprintf(stderr, "[PVM] pvm_swap: invalid arguments\n");
        return -1;
    }

    new_ops = pvm_find(name);
    if (!new_ops) {
        fprintf(stderr, "[PVM] pvm_swap: unknown protocol '%s'\n", name);
        return -1;
    }

    /* Notify outgoing protocol before the switch */
    if (handle->ops && handle->ops->on_migrate) {
        handle->ops->on_migrate(handle->name, name, NULL);
    }

    strncpy(handle->name, name, sizeof(handle->name) - 1);
    handle->name[sizeof(handle->name) - 1] = '\0';
    handle->ops = new_ops;

    printf("[PVM] Hot-swapped to protocol: %s\n", handle->name);
    return 0;
}

void pvm_unload(pvm_handle_t *handle)
{
    if (!handle) return;
    printf("[PVM] Unloaded protocol: %s\n", handle->name);
    free(handle);
}

void pvm_tick(pvm_handle_t *handle, void *ctx)
{
    if (!handle || !handle->ops || !handle->ops->on_tick) return;
    handle->ops->on_tick(ctx);
}
