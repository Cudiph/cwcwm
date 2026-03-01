#ifndef _CWC_INPUT_TABLET_H
#define _CWC_INPUT_TABLET_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_tablet_tool.h>

struct cwc_cursor;

struct cwc_tablet_tool {
    struct wlr_tablet_v2_tablet_tool *tablet_v2_tool;
    struct cwc_seat *seat;

    struct wl_listener set_cursor_l;
    struct wl_listener destroy_l;
};

struct cwc_tablet {
    struct wl_list link; // cwc_seat.tablet_devs
    struct cwc_seat *seat;
    struct wlr_tablet_v2_tablet *tablet_v2;

    struct wl_listener destroy_l;
};

struct cwc_tablet *cwc_tablet_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev);

void process_tablet_tool_proximity(
    struct cwc_cursor *cursor, struct wlr_tablet_tool_proximity_event *event);
void process_tablet_tool_motion(struct cwc_cursor *cursor,
                                struct wlr_tablet_tool_axis_event *event);
void process_tablet_tool_tip(struct cwc_cursor *cursor,
                             struct wlr_tablet_tool_tip_event *event);
void process_tablet_tool_button(struct cwc_cursor *cursor,
                                struct wlr_tablet_tool_button_event *event);

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
