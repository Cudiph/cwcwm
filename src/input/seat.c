/* seat.c - seat initialization
 *
 * Copyright (C) 2024 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libinput.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_seat.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/server.h"
#include "cwc/util.h"

static void on_libinput_device_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_libinput_device *dev = wl_container_of(listener, dev, destroy_l);

    wl_list_remove(&dev->link);
    wl_list_remove(&dev->destroy_l.link);

    free(dev);
}

static void libinput_device_apply_config(struct libinput_device *dev)
{
    // clang-format off
    if (libinput_device_config_tap_get_finger_count(dev)) {
        libinput_device_config_tap_set_enabled(dev, g_config.tap_to_click);
        libinput_device_config_tap_set_drag_enabled(dev, g_config.tap_and_drag);
        libinput_device_config_tap_set_drag_lock_enabled(dev, g_config.drag_lock);
        libinput_device_config_tap_set_button_map(dev, g_config.tap_button_map);

        libinput_device_config_scroll_set_natural_scroll_enabled(dev, g_config.natural_scrolling);
        libinput_device_config_middle_emulation_set_enabled(dev, g_config.middle_button_emulation);
        libinput_device_config_left_handed_set(dev, g_config.left_handed);
        libinput_device_config_dwt_set_enabled(dev, g_config.disable_while_typing);
    }

    libinput_device_config_scroll_set_method(dev, g_config.scroll_method);
    libinput_device_config_click_set_method(dev, g_config.click_method);
    libinput_device_config_send_events_set_mode(dev, g_config.send_events_mode);
    libinput_device_config_accel_set_profile(dev, g_config.accel_profile);
    libinput_device_config_accel_set_speed(dev, g_config.sensitivity);
    // clang-format on
}

static void attach_pointer_device(struct cwc_seat *seat,
                                  struct wlr_input_device *device)
{
    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor, device);

    if (!wlr_input_device_is_libinput(device))
        return;

    struct libinput_device *libinput_dev =
        wlr_libinput_get_device_handle(device);

    libinput_device_apply_config(libinput_dev);
}

static void apply_mapping_pointer_device(struct wlr_input_device *dev)
{
    struct wlr_pointer *pointer = wlr_pointer_from_input_device(dev);

    // TODO: xxx
}

static void on_new_input(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat = wl_container_of(listener, seat, new_input_l);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_POINTER:
        attach_pointer_device(seat, device);
        apply_mapping_pointer_device(device);
        break;
    case WLR_INPUT_DEVICE_KEYBOARD:
        cwc_keyboard_group_add_device(seat->kbd_group, device);
        break;
    case WLR_INPUT_DEVICE_TOUCH:
    case WLR_INPUT_DEVICE_TABLET:
    default:
        break;
    }

    if (wlr_input_device_is_libinput(device)) {
        struct cwc_libinput_device *libinput_dev =
            calloc(1, sizeof(*libinput_dev));
        libinput_dev->device = wlr_libinput_get_device_handle(device);

        libinput_dev->destroy_l.notify = on_libinput_device_destroy;
        wl_signal_add(&device->events.destroy, &libinput_dev->destroy_l);

        wl_list_insert(&seat->libinput_devs, &libinput_dev->link);
    }
}

static void on_request_selection(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat =
        wl_container_of(listener, seat, request_selection_l);
    struct wlr_seat_request_set_selection_event *device = data;

    wlr_seat_set_selection(seat->wlr_seat, device->source, device->serial);
}

static void on_request_primary_selection(struct wl_listener *listener,
                                         void *data)
{
    struct cwc_seat *seat =
        wl_container_of(listener, seat, request_primary_selection_l);
    struct wlr_seat_request_set_primary_selection_event *device = data;

    wlr_seat_set_primary_selection(seat->wlr_seat, device->source,
                                   device->serial);
}

static void on_request_start_drag(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat =
        wl_container_of(listener, seat, request_start_drag_l);
    struct wlr_seat_request_start_drag_event *event = data;

    if (wlr_seat_validate_pointer_grab_serial(seat->wlr_seat, event->origin,
                                              event->serial)) {
        wlr_seat_start_pointer_drag(seat->wlr_seat, event->drag, event->serial);
        return;
    }

    cwc_log(CWC_DEBUG, "ignoring start_drag request: %u", event->serial);
    wlr_data_source_destroy(event->drag->source);
}

static void on_drag_motion(struct wl_listener *listener, void *data)
{
    struct cwc_drag *drag = wl_container_of(listener, drag, on_drag_motion_l);
    struct wlr_cursor *cursor = server.seat->cursor->wlr_cursor;
    wlr_scene_node_set_position(&drag->scene_tree->node, cursor->x, cursor->y);
}

static void on_drag_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_drag *drag = wl_container_of(listener, drag, on_drag_destroy_l);
    wl_list_remove(&drag->on_drag_destroy_l.link);
    wl_list_remove(&drag->on_drag_motion_l.link);
    free(drag);
}

static void on_start_drag(struct wl_listener *listener, void *data)
{
    struct wlr_drag *drag = data;

    struct cwc_drag *cwc_drag = calloc(1, sizeof(*cwc_drag));
    cwc_drag->wlr_drag        = drag;
    cwc_drag->scene_tree =
        wlr_scene_drag_icon_create(server.root.overlay, drag->icon);

    cwc_drag->on_drag_motion_l.notify  = on_drag_motion;
    cwc_drag->on_drag_destroy_l.notify = on_drag_destroy;
    wl_signal_add(&drag->events.motion, &cwc_drag->on_drag_motion_l);
    wl_signal_add(&drag->events.destroy, &cwc_drag->on_drag_destroy_l);
}

static void on_config_commit(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat = wl_container_of(listener, seat, config_commit_l);

    struct cwc_libinput_device *libdev;
    wl_list_for_each(libdev, &seat->libinput_devs, link)
    {
        libinput_device_apply_config(libdev->device);
    }
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat = wl_container_of(listener, seat, destroy_l);

    cwc_cursor_destroy(seat->cursor);
    cwc_keyboard_group_destroy(seat->kbd_group);
    free(seat);
}

/* create new seat currently only support one seat
 *
 * automatically freed when wlr_seat destroyed
 */
