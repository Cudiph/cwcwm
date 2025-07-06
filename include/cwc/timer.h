#ifndef _CWC_TIMER_H
#define _CWC_TIMER_H

#include <wayland-server-core.h>

struct cwc_timer {
    struct wl_list link;
    struct wl_event_source *timer;
    double timeout_ms;
    bool started;
    bool single_shot;
    bool one_shot;
    int cb_ref;   // callback ref in timer registry
    int data_ref; // userdata ref in timer registry
};

void cwc_timer_destroy(struct cwc_timer *timer);

#endif // !_CWC_TIMER_H
