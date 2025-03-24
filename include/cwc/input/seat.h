#ifndef _CWC_SEAT_H
#define _CWC_SEAT_H

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_input_device.h>

struct cwc_server;
struct cwc_keyboard_group;
struct cwc_input_manager;

/* wlr_seat.data == cwc_seat */
struct cwc_seat {
    struct wl_list link; // struct cwc_input_manager.seats
    struct wlr_seat *wlr_seat;

    struct cwc_cursor *cursor;
    struct cwc_keyboard_group *kbd_group;

    struct cwc_layer_surface *exclusive_kbd_interactive;
    struct wlr_keyboard_shortcuts_inhibitor_v1 *kbd_inhibitor;

    struct cwc_input_method *input_method;
    struct cwc_text_input *focused_text_input;
    struct wlr_input_method_keyboard_grab_v2 *kbd_grab;

    struct wl_list switch_devs;     // struct cwc_switch.link
    struct wl_list tablet_devs;     // struct cwc_tablet.link
    struct wl_list tablet_pad_devs; // struct cwc_tablet_pad.link
    struct wl_list touch_devs;      // struct cwc_touch.link
    struct wl_list text_inputs;     // struct cwc_text_input.link

    struct wl_listener request_set_cursor_l;
    struct wl_listener pointer_focus_change_l;
    struct wl_listener keyboard_focus_change_l;

    struct wl_listener request_selection_l;
    struct wl_listener request_primary_selection_l;
    struct wl_listener request_start_drag_l;
    struct wl_listener start_drag_l;
    struct wl_listener destroy_l;

    struct wl_listener kbd_grab_destroy_l;
};

struct cwc_drag {
    struct wlr_drag *wlr_drag;
    struct wlr_scene_tree *scene_tree;

    struct wl_listener on_drag_motion_l;
    struct wl_listener on_drag_destroy_l;
};

struct cwc_seat *cwc_seat_create(struct cwc_input_manager *manager,
                                 const char *name);

void cwc_seat_destroy(struct cwc_seat *seat);

void cwc_seat_add_keyboard_device(struct cwc_seat *seat,
                                  struct wlr_input_device *dev);

void cwc_seat_add_pointer_device(struct cwc_seat *seat,
                                 struct wlr_input_device *dev);

void cwc_seat_add_switch_device(struct cwc_seat *seat,
                                struct wlr_input_device *dev);

void cwc_seat_add_tablet_device(struct cwc_seat *seat,
                                struct wlr_input_device *dev);

void cwc_seat_add_tablet_pad_device(struct cwc_seat *seat,
                                    struct wlr_input_device *dev);

void cwc_seat_add_touch_device(struct cwc_seat *seat,
                               struct wlr_input_device *dev);

#endif // !_CWC_SEAT_H
