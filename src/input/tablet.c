/* tablet.c - tablet input device
 *
 * Copyright (C) 2025 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
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

#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_v2.h>

#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/input/tablet.h"
#include "cwc/server.h"
#include "cwc/util.h"

static void on_tablet_tool_set_cursor(struct wl_listener *listener, void *data)
{
    struct cwc_tablet_tool *tabtool =
        wl_container_of(listener, tabtool, set_cursor_l);
    struct wlr_tablet_v2_event_cursor *event = data;

    struct cwc_cursor *cursor = tabtool->seat->cursor;
    struct wlr_seat_client *focused_client =
        cursor->seat->pointer_state.focused_client;

    if (!focused_client || event->seat_client != focused_client)
        return;

    cwc_cursor_set_surface(cursor, event->surface, event->hotspot_x,
                           event->hotspot_y);
}

static void on_tablet_tool_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_tablet_tool *tabtool =
        wl_container_of(listener, tabtool, destroy_l);

    wl_list_remove(&tabtool->set_cursor_l.link);
    wl_list_remove(&tabtool->destroy_l.link);

    free(tabtool);
}

static void tablet_tool_init(struct cwc_tablet *tablet,
                             struct wlr_tablet_tool *wlr_tool)
{
    struct cwc_tablet_tool *tabtool = calloc(1, sizeof(*tabtool));
    if (!tabtool)
        return;

    tabtool->seat           = tablet->seat;
    tabtool->tablet_v2_tool = wlr_tablet_tool_create(
        server.input->tablet_manager, tablet->seat->wlr_seat, wlr_tool);
    wlr_tool->data = tabtool;

    tabtool->set_cursor_l.notify = on_tablet_tool_set_cursor;
    tabtool->destroy_l.notify    = on_tablet_tool_destroy;
    wl_signal_add(&tabtool->tablet_v2_tool->events.set_cursor,
                  &tabtool->set_cursor_l);
    wl_signal_add(&wlr_tool->events.destroy, &tabtool->destroy_l);
}

void handle_cursor_motion(struct cwc_cursor *cursor,
                          struct wlr_tablet_tool_axis_event *event)
{
    bool changed_x = event->updated_axes & WLR_TABLET_TOOL_AXIS_X;
    bool changed_y = event->updated_axes & WLR_TABLET_TOOL_AXIS_Y;

    if (!changed_x && !changed_y)
        return;
    struct wlr_input_device *device = &event->tablet->base;
    struct wlr_cursor *wlr_cursor   = cursor->wlr_cursor;
    struct cwc_tablet *tablet       = event->tablet->data;
    struct cwc_tablet_tool *tabtool = event->tool->data;

    double cx = wlr_cursor->x;
    double cy = wlr_cursor->y;
    double sx, sy;
    struct wlr_surface *surface = scene_surface_at(cx, cy, &sx, &sy);

    if (!surface)
        goto move_only;

    if (wlr_surface_accepts_tablet_v2(surface, tablet->tablet_v2)) {
        wlr_tablet_v2_tablet_tool_notify_proximity_in(
            tabtool->tablet_v2_tool, tablet->tablet_v2, surface);
        wlr_tablet_v2_tablet_tool_notify_motion(tabtool->tablet_v2_tool, sx,
                                                sy);
    } else {
        process_cursor_motion(cursor, event->time_msec, device, event->dx,
                              event->dy, event->dx, event->dy);
    }

move_only:
    wlr_cursor_warp_absolute(cursor->wlr_cursor, device,
                             changed_x ? event->x : NAN,
                             changed_y ? event->y : NAN);
}

void process_tablet_tool_motion(struct cwc_cursor *cursor,
                                struct wlr_tablet_tool_axis_event *event)
{

    cwc_cursor_notify_activity(cursor);

    struct cwc_tablet_tool *tabtool = event->tool->data;
    struct cwc_tablet *tablet       = event->tablet->data;

    handle_cursor_motion(cursor, event);

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
        wlr_tablet_v2_tablet_tool_notify_pressure(tabtool->tablet_v2_tool,
                                                  event->pressure);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
        wlr_tablet_v2_tablet_tool_notify_distance(tabtool->tablet_v2_tool,
                                                  event->distance);
    }

    if (event->updated_axes
        & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
        wlr_tablet_v2_tablet_tool_notify_tilt(tabtool->tablet_v2_tool,
                                              event->tilt_x, event->tilt_y);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
        wlr_tablet_v2_tablet_tool_notify_rotation(tabtool->tablet_v2_tool,
                                                  event->rotation);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
        wlr_tablet_v2_tablet_tool_notify_slider(tabtool->tablet_v2_tool,
                                                event->slider);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
        wlr_tablet_v2_tablet_tool_notify_wheel(tabtool->tablet_v2_tool,
                                               event->wheel_delta, 0);
    }
}

void process_tablet_tool_proximity(
    struct cwc_cursor *cursor, struct wlr_tablet_tool_proximity_event *event)
{
    struct cwc_tablet_tool *tabtool = event->tool->data;
    struct cwc_tablet *tablet       = event->tablet->data;

    if (tabtool && event->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
        wlr_tablet_v2_tablet_tool_notify_proximity_out(tabtool->tablet_v2_tool);
        return;
    }

    if (!tabtool)
        tablet_tool_init(tablet, event->tool);

    struct wlr_input_device *device = &event->tablet->base;

    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, device, event->x,
                                         event->y, &lx, &ly);
    double dx = lx - cursor->wlr_cursor->x;
    double dy = ly - cursor->wlr_cursor->y;

    process_cursor_motion(cursor, event->time_msec, device, dx, dy, dx, dy);
}

void process_tablet_tool_tip(struct cwc_cursor *cursor,
                             struct wlr_tablet_tool_tip_event *event)
{
    struct cwc_tablet_tool *tabtool = event->tool->data;
    struct cwc_tablet *tablet       = event->tablet->data;
    struct wlr_cursor *wlr_cursor   = cursor->wlr_cursor;

    double cx = wlr_cursor->x;
    double cy = wlr_cursor->y;
    double sx, sy;
    struct wlr_surface *surface = scene_surface_at(cx, cy, &sx, &sy);

    if (!surface)
        return;

    if (wlr_surface_accepts_tablet_v2(surface, tablet->tablet_v2)) {
        if (event->state == WLR_TABLET_TOOL_TIP_UP) {
            wlr_tablet_v2_tablet_tool_notify_up(tabtool->tablet_v2_tool);
            return;
        } else {
            wlr_tablet_v2_tablet_tool_notify_down(tabtool->tablet_v2_tool);
        }
    } else {
        struct wlr_pointer_button_event e = {
            .state   = (enum wl_pointer_button_state)event->state,
            .button  = BTN_LEFT,
            .pointer = NULL};
        process_cursor_button(cursor, &e);
    }
}

void process_tablet_tool_button(struct cwc_cursor *cursor,
                                struct wlr_tablet_tool_button_event *event)
{
    struct cwc_tablet_tool *tabtool = event->tool->data;
    struct cwc_tablet *tablet       = event->tablet->data;
    struct wlr_cursor *wlr_cursor   = cursor->wlr_cursor;

    double cx = wlr_cursor->x;
    double cy = wlr_cursor->y;
    double sx, sy;

    struct wlr_surface *surface = scene_surface_at(cx, cy, &sx, &sy);
    if (wlr_surface_accepts_tablet_v2(surface, tablet->tablet_v2)) {
        wlr_tablet_v2_tablet_tool_notify_button(
            tabtool->tablet_v2_tool, event->button,
            (enum zwp_tablet_pad_v2_button_state)event->state);
    }
}

static void _cwc_tablet_destroy(struct cwc_tablet *tablet)
{
    wl_list_remove(&tablet->link);
    wl_list_remove(&tablet->destroy_l.link);

    free(tablet);
}

static void on_tablet_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_tablet *tablet = wl_container_of(listener, tablet, destroy_l);

    _cwc_tablet_destroy(tablet);
}

struct cwc_tablet *cwc_tablet_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev)
{
    struct cwc_tablet *tablet = calloc(1, sizeof(*tablet));
    if (!tablet)
        return NULL;

    tablet->seat = seat;
    tablet->tablet_v2 =
        wlr_tablet_create(server.input->tablet_manager, seat->wlr_seat, dev);
    tablet->tablet_v2->wlr_tablet->data = tablet;

    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor,
                                   tablet->tablet_v2->wlr_device);

    tablet->destroy_l.notify = on_tablet_destroy;
    wl_signal_add(&tablet->tablet_v2->wlr_device->events.destroy,
                  &tablet->destroy_l);

    wl_list_insert(&seat->tablet_devs, &tablet->link);

    cwc_log(CWC_DEBUG, "new tablet: %s\n", tablet->tablet_v2->wlr_device->name);
    return tablet;
}

static void on_tpad_button(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_pad_button_event *event = data;
}

static void on_tpad_ring(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_pad_ring_event *event = data;
}

static void on_tpad_strip(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_pad_strip_event *event = data;
}

static void on_tpad_attach_tablet(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_tool *tablet = data;
}

struct cwc_tablet_pad *cwc_tablet_pad_create(struct cwc_seat *seat,
                                             struct wlr_input_device *dev)
{
    struct cwc_tablet_pad *tpad = calloc(1, sizeof(*tpad));
    if (!tpad)
        return NULL;

    tpad->seat       = seat;
    tpad->tablet_pad = wlr_tablet_pad_create(server.input->tablet_manager,
                                             seat->wlr_seat, dev);

    tpad->button_l.notify        = on_tpad_button;
    tpad->ring_l.notify          = on_tpad_ring;
    tpad->strip_l.notify         = on_tpad_strip;
    tpad->attach_tablet_l.notify = on_tpad_attach_tablet;
    wl_signal_add(&tpad->tablet_pad->wlr_pad->events.button, &tpad->button_l);
    wl_signal_add(&tpad->tablet_pad->wlr_pad->events.ring, &tpad->ring_l);
    wl_signal_add(&tpad->tablet_pad->wlr_pad->events.strip, &tpad->strip_l);
    wl_signal_add(&tpad->tablet_pad->wlr_pad->events.attach_tablet,
                  &tpad->attach_tablet_l);

    wl_list_insert(&seat->tablet_pad_devs, &tpad->link);

    return tpad;
}

void cwc_tablet_pad_destroy(struct cwc_tablet_pad *tpad)
{
    wl_list_remove(&tpad->button_l.link);
    wl_list_remove(&tpad->ring_l.link);
    wl_list_remove(&tpad->strip_l.link);
    wl_list_remove(&tpad->attach_tablet_l.link);

    wl_list_remove(&tpad->link);

    free(tpad);
}
