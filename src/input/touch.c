/* touch.c - touch input device
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

#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/seat.h"
#include "cwc/input/touch.h"

struct touch_point {
    struct wl_list link;
    struct cwc_touch *touch;
    int32_t touch_id;
};

void process_touch_down(struct cwc_cursor *cursor,
                        struct wlr_touch_down_event *event)
{
    struct cwc_touch *touch = event->touch->data;
    struct cwc_seat *seat   = touch->seat;

    double lx, ly, sx, sy;
    wlr_cursor_absolute_to_layout_coords(seat->cursor->wlr_cursor,
                                         &touch->wlr_touch->base, event->x,
                                         event->y, &lx, &ly);
    wlr_cursor_warp_absolute(cursor->wlr_cursor, &touch->wlr_touch->base,
                             event->x, event->y);

    struct wlr_surface *surface = scene_surface_at(lx, ly, &sx, &sy);
    if (!surface)
        return;

    struct cwc_toplevel *toplevel = cwc_toplevel_try_from_wlr_surface(surface);
    if (toplevel && !seat->is_down)
        cwc_toplevel_focus(toplevel, true);

    { /* track the touches to decide when the seat down is done in touch_up */
        struct touch_point *t_point = calloc(1, sizeof(*t_point));
        if (!t_point)
            return;

        wl_list_insert(&touch->touches, &t_point->link);
        t_point->touch    = touch;
        t_point->touch_id = event->touch_id;
    }

    if (wlr_surface_accepts_touch(surface, seat->wlr_seat)) {
        wlr_seat_touch_notify_down(touch->seat->wlr_seat, surface,
                                   event->time_msec, event->touch_id, sx, sy);
        cwc_seat_begin_down(seat, surface, lx - sx, ly - sy,
                            CWC_SIMULATE_TOUCH);
    } else {
        sx = lx - cursor->wlr_cursor->x;
        sy = ly - cursor->wlr_cursor->y;
        process_cursor_motion(cursor, event->time_msec, &touch->wlr_touch->base,
                              sx, sy, sx, sy);
        struct wlr_pointer_button_event e = {
            .state   = WL_POINTER_BUTTON_STATE_PRESSED,
            .button  = BTN_LEFT,
            .pointer = NULL};
        process_cursor_button(cursor, &e);
    }
}

static void touch_point_destroy_by_id(struct cwc_touch *touch, int32_t touch_id)
{
    struct touch_point *t_point, *tmp;
    wl_list_for_each_safe(t_point, tmp, &touch->touches, link)
    {
        if (t_point->touch_id != touch_id)
            continue;

        wl_list_remove(&t_point->link);
        free(t_point);
        break;
    }

    if (wl_list_empty(&touch->touches))
        cwc_seat_end_down(touch->seat);
}

void process_touch_up(struct cwc_cursor *cursor,
                      struct wlr_touch_up_event *event)
{
    struct cwc_touch *touch = event->touch->data;

    if (touch->seat->input_simulation == CWC_SIMULATE_TOUCH) {
        wlr_seat_touch_notify_up(cursor->seat, event->time_msec,
                                 event->touch_id);
        stop_interactive(cursor);
    } else {
        struct wlr_pointer_button_event e = {
            .state   = WL_POINTER_BUTTON_STATE_RELEASED,
            .button  = BTN_LEFT,
            .pointer = NULL};
        process_cursor_button(cursor, &e);
    }

    touch_point_destroy_by_id(touch, event->touch_id);
}

void process_touch_motion(struct cwc_cursor *cursor,
                          struct wlr_touch_motion_event *event)
{
    struct cwc_touch *touch      = event->touch->data;
    struct cwc_seat *seat        = touch->seat;
    struct wlr_input_device *dev = &touch->wlr_touch->base;

    double lx, ly, sx, sy;
    wlr_cursor_absolute_to_layout_coords(seat->cursor->wlr_cursor,
                                         &touch->wlr_touch->base, event->x,
                                         event->y, &lx, &ly);

    if (cursor->state != CWC_CURSOR_STATE_NORMAL) {
        double dx = lx - cursor->wlr_cursor->x;
        double dy = ly - cursor->wlr_cursor->y;
        process_cursor_motion(cursor, event->time_msec, dev, dx, dy, dx, dy);
        return;
    }

    if (seat->input_simulation == CWC_SIMULATE_TOUCH) {
        sx = lx - seat->surface_origin_x;
        sy = ly - seat->surface_origin_y;
        wlr_seat_touch_notify_motion(touch->seat->wlr_seat, event->time_msec,
                                     event->touch_id, sx, sy);
    } else {
        double dx = lx - cursor->wlr_cursor->x;
        double dy = ly - cursor->wlr_cursor->y;
        process_cursor_motion(cursor, event->time_msec, dev, dx, dy, dx, dy);
    }
}

void process_touch_cancel(struct cwc_cursor *cursor,
                          struct wlr_touch_cancel_event *event)
{
    struct cwc_touch *touch = event->touch->data;
    struct cwc_seat *seat   = touch->seat;

    touch_point_destroy_by_id(touch, event->touch_id);

    if (!seat->init_surface)
        return;

    struct wl_client *client =
        wl_resource_get_client(seat->init_surface->resource);
    struct wlr_seat_client *seat_client =
        wlr_seat_client_for_wl_client(seat->wlr_seat, client);

    if (seat_client)
        wlr_seat_touch_notify_cancel(seat->wlr_seat, seat_client);
}

void process_touch_frame(struct cwc_cursor *cursor)
{
    struct cwc_seat *seat = cursor->seat->data;

    if (seat->input_simulation == CWC_SIMULATE_TOUCH) {
        wlr_seat_touch_notify_frame(cursor->seat);
        return;
    }

    wlr_seat_pointer_notify_frame(cursor->seat);
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch = wl_container_of(listener, touch, destroy_l);

    cwc_touch_destroy(touch);
}

struct cwc_touch *cwc_touch_create(struct cwc_seat *seat,
                                   struct wlr_input_device *dev)
{
    struct cwc_touch *touch = calloc(1, sizeof(*touch));
    if (!touch)
        return NULL;

    touch->seat            = seat;
    touch->wlr_touch       = wlr_touch_from_input_device(dev);
    touch->wlr_touch->data = touch;

    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor, dev);

    touch->destroy_l.notify = on_destroy;
    wl_signal_add(&dev->events.destroy, &touch->destroy_l);

    wl_list_init(&touch->touches);
    wl_list_insert(&seat->touch_devs, &touch->link);

    return touch;
}

void cwc_touch_destroy(struct cwc_touch *touch)
{
    wl_list_remove(&touch->destroy_l.link);
    wl_list_remove(&touch->link);
    free(touch);
}
