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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_transient_seat_v1.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/input/switch.h"
#include "cwc/input/tablet.h"
#include "cwc/input/touch.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "private/callback.h"

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

    struct wlr_touch_point *point;
    if (wlr_seat_validate_touch_grab_serial(seat->wlr_seat, event->origin,
                                            event->serial, &point)) {
        wlr_seat_start_touch_drag(seat->wlr_seat, event->drag, event->serial,
                                  point);
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
    struct cwc_seat *seat = drag->wlr_drag->seat->data;

    struct cwc_toplevel *toplevel = cwc_toplevel_try_from_wlr_surface(
        seat->wlr_seat->keyboard_state.focused_surface);
    struct cwc_output *output =
        cwc_output_at(server.output_layout, seat->cursor->wlr_cursor->x,
                      seat->cursor->wlr_cursor->y);

    /* restore client focus but since the focus stack is stored per output basis
     * so it only works if drag and drop between 2 clients are in the same
     * output. */
    if (toplevel && output && toplevel->container->output == output)
        cwc_output_focus_newest_focus_visible_toplevel(output);

    wl_list_remove(&drag->on_drag_destroy_l.link);
    wl_list_remove(&drag->on_drag_motion_l.link);

    wlr_scene_node_destroy(&drag->scene_tree->node);

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

static void _cwc_seat_destroy(struct cwc_seat *seat)
{
    cwc_log(CWC_DEBUG, "destroying seat (%s): %p", seat->wlr_seat->name, seat);

    cwc_cursor_destroy(seat->cursor);
    cwc_keyboard_group_destroy(seat->kbd_group);

    wl_list_remove(&seat->request_set_cursor_l.link);
    wl_list_remove(&seat->pointer_focus_change_l.link);
    wl_list_remove(&seat->keyboard_focus_change_l.link);

    wl_list_remove(&seat->destroy_l.link);
    wl_list_remove(&seat->request_selection_l.link);
    wl_list_remove(&seat->request_primary_selection_l.link);
    wl_list_remove(&seat->request_start_drag_l.link);
    wl_list_remove(&seat->start_drag_l.link);

    wl_list_remove(&seat->link);
    free(seat);
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat = wl_container_of(listener, seat, destroy_l);

    _cwc_seat_destroy(seat);
}

static void cwc_seat_update_capabilities(struct cwc_seat *seat)
{
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

    if (wl_list_length_at_least(&seat->kbd_group->wlr_kbd_group->devices, 1))
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;

    if (wl_list_length_at_least(&seat->touch_devs, 1))
        caps |= WL_SEAT_CAPABILITY_TOUCH;

    wlr_seat_set_capabilities(seat->wlr_seat, caps);
}

/* create new seat currently only support one seat
 *
 * automatically freed when wlr_seat destroyed
 */
struct cwc_seat *cwc_seat_create(struct cwc_input_manager *manager,
                                 const char *name)
{
    struct cwc_seat *seat = calloc(1, sizeof(*seat));
    if (!seat)
        return NULL;

    cwc_log(CWC_DEBUG, "creating seat (%s): %p", name, seat);

    seat->wlr_seat       = wlr_seat_create(server.wl_display, name);
    seat->wlr_seat->data = seat;

    seat->cursor    = cwc_cursor_create(seat->wlr_seat);
    seat->kbd_group = cwc_keyboard_group_create(seat, NULL);

    wl_list_init(&seat->switch_devs);
    wl_list_init(&seat->tablet_devs);
    wl_list_init(&seat->tablet_pad_devs);
    wl_list_init(&seat->touch_devs);
    wl_list_init(&seat->text_inputs);

    seat->destroy_l.notify = on_destroy;
    wl_signal_add(&seat->wlr_seat->events.destroy, &seat->destroy_l);

    seat->request_set_cursor_l.notify    = on_request_set_cursor;
    seat->pointer_focus_change_l.notify  = on_pointer_focus_change;
    seat->keyboard_focus_change_l.notify = on_keyboard_focus_change;
    wl_signal_add(&seat->wlr_seat->events.request_set_cursor,
                  &seat->request_set_cursor_l);
    wl_signal_add(&seat->wlr_seat->pointer_state.events.focus_change,
                  &seat->pointer_focus_change_l);
    wl_signal_add(&seat->wlr_seat->keyboard_state.events.focus_change,
                  &seat->keyboard_focus_change_l);

    seat->request_selection_l.notify         = on_request_selection;
    seat->request_primary_selection_l.notify = on_request_primary_selection;
    seat->request_start_drag_l.notify        = on_request_start_drag;
    seat->start_drag_l.notify                = on_start_drag;
    wl_signal_add(&seat->wlr_seat->events.request_set_selection,
                  &seat->request_selection_l);
    wl_signal_add(&seat->wlr_seat->events.request_set_primary_selection,
                  &seat->request_primary_selection_l);
    wl_signal_add(&seat->wlr_seat->events.request_start_drag,
                  &seat->request_start_drag_l);
    wl_signal_add(&seat->wlr_seat->events.start_drag, &seat->start_drag_l);

    wl_list_insert(&manager->seats, &seat->link);

    cwc_seat_update_capabilities(seat);

    return seat;
}

void cwc_seat_destroy(struct cwc_seat *seat)
{
    wlr_seat_destroy(seat->wlr_seat);
}

void cwc_seat_add_keyboard_device(struct cwc_seat *seat,
                                  struct wlr_input_device *dev)
{
    cwc_keyboard_group_add_device(seat->kbd_group, dev);
}

static void map_input_device_to_output(struct cwc_seat *seat,
                                       struct wlr_input_device *dev)
{
    const char *output_name = NULL;

    switch (dev->type) {
    case WLR_INPUT_DEVICE_POINTER:
        output_name = wlr_pointer_from_input_device(dev)->output_name;
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        output_name = wlr_touch_from_input_device(dev)->output_name;
        break;
    default:
        return;
    }

    if (output_name == NULL)
        return;

    struct cwc_output *output = cwc_output_get_by_name(output_name);

    if (!output)
        return;

    wlr_cursor_map_input_to_output(seat->cursor->wlr_cursor, dev,
                                   output->wlr_output);
}

void cwc_seat_add_pointer_device(struct cwc_seat *seat,
                                 struct wlr_input_device *dev)
{
    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor, dev);
    map_input_device_to_output(seat, dev);
    cwc_seat_update_capabilities(seat);
}

