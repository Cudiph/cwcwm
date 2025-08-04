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

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_cursor.h>

#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/input/tablet.h"
#include "cwc/input/cursor.h"
#include "cwc/server.h"


#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/luaclass.h"
#include "cwc/signal.h"

static void on_axis(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_tool_axis_event *event = data;
    struct cwc_tablet *tablet = wl_container_of(listener, tablet, axis_l);
   
    struct wlr_input_device *device                 = &event->tablet->base;
    if(event->updated_axes & WLR_TABLET_TOOL_AXIS_X || event->updated_axes & WLR_TABLET_TOOL_AXIS_Y){
        double lx, ly;
        struct cwc_cursor *cursor = server.seat->cursor;
        wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, device, event->x,
                                             event->y, &lx, &ly);

        double dx = lx - cursor->wlr_cursor->x;
        double dy = ly - cursor->wlr_cursor->y;

        process_cursor_motion(cursor, event->time_msec, device, dx, dy, dx, dy);
    }
}

static void on_proximity(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_tool_proximity_event *event = data;
}

static void on_tip(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_tool_tip_event *event = data;

}

static void on_button(struct wl_listener *listener, void *data)
{
    struct wlr_tablet_tool_button_event *event = data;

    struct cwc_tablet *tablet = wl_container_of(listener, tablet, button_l);
    struct cwc_cursor *cursor = server.seat->cursor;

    double cx = cursor->wlr_cursor->x;
    double cy = cursor->wlr_cursor->y;
    double sx, sy;
    struct cwc_toplevel *toplevel = cwc_toplevel_at(cx, cy, &sx, &sy);

    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier,
                                         cursor->seat);

    bool handled = false;
    if (event->state == WLR_BUTTON_PRESSED) {
        struct cwc_output *new_output =
            cwc_output_at(server.output_layout, cx, cy);
        if (new_output)
            cwc_output_focus(new_output);

        if (toplevel)
            cwc_toplevel_focus(toplevel, false);

        struct wlr_keyboard *kbd = wlr_seat_get_keyboard(cursor->seat);
        uint32_t modifiers       = kbd ? wlr_keyboard_get_modifiers(kbd) : 0;

        handled |= keybind_mouse_execute(server.main_mouse_kmap, modifiers,
                                         event->button, true);

    } else {
        struct wlr_keyboard *kbd = wlr_seat_get_keyboard(cursor->seat);
        uint32_t modifiers       = kbd ? wlr_keyboard_get_modifiers(kbd) : 0;

        stop_interactive(cursor);

        // same as keyboard binding always pass release button to client
        keybind_mouse_execute(server.main_mouse_kmap, modifiers, event->button,
                              false);
    }

    // don't notify when either state is have keybind to prevent
    // half state which when the key is pressed but never released
    if (!handled)
        wlr_seat_pointer_notify_button(cursor->seat, event->time_msec,
                                       event->button, event->state);
}
static void on_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_tablet *tablet = wl_container_of(listener, tablet, destroy_l);

    cwc_tablet_destroy(tablet);
}

struct cwc_tablet *cwc_tablet_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev)
{
    struct cwc_tablet *tablet = calloc(1, sizeof(*tablet));
    if (!tablet)
        return NULL;

    tablet->seat = seat;
    tablet->tablet =
        wlr_tablet_create(server.input->tablet_manager, seat->wlr_seat, dev);

    tablet->axis_l.notify      = on_axis;
    tablet->proximity_l.notify = on_proximity;
    tablet->tip_l.notify       = on_tip;
    tablet->button_l.notify    = on_button;
    wl_signal_add(&tablet->tablet->wlr_tablet->events.axis, &tablet->axis_l);
    wl_signal_add(&tablet->tablet->wlr_tablet->events.proximity,
                  &tablet->proximity_l);
    wl_signal_add(&tablet->tablet->wlr_tablet->events.tip, &tablet->tip_l);
    wl_signal_add(&tablet->tablet->wlr_tablet->events.button,
                  &tablet->button_l);

    tablet->destroy_l.notify = on_destroy;
    wl_signal_add(&dev->events.destroy, &tablet->destroy_l);

    wl_list_insert(&seat->tablet_devs, &tablet->link);

    return tablet;
}

void cwc_tablet_destroy(struct cwc_tablet *tablet)
{
    wl_list_remove(&tablet->axis_l.link);
    wl_list_remove(&tablet->proximity_l.link);
    wl_list_remove(&tablet->tip_l.link);
    wl_list_remove(&tablet->button_l.link);


    wl_list_remove(&tablet->destroy_l.link);
    wl_list_remove(&tablet->link);

    free(tablet);
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
