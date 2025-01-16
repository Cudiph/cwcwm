#ifndef _CWC_INPUT_TOUCH_H
#define _CWC_INPUT_TOUCH_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>

struct cwc_touch {
    struct wl_list link;
    struct cwc_seat *seat;
    struct wlr_touch *wlr_touch;

    struct wl_listener down_l;
    struct wl_listener up_l;
    struct wl_listener motion_l;
    struct wl_listener cancel_l;
    struct wl_listener frame_l;

    struct wl_listener destroy_l;
};

struct cwc_touch *cwc_touch_create(struct cwc_seat *seat,
                                   struct wlr_input_device *dev);

void cwc_touch_destroy(struct cwc_touch *touch);

#endif // !_CWC_INPUT_TOUCH_H
