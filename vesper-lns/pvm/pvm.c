#include "pvm.h"
#include "../transport/transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Protocol function pointer type */
typedef int (*proto_send_fn)(void *data, size_t len);

struct protocol_handle {
    char            name[16];
    proto_send_fn   send_fn;
};

pvm_handle_t *pvm_load(const char *name)
{
    pvm_handle_t *h;

    if (!name) {
        fprintf(stderr, "[PVM] pvm_load: NULL protocol name\n");
        return NULL;
    }

    h = (pvm_handle_t *)malloc(sizeof(pvm_handle_t));
    if (!h) {
        fprintf(stderr, "[PVM] pvm_load: allocation failure\n");
        return NULL;
    }

    strncpy(h->name, name, sizeof(h->name) - 1);
    h->name[sizeof(h->name) - 1] = '\0';

    if (strcmp(name, "udp") == 0) {
        h->send_fn = udp_send;
        printf("[PVM] Loaded protocol: UDP\n");
    } else if (strcmp(name, "tcp") == 0) {
        h->send_fn = tcp_send;
        printf("[PVM] Loaded protocol: TCP\n");
    } else {
        fprintf(stderr, "[PVM] Unknown protocol: %s\n", name);
        free(h);
        return NULL;
    }

    return h;
}

int pvm_execute(pvm_handle_t *handle, void *data, size_t len)
{
    if (!handle || !handle->send_fn) {
        fprintf(stderr, "[PVM] pvm_execute: invalid handle\n");
        return -1;
    }
    printf("[PVM] Executing protocol: %s  len=%zu\n", handle->name, len);
    return handle->send_fn(data, len);
}

void pvm_unload(pvm_handle_t *handle)
{
    if (!handle) return;
    printf("[PVM] Unloaded protocol: %s\n", handle->name);
    free(handle);
}
