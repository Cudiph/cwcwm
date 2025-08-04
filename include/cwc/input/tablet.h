#ifndef _CWC_INPUT_TABLET_H
#define _CWC_INPUT_TABLET_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>

struct cwc_tablet {
    struct wl_list link;
    struct cwc_seat *seat;
    struct wlr_tablet_v2_tablet *tablet;

    struct wl_listener axis_l;
    struct wl_listener proximity_l;
    struct wl_listener tip_l;
    struct wl_listener button_l;
    struct wl_listener destroy_l;
};

struct cwc_tablet *cwc_tablet_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev);

void cwc_tablet_destroy(struct cwc_tablet *tablet);

struct cwc_tablet_pad {
    struct wl_list link;
    struct cwc_seat *seat;
    struct wlr_tablet_v2_tablet_pad *tablet_pad;

    struct wl_listener button_l;
    struct wl_listener ring_l;
    struct wl_listener strip_l;
    struct wl_listener attach_tablet_l;
};

struct cwc_tablet_pad *cwc_tablet_pad_create(struct cwc_seat *seat,
                                             struct wlr_input_device *dev);

void cwc_tablet_pad_destroy(struct cwc_tablet_pad *tpad);

#endif // !_CWC_INPUT_TABLET_H
