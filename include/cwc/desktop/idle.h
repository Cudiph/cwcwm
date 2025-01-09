#ifndef _CWC_IDLE_H
#define _CWC_IDLE_H

#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>

struct cwc_idle {
    struct cwc_server *server;
    struct wlr_idle_notifier_v1 *idle_notifier;
    struct wlr_idle_inhibit_manager_v1 *inhibit_manager;

    struct wl_listener new_inhibitor_l;
};

void update_idle_inhibitor(void *data);

#endif // !_CWC_IDLE_H
