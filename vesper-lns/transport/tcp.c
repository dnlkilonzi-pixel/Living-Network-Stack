#include "transport.h"
#include <stdio.h>

int tcp_send(void *data, size_t len)
{
    if (!data || len == 0) {
        fprintf(stderr, "[TCP] tcp_send: invalid arguments\n");
        return -1;
    }

    /* Simulate a TCP stream segment transmission */
    printf("[TCP] Sending stream segment: %zu bytes -> \"%.*s\"\n",
           len, (int)(len > 64 ? 64 : len), (const char *)data);
    printf("[TCP] Three-way handshake complete, data acknowledged\n");
    return 0;
}
