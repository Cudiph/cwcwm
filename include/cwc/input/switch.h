#ifndef _CWC_INPUT_SWITCH_H
#define _CWC_INPUT_SWITCH_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>

struct cwc_switch {
    struct wl_list link; // cwc_seat.switch_devs
    struct cwc_seat *seat;
    struct wlr_switch *wlr_switch;

    struct wl_listener toggle_l;
    struct wl_listener destroy_l;
};

struct cwc_switch *cwc_switch_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev);

void cwc_switch_destroy(struct cwc_switch *switch_dev);

#endif // !_CWC_INPUT_SWITCH_H
