#ifndef _CWC_INPUT_MANAGER_H
#define _CWC_INPUT_MANAGER_H

#include <wayland-server-core.h>

struct cwc_libinput_device {
    struct wl_list link; // cwc_input_manager.devices
    struct libinput_device *device;
    struct wlr_input_device *wlr_input_dev;

    struct wl_listener destroy_l;
};

struct cwc_input_manager {
    struct wl_list devices; // cwc_libinput_device.link
    struct wl_list seats;   // cwc_seat.link

    // pointer
    struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
    struct wlr_pointer_gestures_v1 *pointer_gestures;

    struct wlr_cursor_shape_manager_v1 *cursor_shape_manager;
    struct wl_listener request_set_shape_l;

    struct wlr_pointer_constraints_v1 *pointer_constraints;
    struct wl_listener new_pointer_constraint_l;

    struct wlr_virtual_pointer_manager_v1 *virtual_pointer_manager;
    struct wl_listener new_vpointer_l;

    // kbd
    struct wlr_virtual_keyboard_manager_v1 *virtual_kbd_manager;
    struct wl_listener new_vkbd_l;

    struct wlr_keyboard_shortcuts_inhibit_manager_v1 *kbd_inhibit_manager;
    struct wl_listener new_keyboard_inhibitor_l;

    // extra
    struct wlr_transient_seat_manager_v1 *transient_seat_manager;
    struct wl_listener create_seat_l;

    struct wlr_tablet_manager_v2 *tablet_manager;

    // backend
    struct wl_listener new_input_l;
};

struct cwc_input_manager *cwc_input_manager_get();

void cwc_input_manager_destroy();

void cwc_input_manager_update_cursor_scale();

#endif // !_CWC_INPUT_MANAGER_H
