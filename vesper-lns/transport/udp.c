#include "transport.h"
#include <stdio.h>

int udp_send(void *data, size_t len)
{
    if (!data || len == 0) {
        fprintf(stderr, "[UDP] udp_send: invalid arguments\n");
        return -1;
    }

    /* Simulate a UDP datagram transmission */
    printf("[UDP] Sending datagram: %zu bytes -> \"%.*s\"\n",
           len, (int)(len > 64 ? 64 : len), (const char *)data);
    printf("[UDP] Datagram dispatched (no-ACK, fire-and-forget)\n");
    return 0;
}
