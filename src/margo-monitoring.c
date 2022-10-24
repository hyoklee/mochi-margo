/**
 * (C) 2022 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <string.h>
#include "margo-instance.h"
#include "margo-monitoring.h"

static void* margo_default_monitor_initialize(margo_instance_id mid,
                                              void*             uargs,
                                              const char*       config)
{
    return NULL;
}

static void margo_default_monitor_finalize(void* uargs) {}

#define __MONITOR_FN(__event__)                                          \
    static void margo_default_monitor_on_##__event__(                    \
        void* uargs, double timestamp, margo_monitor_event_t event_type, \
        margo_monitor_##__event__##_args_t event_args)

__MONITOR_FN(progress) {}
__MONITOR_FN(trigger) {}
__MONITOR_FN(register) {}
__MONITOR_FN(deregister) {}
__MONITOR_FN(lookup) {}
__MONITOR_FN(create) {}
__MONITOR_FN(forward) {}
__MONITOR_FN(forward_cb) {}
__MONITOR_FN(respond) {}
__MONITOR_FN(respond_cb) {}
__MONITOR_FN(destroy) {}
__MONITOR_FN(bulk_create) {}
__MONITOR_FN(bulk_transfer) {}
__MONITOR_FN(bulk_transfer_cb) {}
__MONITOR_FN(bulk_free) {}
__MONITOR_FN(rpc_handler) {}
__MONITOR_FN(rpc_ult) {}
__MONITOR_FN(wait) {}
__MONITOR_FN(sleep) {}
__MONITOR_FN(set_input) {}
__MONITOR_FN(set_output) {}
__MONITOR_FN(get_input) {}
__MONITOR_FN(get_output) {}
__MONITOR_FN(free_input) {}
__MONITOR_FN(free_output) {}
__MONITOR_FN(prefinalize) {}
__MONITOR_FN(finalize) {}

static void margo_default_monitor_on_user(void*                 uargs,
                                          double                timestamp,
                                          margo_monitor_event_t event_type,
                                          void*                 args)
{}

struct margo_monitor __margo_default_monitor
    = {.uargs      = NULL,
       .initialize = margo_default_monitor_initialize,
       .finalize   = margo_default_monitor_finalize,
#define X(__x__, __y__) .on_##__y__ = margo_default_monitor_on_##__y__,
       MARGO_EXPAND_MONITOR_MACROS
#undef X
};

const struct margo_monitor* margo_default_monitor = &__margo_default_monitor;

int margo_set_monitor(margo_instance_id           mid,
                      const struct margo_monitor* monitor,
                      const char*                 config)
{
    if (!mid) return -1;
    if (mid->monitor) {
        if (mid->monitor->finalize) {
            mid->monitor->finalize(mid->monitor->uargs);
        }
    } else {
        mid->monitor = (struct margo_monitor*)malloc(sizeof(*(mid->monitor)));
    }
    if (!monitor) {
        free(mid->monitor);
        mid->monitor = NULL;
    } else {
        memcpy(mid->monitor, monitor, sizeof(*(mid->monitor)));
        if (mid->monitor->initialize)
            mid->monitor->uargs
                = mid->monitor->initialize(mid, mid->monitor->uargs, config);
    }
    return 0;
}