struct cwc_seat *cwc_seat_create()
{
    struct cwc_seat *seat = calloc(1, sizeof(*seat));

    seat->wlr_seat       = wlr_seat_create(server.wl_display, "seat0");
    seat->wlr_seat->data = seat;
    seat->cursor         = cwc_cursor_create(seat->wlr_seat);
    seat->kbd_group      = cwc_keyboard_group_create(seat, false);

    wl_list_init(&seat->libinput_devs);

    seat->destroy_l.notify = on_destroy;
    wl_signal_add(&seat->wlr_seat->events.destroy, &seat->destroy_l);

    seat->new_input_l.notify                 = on_new_input;
    seat->request_selection_l.notify         = on_request_selection;
    seat->request_primary_selection_l.notify = on_request_primary_selection;
    seat->request_start_drag_l.notify        = on_request_start_drag;
    seat->start_drag_l.notify                = on_start_drag;
    wl_signal_add(&server.backend->events.new_input, &seat->new_input_l);
    wl_signal_add(&seat->wlr_seat->events.request_set_selection,
                  &seat->request_selection_l);
    wl_signal_add(&seat->wlr_seat->events.request_set_primary_selection,
                  &seat->request_primary_selection_l);
    wl_signal_add(&seat->wlr_seat->events.request_start_drag,
                  &seat->request_start_drag_l);
    wl_signal_add(&seat->wlr_seat->events.start_drag, &seat->start_drag_l);

    seat->config_commit_l.notify = on_config_commit;
    wl_signal_add(&g_config.events.commit, &seat->config_commit_l);

    wlr_seat_set_capabilities(seat->wlr_seat,
                              WL_SEAT_CAPABILITY_POINTER
                                  | WL_SEAT_CAPABILITY_KEYBOARD);

    return seat;
}
