#ifndef _CWC_SEAT_H
#define _CWC_SEAT_H

#include <wayland-server-core.h>
#include <wayland-util.h>

struct cwc_server;
struct cwc_keyboard_group;

/* wlr_seat.data == cwc_seat */
struct cwc_seat {
    struct wlr_seat *wlr_seat;
    struct cwc_cursor *cursor;
    struct cwc_keyboard_group *kbd_group;
    struct cwc_layer_surface *exclusive_kbd_interactive;

    struct wl_listener new_input_l;
    struct wl_listener request_selection_l;
    struct wl_listener request_primary_selection_l;
    struct wl_listener request_start_drag_l;
    struct wl_listener start_drag_l;
    struct wl_listener destroy_l;
};

struct cwc_drag {
    struct wlr_drag *wlr_drag;
    struct wlr_scene_tree *scene_tree;

    struct wl_listener on_drag_motion_l;
    struct wl_listener on_drag_destroy_l;
};

struct cwc_libinput_device {
    struct wl_list link; // cwc_seat.libinput_devs
    struct libinput_device *device;
    struct wlr_input_device *wlr_input_dev;

    struct wl_listener destroy_l;
};

struct cwc_seat *cwc_seat_create();

#endif // !_CWC_SEAT_H
