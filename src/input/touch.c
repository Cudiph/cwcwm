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

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_touch.h>

#include "cwc/desktop/toplevel.h"
#include "cwc/input/seat.h"
#include "cwc/input/touch.h"

static void on_down(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch = wl_container_of(listener, touch, down_l);
    struct wlr_touch_down_event *event = data;

    double sx, sy;
    struct wlr_surface *surface =
        scene_surface_at(event->x, event->y, &sx, &sy);

    if (surface)
        wlr_seat_touch_notify_down(touch->seat->wlr_seat, surface,
                                   event->time_msec, event->touch_id, sx, sy);
}

static void on_up(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch          = wl_container_of(listener, touch, up_l);
    struct wlr_touch_up_event *event = data;

    wlr_seat_touch_notify_up(touch->seat->wlr_seat, event->time_msec,
                             event->touch_id);
}

static void on_motion(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch = wl_container_of(listener, touch, motion_l);
    struct wlr_touch_motion_event *event = data;

    double sx, sy;
    struct wlr_surface *surface =
        scene_surface_at(event->x, event->y, &sx, &sy);

    if (surface)
        wlr_seat_touch_notify_motion(touch->seat->wlr_seat, event->time_msec,
                                     event->touch_id, sx, sy);
}

static void on_cancel(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch = wl_container_of(listener, touch, cancel_l);
    struct wlr_touch_cancel_event *event = data;

    // wlr_seat_touch_notify_cancel();
}

static void on_frame(struct wl_listener *listener, void *data)
{
    struct cwc_touch *touch = wl_container_of(listener, touch, frame_l);
    wlr_seat_touch_notify_frame(touch->seat->wlr_seat);
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

    touch->down_l.notify   = on_down;
    touch->up_l.notify     = on_up;
    touch->motion_l.notify = on_motion;
    touch->cancel_l.notify = on_cancel;
    touch->frame_l.notify  = on_frame;
    wl_signal_add(&touch->wlr_touch->events.down, &touch->down_l);
    wl_signal_add(&touch->wlr_touch->events.up, &touch->up_l);
    wl_signal_add(&touch->wlr_touch->events.motion, &touch->motion_l);
    wl_signal_add(&touch->wlr_touch->events.cancel, &touch->cancel_l);
    wl_signal_add(&touch->wlr_touch->events.frame, &touch->frame_l);

    touch->destroy_l.notify = on_destroy;
    wl_signal_add(&dev->events.destroy, &touch->destroy_l);

    wl_list_insert(&seat->touch_devs, &touch->link);

    return touch;
}

void cwc_touch_destroy(struct cwc_touch *touch)
{
    wl_list_remove(&touch->up_l.link);
    wl_list_remove(&touch->down_l.link);
    wl_list_remove(&touch->motion_l.link);
    wl_list_remove(&touch->cancel_l.link);
    wl_list_remove(&touch->frame_l.link);

    wl_list_remove(&touch->destroy_l.link);

    wl_list_remove(&touch->link);

    free(touch);
}
