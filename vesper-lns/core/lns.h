#ifndef LNS_H
#define LNS_H

#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include <stddef.h>

/* Initialise the LNS runtime (call once at startup) */
void lns_init(void);

/* Primary send entry point.
 * Resolves intent, mutates decision, selects protocol via PVM, sends data.
 * Returns 0 on success, -1 on failure. */
int lns_send(void *data, size_t len, intent_t intent);

/* Inject custom network metrics (overrides simulation) */
void lns_set_metrics(net_metrics_t metrics);

/* Tear down the LNS runtime */
void lns_shutdown(void);

#endif /* LNS_H */
