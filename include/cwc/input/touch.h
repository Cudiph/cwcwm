#ifndef _CWC_INPUT_TOUCH_H
#define _CWC_INPUT_TOUCH_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_touch.h>

struct cwc_cursor;

struct cwc_touch {
    struct wl_list link;
    struct cwc_seat *seat;
    struct wlr_touch *wlr_touch;

    struct wl_list touches; // touch_point.link

    struct wl_listener destroy_l;
};

struct cwc_touch *cwc_touch_create(struct cwc_seat *seat,
                                   struct wlr_input_device *dev);

void cwc_touch_destroy(struct cwc_touch *touch);

void process_touch_up(struct cwc_cursor *cursor,
                      struct wlr_touch_up_event *event);
void process_touch_down(struct cwc_cursor *cursor,
                        struct wlr_touch_down_event *event);
void process_touch_motion(struct cwc_cursor *cursor,
                          struct wlr_touch_motion_event *event);
void process_touch_cancel(struct cwc_cursor *cursor,
                          struct wlr_touch_cancel_event *event);
void process_touch_frame(struct cwc_cursor *cursor);

#endif // !_CWC_INPUT_TOUCH_H