void cwc_seat_add_switch_device(struct cwc_seat *seat,
                                struct wlr_input_device *dev)
{
    cwc_switch_create(seat, dev);
}

void cwc_seat_add_tablet_device(struct cwc_seat *seat,
                                struct wlr_input_device *dev)
{
    cwc_tablet_create(seat, dev);
}

void cwc_seat_add_tablet_pad_device(struct cwc_seat *seat,
                                    struct wlr_input_device *dev)
{
    cwc_tablet_pad_create(seat, dev);
}

void cwc_seat_add_touch_device(struct cwc_seat *seat,
                               struct wlr_input_device *dev)
{
    map_input_device_to_output(seat, dev);
    cwc_touch_create(seat, dev);
    cwc_seat_update_capabilities(seat);
}

static void generate_seat_name(struct cwc_input_manager *mgr,
                               bool transient,
                               char *buf,
                               int len)
{
    int count = 0;
    struct cwc_seat *seat;

    if (transient) {
        wl_list_for_each(seat, &mgr->seats, link)
        {
            if (strncmp(seat->wlr_seat->name, "tseat", 5) == 0)
                count++;
        }

        snprintf(buf, len, "tseat%d", count);
        return;
    }

    wl_list_for_each(seat, &mgr->seats, link)
    {
        if (strncmp(seat->wlr_seat->name, "seat", 4) == 0)
            count++;
    }
    snprintf(buf, len, "seat%d", count);
}

static void on_create_tseat(struct wl_listener *listener, void *data)
{
    struct cwc_input_manager *input_mgr =
        wl_container_of(listener, input_mgr, create_seat_l);
    struct wlr_transient_seat_v1 *t_seat = data;

    char name[50];
    generate_seat_name(input_mgr, true, name, sizeof(name) - 1);

    struct cwc_seat *seat = cwc_seat_create(input_mgr, name);

    if (seat && seat->wlr_seat) {
        wlr_transient_seat_v1_ready(t_seat, seat->wlr_seat);
    } else {
        wlr_transient_seat_v1_deny(t_seat);
    }
}

void setup_seat(struct cwc_input_manager *input_mgr)
{
    // main seat
    server.seat = cwc_seat_create(input_mgr, "seat0");

    // transient seat
    input_mgr->transient_seat_manager =
        wlr_transient_seat_manager_v1_create(server.wl_display);
    input_mgr->create_seat_l.notify = on_create_tseat;
    wl_signal_add(&input_mgr->transient_seat_manager->events.create_seat,
                  &input_mgr->create_seat_l);
}

void cleanup_seat(struct cwc_input_manager *input_mgr)
{
    wl_list_remove(&input_mgr->create_seat_l.link);
}
