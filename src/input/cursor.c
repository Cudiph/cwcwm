/* cursor.c - cursor/pointer processing
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

#include <drm_fourcc.h>
#include <hyprcursor/hyprcursor.h>
#include <lauxlib.h>
#include <linux/input-event-codes.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/util/region.h>
#include <wlr/xwayland.h>

#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/desktop/transaction.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/layout/bsp.h"
#include "cwc/layout/container.h"
#include "cwc/layout/master.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"

static void process_cursor_move(struct cwc_cursor *cursor)
{
    struct cwc_toplevel *grabbed = cursor->grabbed_toplevel;
    double cx                    = cursor->wlr_cursor->x;
    double cy                    = cursor->wlr_cursor->y;

    double new_x = cx - cursor->grab_x;
    double new_y = cy - cursor->grab_y;
    cwc_container_set_position_global(grabbed->container, new_x, new_y);
}

static struct wlr_box cwc_output_get_snap_geometry(struct cwc_output *output,
                                                   uint32_t edges)
{

    struct wlr_box new_box = output->usable_area;
    new_box.x += output->output_layout_box.x;
    new_box.y += output->output_layout_box.y;

    if (edges & WLR_EDGE_TOP) {
        new_box.height /= 2;
    } else if (edges & WLR_EDGE_BOTTOM) {
        new_box.y += new_box.height / 2;
        new_box.height /= 2;
    }

    if (edges & WLR_EDGE_LEFT) {
        new_box.width /= 2;
    } else if (edges & WLR_EDGE_RIGHT) {
        new_box.x += new_box.width / 2;
        new_box.width /= 2;
    }

    return new_box;
}

static void destroy_snap_overlay(struct cwc_cursor *cursor)
{
    if (cursor->snap_overlay) {
        wlr_scene_node_destroy(&cursor->snap_overlay->node);
        cursor->snap_overlay = NULL;
    }
}

static void process_cursor_move_floating(struct cwc_cursor *cursor)
{
    struct cwc_toplevel *grabbed = cursor->grabbed_toplevel;
    double cx                    = cursor->wlr_cursor->x;
    double cy                    = cursor->wlr_cursor->y;
    struct cwc_output *c_output  = cwc_output_at(server.output_layout, cx, cy);

    double new_x = cx - cursor->grab_x;
    double new_y = cy - cursor->grab_y;
    cwc_container_set_position_global(grabbed->container, new_x, new_y);

    uint32_t snap_edges = get_snap_edges(&c_output->output_layout_box, cx, cy,
                                         g_config.cursor_edge_threshold);
    if (!snap_edges) {
        destroy_snap_overlay(cursor);
        return;
    }

    struct wlr_box overlay_rect =
        cwc_output_get_snap_geometry(c_output, snap_edges);

    if (!cursor->snap_overlay) {
        cursor->snap_overlay = wlr_scene_rect_create(
            server.root.overlay, overlay_rect.width, overlay_rect.height,
            g_config.cursor_edge_snapping_overlay_color);
    } else {
        wlr_scene_rect_set_size(cursor->snap_overlay, overlay_rect.width,
                                overlay_rect.height);
    }

    wlr_scene_node_set_position(&cursor->snap_overlay->node, overlay_rect.x,
                                overlay_rect.y);
}

/* scheduling the resize will prevent the compositor flooding configure request.
 * While it is not a problem in wayland, it is an issue for xwayland windows in
 * my case it's chromium that has the issue.
 */
static inline void schedule_resize(struct cwc_toplevel *toplevel,
                                   struct cwc_cursor *cursor,
                                   struct wlr_box *new_box)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int interval_msec = 8; // default to 120hz
    int refresh_rate  = toplevel->container->output->wlr_output->refresh;
    if (refresh_rate) {
        refresh_rate /= 1000;
        refresh_rate  = MAX(refresh_rate, 1);
        interval_msec = 1000.0 / refresh_rate;
    }

    uint64_t delta_t_msec =
        timespec_to_msec(&now) - cursor->last_resize_time_msec;

    if (delta_t_msec > interval_msec) {
        if (new_box) {
            cwc_container_set_box_global(toplevel->container, new_box);
            clock_gettime(CLOCK_MONOTONIC, &now);
        } else {
            transaction_schedule_tag(
                cwc_output_get_current_tag_info(toplevel->container->output));
        }

        cursor->last_resize_time_msec = timespec_to_msec(&now);
    } else if (new_box) {
        cursor->pending_box = *new_box;
    }
}

static void process_cursor_resize(struct cwc_cursor *cursor)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;
    double cx                     = cursor->wlr_cursor->x;
    double cy                     = cursor->wlr_cursor->y;

    double border_x = cx - cursor->grab_x;
    double border_y = cy - cursor->grab_y;
    int new_left    = cursor->grab_float.x;
    int new_right   = cursor->grab_float.x + cursor->grab_float.width;
    int new_top     = cursor->grab_float.y;
    int new_bottom  = cursor->grab_float.y + cursor->grab_float.height;

    if (cursor->resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if (new_top >= new_bottom)
            new_top = new_bottom - 1;
    } else if (cursor->resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if (new_bottom <= new_top)
            new_bottom = new_top + 1;
    }

    if (cursor->resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if (new_left >= new_right)
            new_left = new_right - 1;
    } else if (cursor->resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if (new_right <= new_left)
            new_right = new_left + 1;
    }

    struct wlr_box new_box = {
        .x      = new_left,
        .y      = new_top,
        .width  = new_right - new_left,
        .height = new_bottom - new_top,
    };

    schedule_resize(toplevel, cursor, &new_box);
}

static void process_cursor_resize_bsp(struct cwc_cursor *cursor)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;
    double cx                     = cursor->wlr_cursor->x;
    double cy                     = cursor->wlr_cursor->y;

    double diff_x = cx - cursor->grab_x;
    double diff_y = cy - cursor->grab_y;

    struct bsp_grab *grab_bsp   = &cursor->grab_bsp;
    struct bsp_node *horizontal = grab_bsp->horizontal;
    struct bsp_node *vertical   = grab_bsp->vertical;

    if (horizontal) {
        double newfact =
            grab_bsp->wfact_horizontal + diff_x / horizontal->width;
        horizontal->left_wfact = CLAMP(newfact, 0.05, 0.95);
    }

    if (vertical) {
        double newfact = grab_bsp->wfact_vertical + diff_y / vertical->height;
        vertical->left_wfact = CLAMP(newfact, 0.05, 0.95);
    }

    schedule_resize(toplevel, cursor, NULL);
}

static void process_cursor_resize_master(struct cwc_cursor *cursor)
{
    struct cwc_output *output = cursor->grabbed_toplevel->container->output;
    master_resize_update(output, cursor);
}

static void cwc_cursor_unhide(struct cwc_cursor *cursor)
{
    if (!cursor->hidden)
        return;

    if (cursor->name_before_hidden) {
        cwc_cursor_set_image_by_name(cursor, cursor->name_before_hidden);
    } else if (cursor->client_surface) {
        cwc_cursor_set_surface(cursor, cursor->client_surface,
                               cursor->hotspot_x, cursor->hotspot_y);
    }

    cursor->hidden = false;
}

static inline void _send_pointer_move_signal(struct cwc_cursor *cursor,
                                             uint32_t time_msec,
                                             struct wlr_input_device *device,
                                             double dx,
                                             double dy,
                                             double dx_unaccel,
                                             double dy_unaccel)
{
    struct cwc_pointer_move_event event = {
        .cursor     = cursor,
        .dx         = dx,
        .dy         = dy,
        .dx_unaccel = dx_unaccel,
        .dy_unaccel = dy_unaccel,
    };
    lua_State *L = g_config_get_lua_State();
    lua_settop(L, 0);
    luaC_object_push(L, cursor);
    lua_pushnumber(L, time_msec);
    lua_pushnumber(L, dx);
    lua_pushnumber(L, dy);
    lua_pushnumber(L, dx_unaccel);
    lua_pushnumber(L, dy_unaccel);
    cwc_signal_emit("pointer::move", &event, L, 6);
}

void process_cursor_motion(struct cwc_cursor *cursor,
                           uint32_t time_msec,
                           struct wlr_input_device *device,
                           double dx,
                           double dy,
                           double dx_unaccel,
                           double dy_unaccel)
{
    struct wlr_seat *wlr_seat     = cursor->seat;
    struct wlr_cursor *wlr_cursor = cursor->wlr_cursor;

    cwc_cursor_unhide(cursor);
    wl_event_source_timer_update(cursor->inactive_timer,
                                 g_config.cursor_inactive_timeout);
    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier, wlr_seat);

    switch (cursor->state) {
    case CWC_CURSOR_STATE_MOVE:
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_move_floating(cursor);
    case CWC_CURSOR_STATE_MOVE_MASTER:
    case CWC_CURSOR_STATE_MOVE_BSP:
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_move(cursor);
    case CWC_CURSOR_STATE_RESIZE:
        // skip synchronization otherwise it'll make resizing sluggish
        server.resize_count = -1e6;
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_resize(cursor);
    case CWC_CURSOR_STATE_RESIZE_BSP:
        server.resize_count = -1e6;
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_resize_bsp(cursor);
    case CWC_CURSOR_STATE_RESIZE_MASTER:
        server.resize_count = -1e6;
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_resize_master(cursor);
    default:
        break;
    }

    double cx = wlr_cursor->x;
    double cy = wlr_cursor->y;
    double sx, sy;
    struct wlr_surface *surface = scene_surface_at(cx, cy, &sx, &sy);
    struct cwc_output *output   = cwc_output_at(server.output_layout, cx, cy);
    struct wlr_pointer_constraint_v1 *surf_constraint =
        wlr_pointer_constraints_v1_constraint_for_surface(
            server.input->pointer_constraints, surface, cursor->seat);

    if (!time_msec) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        time_msec = timespec_to_msec(&now);
        goto notify;
    }

    if (!cursor->send_events)
        goto move;

    wlr_relative_pointer_manager_v1_send_relative_motion(
        server.input->relative_pointer_manager, wlr_seat,
        (uint64_t)time_msec * 1000, dx, dy, dx_unaccel, dy_unaccel);

    if (cursor->last_output != output) {
        lua_State *L = g_config_get_lua_State();
        cwc_object_emit_signal_simple("screen::mouse_enter", L, output);

        if (cursor->last_output)
            cwc_object_emit_signal_simple("screen::mouse_leave", L,
                                          cursor->last_output);

        cursor->last_output = output;
    }

    // sway + dwl implementation in very simplified way, may contain bugs
    if (surf_constraint && device && device->type == WLR_INPUT_DEVICE_POINTER
        && surf_constraint->surface
               == cursor->seat->pointer_state.focused_surface
        && surf_constraint->surface
               == cursor->seat->keyboard_state.focused_surface) {

        double sx_confined, sy_confined;
        if (!wlr_region_confine(&surf_constraint->region, sx, sy, sx + dx,
                                sy + dy, &sx_confined, &sy_confined))
            return;

        if (surf_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
            return;

        dx = sx_confined - sx;
        dy = sy_confined - sy;
    }

notify:
    if (surface) {
        wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(wlr_seat, time_msec, sx, sy);
    } else {
        cwc_cursor_set_image_by_name(cursor, "default");
        wlr_seat_pointer_clear_focus(wlr_seat);
    }

move:
    if (dx || dy)
        wlr_cursor_move(wlr_cursor, device, dx, dy);

    if (cursor->grab)
        _send_pointer_move_signal(cursor, time_msec, device, dx, dy, dx_unaccel,
                                  dy_unaccel);

    cursor->dont_emit_signal = false;
}

static void on_client_side_cursor_destroy(struct wl_listener *listener,
                                          void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, client_side_surface_destroy_l);

    cursor->client_surface = NULL;
    wl_list_remove(&cursor->client_side_surface_destroy_l.link);
    wl_list_init(&cursor->client_side_surface_destroy_l.link);
}

/* client side cursor */
void on_request_set_cursor(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat =
        wl_container_of(listener, seat, request_set_cursor_l);
    struct cwc_cursor *cursor = seat->cursor;

    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client =
        cursor->seat->pointer_state.focused_client;

    if (!focused_client || event->seat_client != focused_client)
        return;

    cwc_cursor_set_surface(cursor, event->surface, event->hotspot_x,
                           event->hotspot_y);
}

static void _notify_mouse_signal(struct wlr_surface *old_surface,
                                 struct wlr_surface *new_surface)
{
    struct cwc_toplevel *old = cwc_toplevel_try_from_wlr_surface(old_surface);
    struct cwc_toplevel *new = cwc_toplevel_try_from_wlr_surface(new_surface);

    lua_State *L = g_config_get_lua_State();
    if (old && cwc_toplevel_is_mapped(old) && !cwc_toplevel_is_unmanaged(old)) {
        cwc_object_emit_signal_simple("client::mouse_leave", L, old);
    }

    if (new && cwc_toplevel_is_mapped(new) && !cwc_toplevel_is_unmanaged(new)) {
        cwc_object_emit_signal_simple("client::mouse_enter", L, new);
    }
}

void on_pointer_focus_change(struct wl_listener *listener, void *data)
{

    struct cwc_seat *seat =
        wl_container_of(listener, seat, pointer_focus_change_l);
    struct cwc_cursor *cursor                         = seat->cursor;
    struct wlr_seat_pointer_focus_change_event *event = data;

    if (event->new_surface == NULL)
        cwc_cursor_set_image_by_name(cursor, "default");

    if (!cursor->dont_emit_signal) {
        _notify_mouse_signal(event->old_surface, event->new_surface);
        cursor->dont_emit_signal = false;
    }
}

/* cursor mouse movement */
static void on_cursor_motion(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_motion_l);

    struct wlr_pointer_motion_event *event = data;

    process_cursor_motion(cursor, event->time_msec, &event->pointer->base,
                          event->delta_x, event->delta_y, event->unaccel_dx,
                          event->unaccel_dy);
}

static void on_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_motion_abs_l);

    struct wlr_pointer_motion_absolute_event *event = data;
    struct wlr_input_device *device                 = &event->pointer->base;

    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, device, event->x,
                                         event->y, &lx, &ly);

    double dx = lx - cursor->wlr_cursor->x;
    double dy = ly - cursor->wlr_cursor->y;

    process_cursor_motion(cursor, event->time_msec, device, dx, dy, dx, dy);
}

static inline void
_send_pointer_axis_signal(struct cwc_cursor *cursor,
                          struct wlr_pointer_axis_event *event)
{
    struct cwc_pointer_axis_event cwc_event = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    lua_settop(L, 0);
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushboolean(L, event->orientation);
    lua_pushnumber(L, event->delta);
    lua_pushnumber(L, event->delta_discrete);
    cwc_signal_emit("pointer::axis", &cwc_event, L, 5);
}

/* true means client shouldn't get notified */
static bool _process_axis_bind(struct cwc_cursor *cursor,
                               struct wlr_pointer_axis_event *event)
{
    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(cursor->seat);
    uint32_t modifiers       = kbd ? wlr_keyboard_get_modifiers(kbd) : 0;

    if (event->source != WL_POINTER_AXIS_SOURCE_WHEEL)
        return false;

    enum cwc_cursor_pseudo_btn button = 0;

    if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (event->delta >= 0)
            button = SCROLL_DOWN;
        else if (event->delta <= 0)
            button = SCROLL_UP;
    } else if (event->orientation == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        if (event->delta >= 0)
            button = SCROLL_LEFT;
        else if (event->delta <= 0)
            button = SCROLL_RIGHT;
    }

    if (!button)
        return false;

    return keybind_mouse_execute(server.main_mouse_kmap, modifiers, button,
                                 true);
}

/* scroll wheel */
static void on_cursor_axis(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_axis_l);

    struct wlr_pointer_axis_event *event = data;
    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier,
                                         cursor->seat);

    if (_process_axis_bind(cursor, event))
        return;

    if (cursor->send_events)
        wlr_seat_pointer_notify_axis(
            cursor->seat, event->time_msec, event->orientation, event->delta,
            event->delta_discrete, event->source, event->relative_direction);

    if (cursor->grab)
        _send_pointer_axis_signal(cursor, event);
}

void start_interactive_move(struct cwc_toplevel *toplevel)
{
    struct cwc_cursor *cursor = server.seat->cursor;
    double cx                 = cursor->wlr_cursor->x;
    double cy                 = cursor->wlr_cursor->y;

    toplevel = toplevel ? toplevel
                        : cwc_toplevel_at_with_deep_check(cx, cy, NULL, NULL);
    if (!toplevel || !cwc_toplevel_can_enter_interactive(toplevel))
        return;

    // set image first before changing the state
    cursor->name_before_interactive = cursor->current_name;
    cwc_cursor_set_image_by_name(cursor, "grabbing");

    if (cwc_toplevel_is_floating(toplevel)) {
        cursor->state = CWC_CURSOR_STATE_MOVE;
    } else {
        struct cwc_tag_info *tag_info =
            cwc_output_get_current_tag_info(toplevel->container->output);
        if (tag_info->layout_mode == CWC_LAYOUT_BSP) {
            if (toplevel->container->bsp_node)
                bsp_remove_container(toplevel->container, true);
            cursor->state = CWC_CURSOR_STATE_MOVE_BSP;
        } else {
            cursor->state = CWC_CURSOR_STATE_MOVE_MASTER;
        }

        struct wlr_box geom = cwc_toplevel_get_geometry(toplevel);

        /* grab the middle point of the toplevel */
        cwc_toplevel_set_position_global(toplevel, cx - geom.width / 2.0,
                                         cy - geom.height / 2.0);
    }

    cursor->grab_x           = cx - toplevel->container->tree->node.x;
    cursor->grab_y           = cy - toplevel->container->tree->node.y;
    cursor->grabbed_toplevel = toplevel;
    toplevel->container->state |= CONTAINER_STATE_MOVING;
}

/* geo_box is wlr_surface box */
static uint32_t
decide_which_edge_to_resize(double sx, double sy, struct wlr_box geo_box)
{
    double nx, ny;
    surface_coord_to_normdevice_coord(geo_box, sx, sy, &nx, &ny);

    // exclusive single edge check
    if (nx >= -0.3 && nx <= 0.3) {
        if (ny <= -0.4)
            return WLR_EDGE_TOP;
        else if (ny >= 0.6)
            return WLR_EDGE_BOTTOM;
    } else if (ny >= -0.3 && ny <= 0.3) {
        if (nx <= -0.4)
            return WLR_EDGE_LEFT;
        else if (nx >= 0.6)
            return WLR_EDGE_RIGHT;
    }

    // corner check
    uint32_t edges = 0;
    if (nx >= -0.05)
        edges |= WLR_EDGE_RIGHT;
    else
        edges |= WLR_EDGE_LEFT;

    if (ny >= -0.05)
        edges |= WLR_EDGE_BOTTOM;
    else
        edges |= WLR_EDGE_TOP;

    return edges;
}

static void start_interactive_resize_floating(struct cwc_cursor *cursor,
                                              uint32_t edges,
                                              double cx,
                                              double cy)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;
    struct wlr_box geo_box        = cwc_container_get_box(toplevel->container);

    cursor->grab_float = geo_box;

    double border_x =
        geo_box.x + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
    double border_y =
        geo_box.y + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
    cursor->grab_x = cx - border_x;
    cursor->grab_y = cy - border_y;

    cursor->state = CWC_CURSOR_STATE_RESIZE;
}

static void start_interactive_resize_bsp(struct cwc_cursor *cursor,
                                         uint32_t edges,
                                         double cx,
                                         double cy)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;

    cursor->grab_x = cx;
    cursor->grab_y = cy;

    struct bsp_node *vertical   = NULL;
    struct bsp_node *horizontal = NULL;
    bsp_find_resize_fence(toplevel->container->bsp_node, cursor->resize_edges,
                          &vertical, &horizontal);

    if (vertical) {
        cursor->grab_bsp.vertical       = vertical;
        cursor->grab_bsp.wfact_vertical = vertical->left_wfact;
    }

    if (horizontal) {
        cursor->grab_bsp.horizontal       = horizontal;
        cursor->grab_bsp.wfact_horizontal = horizontal->left_wfact;
    }

    cursor->state = CWC_CURSOR_STATE_RESIZE_BSP;
}

static void start_interactive_resize_master(struct cwc_cursor *cursor,
                                            uint32_t edges,
                                            double cx,
                                            double cy)
{
    struct cwc_output *output = cursor->grabbed_toplevel->container->output;

    cursor->grab_x = cx;
    cursor->grab_y = cy;

    master_resize_start(output, cursor);

    cursor->state = CWC_CURSOR_STATE_RESIZE_MASTER;
}

void start_interactive_resize(struct cwc_toplevel *toplevel, uint32_t edges)
{
    struct cwc_cursor *cursor = server.seat->cursor;
    double cx                 = cursor->wlr_cursor->x;
    double cy                 = cursor->wlr_cursor->y;

    double sx, sy;
    toplevel =
        toplevel ? toplevel : cwc_toplevel_at_with_deep_check(cx, cy, &sx, &sy);
    if (!toplevel || !cwc_toplevel_can_enter_interactive(toplevel))
        return;

    if (!cwc_toplevel_is_x11(toplevel))
        wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, true);

    struct wlr_box geo_box = cwc_toplevel_get_geometry(toplevel);
    edges = edges ? edges : decide_which_edge_to_resize(sx, sy, geo_box);

    toplevel->container->state |= CONTAINER_STATE_RESIZING;
    cursor->grabbed_toplevel        = toplevel;
    cursor->name_before_interactive = cursor->current_name;
    cursor->resize_edges            = edges;

    cwc_cursor_set_image_by_name(cursor, wlr_xcursor_get_resize_name(edges));

    struct cwc_tag_info *tag_info =
        cwc_output_get_current_tag_info(toplevel->container->output);

    if (cwc_toplevel_is_floating(toplevel)) {
        start_interactive_resize_floating(cursor, edges, cx, cy);
    } else if (tag_info->layout_mode == CWC_LAYOUT_BSP
               && toplevel->container->bsp_node) {
        start_interactive_resize_bsp(cursor, edges, cx, cy);
    } else if (tag_info->layout_mode == CWC_LAYOUT_MASTER) {
        start_interactive_resize_master(cursor, edges, cx, cy);
    }

    // init resize schedule
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    cursor->last_resize_time_msec = timespec_to_msec(&now);
}

static void end_interactive_move_floating(struct cwc_cursor *cursor)
{
    double cx = cursor->wlr_cursor->x;
    double cy = cursor->wlr_cursor->y;

    if (cursor->snap_overlay) {
        destroy_snap_overlay(cursor);
    }

    struct cwc_output *current_output =
        cwc_output_at(server.output_layout, cx, cy);
    if (!current_output)
        return;

    uint32_t snap_edges = get_snap_edges(&current_output->output_layout_box, cx,
                                         cy, g_config.cursor_edge_threshold);

    if (!snap_edges)
        return;

    struct cwc_container *grabbed = cursor->grabbed_toplevel->container;

    struct wlr_box new_box =
        cwc_output_get_snap_geometry(current_output, snap_edges);

    cwc_container_set_box_global_gap(grabbed, &new_box);
}

static void end_interactive_resize_floating(struct cwc_cursor *cursor)
{
    // apply pending change from schedule
    cwc_container_set_box_global(cursor->grabbed_toplevel->container,
                                 &cursor->pending_box);
    cursor->grab_float = (struct wlr_box){0};
}

static void end_interactive_move_master(struct cwc_cursor *cursor)
{
    struct cwc_container *grabbed = cursor->grabbed_toplevel->container;

    // set the grabbed to float so that its filtered out
    grabbed->state |= CONTAINER_STATE_FLOATING;
    struct cwc_toplevel *toplevel_under_cursor =
        cwc_toplevel_at_tiled(cursor->wlr_cursor->x, cursor->wlr_cursor->y);
    grabbed->state &= ~CONTAINER_STATE_FLOATING;

    if (toplevel_under_cursor
        && cwc_toplevel_is_visible(toplevel_under_cursor)) {
        wl_list_swap(&toplevel_under_cursor->container->link_output_container,
                     &grabbed->link_output_container);
        wl_list_swap(&toplevel_under_cursor->container->link, &grabbed->link);
    }

    transaction_schedule_tag(cwc_output_get_current_tag_info(grabbed->output));
}

static void end_interactive_resize_master(struct cwc_cursor *cursor)
{
    struct cwc_output *output = cursor->grabbed_toplevel->container->output;
    master_resize_end(output, cursor);
}

static void end_interactive_move_bsp(struct cwc_cursor *cursor)
{
    struct cwc_container *grabbed = cursor->grabbed_toplevel->container;
    double cx                     = cursor->wlr_cursor->x;
    double cy                     = cursor->wlr_cursor->y;

    grabbed->state |= CONTAINER_STATE_FLOATING;
    struct cwc_toplevel *toplevel_under_cursor = cwc_toplevel_at_tiled(cx, cy);
    grabbed->state &= ~CONTAINER_STATE_FLOATING;

    if (!toplevel_under_cursor || !toplevel_under_cursor->container->bsp_node
        || grabbed->workspace != toplevel_under_cursor->container->workspace) {
        bsp_insert_container(grabbed, grabbed->workspace);
        return;
    }

    struct cwc_tag_info *tag_info = cwc_output_get_current_tag_info(
        toplevel_under_cursor->container->output);

    tag_info->bsp_root_entry.last_focused = toplevel_under_cursor->container;

    struct wlr_box box =
        cwc_container_get_box(toplevel_under_cursor->container);
    enum Position pos = wlr_box_bsp_should_insert_at_position(&box, cx, cy);
    bsp_insert_container_pos(grabbed,
                             toplevel_under_cursor->container->workspace, pos);
}

static void end_interactive_resize_bsp(struct cwc_cursor *cursor)
{
    cursor->grab_bsp = (struct bsp_grab){0};
}

void stop_interactive(struct cwc_cursor *cursor)
{
    if (!cursor)
        cursor = server.seat->cursor;

    if (cursor->state == CWC_CURSOR_STATE_NORMAL)
        return;

    switch (cursor->state) {
    case CWC_CURSOR_STATE_MOVE:
        end_interactive_move_floating(cursor);
        break;
    case CWC_CURSOR_STATE_RESIZE:
        end_interactive_resize_floating(cursor);
        break;
    case CWC_CURSOR_STATE_MOVE_BSP:
        end_interactive_move_bsp(cursor);
        break;
    case CWC_CURSOR_STATE_RESIZE_BSP:
        end_interactive_resize_bsp(cursor);
        break;
    case CWC_CURSOR_STATE_MOVE_MASTER:
        end_interactive_move_master(cursor);
        break;
    case CWC_CURSOR_STATE_RESIZE_MASTER:
        end_interactive_resize_master(cursor);
        break;
    default:
        break;
    }

    // cursor fallback
    cursor->state = CWC_CURSOR_STATE_NORMAL;
    if (cursor->name_before_interactive)
        cwc_cursor_set_image_by_name(cursor, cursor->name_before_interactive);
    else if (cursor->client_surface) {
        cwc_cursor_set_surface(cursor, cursor->client_surface,
                               cursor->hotspot_x, cursor->hotspot_y);
    } else
        cwc_cursor_set_image_by_name(cursor, NULL);

    struct cwc_toplevel **grabbed = &cursor->grabbed_toplevel;

    if (!cwc_toplevel_is_x11(*grabbed))
        wlr_xdg_toplevel_set_resizing((*grabbed)->xdg_toplevel, false);

    (*grabbed)->container->state &= ~CONTAINER_STATE_RESIZING;
    (*grabbed)->container->state &= ~CONTAINER_STATE_MOVING;
    *grabbed = NULL;
}

static inline void
_send_pointer_button_signal(struct cwc_cursor *cursor,
                            struct wlr_pointer_button_event *event,
                            bool press)
{
    struct cwc_pointer_button_event cwc_event = {
        .cursor = cursor,
        .event  = event,
    };
    lua_State *L = g_config_get_lua_State();
    lua_settop(L, 0);
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->button);
    lua_pushboolean(L, event->state);
    cwc_signal_emit("pointer::button", &cwc_event, L, 4);
}

/* mouse click */
static void on_cursor_button(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_button_l);
    struct wlr_pointer_button_event *event = data;

    double cx = cursor->wlr_cursor->x;
    double cy = cursor->wlr_cursor->y;
    double sx, sy;
    struct cwc_toplevel *toplevel = cwc_toplevel_at(cx, cy, &sx, &sy);

    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier,
                                         cursor->seat);

    bool handled = false;
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
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

    if (!handled && cursor->send_events)
        wlr_seat_pointer_notify_button(cursor->seat, event->time_msec,
                                       event->button, event->state);

    if (cursor->grab)
        _send_pointer_button_signal(cursor, event,
                                    WL_POINTER_BUTTON_STATE_PRESSED);
}

/* cursor render */
static void on_cursor_frame(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_frame_l);

    wlr_seat_pointer_notify_frame(cursor->seat);
}

static inline void
_send_pointer_swipe_begin_signal(struct cwc_cursor *cursor,
                                 struct wlr_pointer_swipe_begin_event *event)
{
    struct cwc_pointer_swipe_begin_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->fingers);

    cwc_signal_emit("pointer::swipe::begin", &signal_data, L, 3);
}

static void on_swipe_begin(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, swipe_begin_l);
    struct wlr_pointer_swipe_begin_event *event = data;

    _send_pointer_swipe_begin_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_swipe_begin(server.input->pointer_gestures,
                                                 cursor->seat, event->time_msec,
                                                 event->fingers);
}

static inline void
_send_pointer_swipe_update_signal(struct cwc_cursor *cursor,
                                  struct wlr_pointer_swipe_update_event *event)
{
    struct cwc_pointer_swipe_update_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->fingers);
    lua_pushnumber(L, event->dx);
    lua_pushnumber(L, event->dy);

    cwc_signal_emit("pointer::swipe::update", &signal_data, L, 5);
}

static void on_swipe_update(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, swipe_update_l);
    struct wlr_pointer_swipe_update_event *event = data;

    _send_pointer_swipe_update_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_swipe_update(
            server.input->pointer_gestures, cursor->seat, event->time_msec,
            event->dx, event->dy);
}

static inline void
_send_pointer_swipe_end_signal(struct cwc_cursor *cursor,
                               struct wlr_pointer_swipe_end_event *event)
{
    struct cwc_pointer_swipe_end_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushboolean(L, event->cancelled);

    cwc_signal_emit("pointer::swipe::end", &signal_data, L, 3);
}

static void on_swipe_end(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor = wl_container_of(listener, cursor, swipe_end_l);
    struct wlr_pointer_swipe_end_event *event = data;

    _send_pointer_swipe_end_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_swipe_end(server.input->pointer_gestures,
                                               cursor->seat, event->time_msec,
                                               event->cancelled);
}

static inline void
_send_pointer_pinch_begin_signal(struct cwc_cursor *cursor,
                                 struct wlr_pointer_pinch_begin_event *event)
{
    struct cwc_pointer_pinch_begin_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->fingers);

    cwc_signal_emit("pointer::pinch::begin", &signal_data, L, 3);
}

static void on_pinch_begin(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, pinch_begin_l);
    struct wlr_pointer_pinch_begin_event *event = data;

    _send_pointer_pinch_begin_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_pinch_begin(server.input->pointer_gestures,
                                                 cursor->seat, event->time_msec,
                                                 event->fingers);
}

static inline void
_send_pointer_pinch_update_signal(struct cwc_cursor *cursor,
                                  struct wlr_pointer_pinch_update_event *event)
{
    struct cwc_pointer_pinch_update_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->fingers);
    lua_pushnumber(L, event->dx);
    lua_pushnumber(L, event->dy);
    lua_pushnumber(L, event->scale);
    lua_pushnumber(L, event->rotation);

    cwc_signal_emit("pointer::pinch::update", &signal_data, L, 7);
}

static void on_pinch_update(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, pinch_update_l);
    struct wlr_pointer_pinch_update_event *event = data;

    _send_pointer_pinch_update_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_pinch_update(
            server.input->pointer_gestures, cursor->seat, event->time_msec,
            event->dx, event->dy, event->scale, event->rotation);
}

static inline void
_send_pointer_pinch_end_signal(struct cwc_cursor *cursor,
                               struct wlr_pointer_pinch_end_event *event)
{
    struct cwc_pointer_pinch_end_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushboolean(L, event->cancelled);

    cwc_signal_emit("pointer::pinch::end", &signal_data, L, 3);
}

static void on_pinch_end(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor = wl_container_of(listener, cursor, pinch_end_l);
    struct wlr_pointer_pinch_end_event *event = data;

    _send_pointer_pinch_end_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_pinch_end(server.input->pointer_gestures,
                                               cursor->seat, event->time_msec,
                                               event->cancelled);
}

static inline void
_send_pointer_hold_begin_signal(struct cwc_cursor *cursor,
                                struct wlr_pointer_hold_begin_event *event)
{
    struct cwc_pointer_hold_begin_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, event->fingers);

    cwc_signal_emit("pointer::hold::begin", &signal_data, L, 3);
}

static void on_hold_begin(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor = wl_container_of(listener, cursor, hold_begin_l);
    struct wlr_pointer_hold_begin_event *event = data;

    _send_pointer_hold_begin_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_hold_begin(server.input->pointer_gestures,
                                                cursor->seat, event->time_msec,
                                                event->fingers);
}

static inline void
_send_pointer_hold_end_signal(struct cwc_cursor *cursor,
                              struct wlr_pointer_hold_end_event *event)
{
    struct cwc_pointer_hold_end_event signal_data = {
        .cursor = cursor,
        .event  = event,
    };

    lua_State *L = g_config_get_lua_State();
    luaC_object_push(L, cursor);
    lua_pushnumber(L, event->time_msec);
    lua_pushboolean(L, event->cancelled);

    cwc_signal_emit("pointer::hold::end", &signal_data, L, 3);
}

static void on_hold_end(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor = wl_container_of(listener, cursor, hold_end_l);
    struct wlr_pointer_hold_end_event *event = data;

    _send_pointer_hold_end_signal(cursor, event);

    if (cursor->send_events)
        wlr_pointer_gestures_v1_send_hold_end(server.input->pointer_gestures,
                                              cursor->seat, event->time_msec,
                                              event->cancelled);
}

/* stuff for creating wlr_buffer from cair surface mainly from hypcursor */

static void cairo_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
    struct hyprcursor_buffer *buffer =
        wl_container_of(wlr_buffer, buffer, base);

    wlr_buffer_finish(&buffer->base);
    free(buffer);
    // the cairo surface is managed by hyprcursor manager no need to free the
    // cairo surface
}

static bool cairo_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
                                               uint32_t flags,
                                               void **data,
                                               uint32_t *format,
                                               size_t *stride)
{
    struct hyprcursor_buffer *buffer =
        wl_container_of(wlr_buffer, buffer, base);

    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
        return false;

    *format = DRM_FORMAT_ARGB8888;
    *data   = cairo_image_surface_get_data(buffer->surface);
    *stride = cairo_image_surface_get_stride(buffer->surface);
    return true;
}

static void cairo_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
    // this space is intentionally left blank
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
    .destroy               = cairo_buffer_destroy,
    .begin_data_ptr_access = cairo_buffer_begin_data_ptr_access,
    .end_data_ptr_access   = cairo_buffer_end_data_ptr_access};

/* hyprcursor cursor animation (pre-independent hyprland) */
static int animation_loop(void *data)
{
    struct cwc_cursor *cursor = data;

    size_t i = ++cursor->frame_index;
    if (i >= cursor->images_count) {
        i = cursor->frame_index = 0;
    }

    struct hyprcursor_buffer **buffer_array = cursor->cursor_buffers.data;

    wlr_cursor_set_buffer(cursor->wlr_cursor, &buffer_array[i]->base,
                          cursor->images[i]->hotspotX / cursor->scale,
                          cursor->images[i]->hotspotY / cursor->scale,
                          cursor->scale);

    wl_event_source_timer_update(cursor->animation_timer,
                                 cursor->images[i]->delay);
    return 1;
}

static int cursor_inactive_hide_cursor(void *data)
{
    struct cwc_cursor *cursor  = data;
    cursor->name_before_hidden = cursor->current_name;
    cursor->hidden             = true;
    cwc_cursor_hide_cursor(cursor);
    return 1;
}

static void hyprcursor_logger(enum eHyprcursorLogLevel level, char *message)
{
    enum wlr_log_importance wlr_level = WLR_DEBUG;
    switch (level) {
    case HC_LOG_NONE:
        wlr_level = WLR_SILENT;
        break;
    case HC_LOG_TRACE:
    case HC_LOG_INFO:
        wlr_level = WLR_DEBUG;
        break;
    case HC_LOG_WARN:
    case HC_LOG_ERR:
    case HC_LOG_CRITICAL:
        wlr_level = WLR_ERROR;
        break;
    }

    wlr_log(wlr_level, "[hyprcursor] %s", message);
}

static void on_config_commit(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, config_commit_l);
    struct cwc_config *old_config = data;

    if (old_config->cursor_size == g_config.cursor_size)
        return;

    struct hyprcursor_cursor_style_info newstyle = {.size =
                                                        g_config.cursor_size};
    wlr_xcursor_manager_destroy(cursor->xcursor_mgr);
    cursor->xcursor_mgr = wlr_xcursor_manager_create(NULL, newstyle.size);
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, "default");
    cwc_cursor_hyprcursor_change_style(cursor, newstyle);
    cwc_cursor_hide_cursor(cursor);
    cwc_cursor_set_image_by_name(cursor, "default");

    char size[7];
    snprintf(size, 6, "%u", newstyle.size);
    setenv("XCURSOR_SIZE", size, true);
}

struct cwc_cursor *cwc_cursor_create(struct wlr_seat *seat)
{
    struct cwc_cursor *cursor = calloc(1, sizeof(*cursor));
    if (cursor == NULL) {
        cwc_log(CWC_ERROR, "failed to allocate cwc_cursor");
        return NULL;
    }

    // bases
    cursor->seat             = seat;
    cursor->wlr_cursor       = wlr_cursor_create();
    cursor->wlr_cursor->data = cursor;
    cursor->info.size        = g_config.cursor_size;
    cursor->hyprcursor_mgr =
        hyprcursor_manager_create_with_logger(NULL, hyprcursor_logger);
    cursor->scale       = 1.0f;
    cursor->state       = CWC_CURSOR_STATE_NORMAL;
    cursor->send_events = true;

    // set_xcursor must after creating manager to load the theme
    cursor->xcursor_mgr = wlr_xcursor_manager_create(NULL, cursor->info.size);
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, "default");

    // timer
    cursor->animation_timer =
        wl_event_loop_add_timer(server.wl_event_loop, animation_loop, cursor);
    cursor->inactive_timer = wl_event_loop_add_timer(
        server.wl_event_loop, cursor_inactive_hide_cursor, cursor);

    // event listeners
    cursor->cursor_motion_l.notify     = on_cursor_motion;
    cursor->cursor_motion_abs_l.notify = on_cursor_motion_absolute;
    cursor->cursor_axis_l.notify       = on_cursor_axis;
    cursor->cursor_button_l.notify     = on_cursor_button;
    cursor->cursor_frame_l.notify      = on_cursor_frame;
    wl_signal_add(&cursor->wlr_cursor->events.motion, &cursor->cursor_motion_l);
    wl_signal_add(&cursor->wlr_cursor->events.motion_absolute,
                  &cursor->cursor_motion_abs_l);
    wl_signal_add(&cursor->wlr_cursor->events.axis, &cursor->cursor_axis_l);
    wl_signal_add(&cursor->wlr_cursor->events.button, &cursor->cursor_button_l);
    wl_signal_add(&cursor->wlr_cursor->events.frame, &cursor->cursor_frame_l);

    cursor->swipe_begin_l.notify  = on_swipe_begin;
    cursor->swipe_update_l.notify = on_swipe_update;
    cursor->swipe_end_l.notify    = on_swipe_end;
    wl_signal_add(&cursor->wlr_cursor->events.swipe_begin,
                  &cursor->swipe_begin_l);
    wl_signal_add(&cursor->wlr_cursor->events.swipe_update,
                  &cursor->swipe_update_l);
    wl_signal_add(&cursor->wlr_cursor->events.swipe_end, &cursor->swipe_end_l);

    cursor->pinch_begin_l.notify  = on_pinch_begin;
    cursor->pinch_update_l.notify = on_pinch_update;
    cursor->pinch_end_l.notify    = on_pinch_end;
    wl_signal_add(&cursor->wlr_cursor->events.pinch_begin,
                  &cursor->pinch_begin_l);
    wl_signal_add(&cursor->wlr_cursor->events.pinch_update,
                  &cursor->pinch_update_l);
    wl_signal_add(&cursor->wlr_cursor->events.pinch_end, &cursor->pinch_end_l);

    cursor->hold_begin_l.notify = on_hold_begin;
    cursor->hold_end_l.notify   = on_hold_end;
    wl_signal_add(&cursor->wlr_cursor->events.hold_begin,
                  &cursor->hold_begin_l);
    wl_signal_add(&cursor->wlr_cursor->events.hold_end, &cursor->hold_end_l);

    cursor->config_commit_l.notify = on_config_commit;
    wl_signal_add(&g_config.events.commit, &cursor->config_commit_l);

    cursor->client_side_surface_destroy_l.notify =
        on_client_side_cursor_destroy;
    wl_list_init(&cursor->client_side_surface_destroy_l.link);

    wlr_cursor_attach_output_layout(cursor->wlr_cursor, server.output_layout);
    cwc_cursor_update_scale(cursor);
    cwc_cursor_hyprcursor_change_style(cursor, cursor->info);

    char size[7];
    snprintf(size, sizeof(size) - 1, "%u", cursor->info.size);
    setenv("XCURSOR_SIZE", size, true);

    lua_State *L = g_config_get_lua_State();
    luaC_object_pointer_register(L, cursor);

    return cursor;
}

static void hyprcursor_buffer_fini(struct cwc_cursor *cursor);

void cwc_cursor_destroy(struct cwc_cursor *cursor)
{
    lua_State *L = g_config_get_lua_State();
    luaC_object_unregister(L, cursor);

    // clean hyprcursor leftover
    if (cursor->images != NULL)
        hyprcursor_cursor_image_data_free(cursor->images, cursor->images_count);
    hyprcursor_buffer_fini(cursor);

    hyprcursor_style_done(cursor->hyprcursor_mgr, cursor->info);
    hyprcursor_manager_free(cursor->hyprcursor_mgr);
    wlr_xcursor_manager_destroy(cursor->xcursor_mgr);

    wl_event_source_remove(cursor->animation_timer);

    wl_list_remove(&cursor->cursor_motion_l.link);
    wl_list_remove(&cursor->cursor_motion_abs_l.link);
    wl_list_remove(&cursor->cursor_axis_l.link);
    wl_list_remove(&cursor->cursor_button_l.link);
    wl_list_remove(&cursor->cursor_frame_l.link);

    wl_list_remove(&cursor->swipe_begin_l.link);
    wl_list_remove(&cursor->swipe_update_l.link);
    wl_list_remove(&cursor->swipe_end_l.link);

    wl_list_remove(&cursor->pinch_begin_l.link);
    wl_list_remove(&cursor->pinch_update_l.link);
    wl_list_remove(&cursor->pinch_end_l.link);

    wl_list_remove(&cursor->hold_begin_l.link);
    wl_list_remove(&cursor->hold_end_l.link);

    wl_list_remove(&cursor->config_commit_l.link);

    wlr_cursor_destroy(cursor->wlr_cursor);
    free(cursor);
}

/* load hyprcursor buffer */
static void hyprcursor_buffer_init(struct cwc_cursor *cursor)
{
    wl_array_init(&cursor->cursor_buffers);
    for (int i = 0; i < cursor->images_count; ++i) {
        hyprcursor_cursor_image_data *image_data = cursor->images[i];
        struct hyprcursor_buffer *buffer         = calloc(1, sizeof(*buffer));
        if (buffer == NULL) {
            cwc_log(CWC_ERROR, "failed to allocate hyprcursor_buffer");
            return;
        }
        buffer->surface = image_data->surface;
        wlr_buffer_init(&buffer->base, &cairo_buffer_impl, image_data->size,
                        image_data->size);

        struct hyprcursor_buffer **buffer_array =
            wl_array_add(&cursor->cursor_buffers, sizeof(&image_data));
        *buffer_array = buffer;
    }
}

/* free hyprcursor buffer */
static void hyprcursor_buffer_fini(struct cwc_cursor *cursor)
{
    if (cursor->cursor_buffers.size == 0)
        return;

    struct hyprcursor_buffer **buffer_array = cursor->cursor_buffers.data;

    int len = cursor->cursor_buffers.size / sizeof(buffer_array);
    for (int i = 0; i < len; i++) {
        wlr_buffer_drop(&buffer_array[i]->base);
    }
    wl_array_release(&cursor->cursor_buffers);
    cursor->cursor_buffers.size = 0;
}

void cwc_cursor_set_image_by_name(struct cwc_cursor *cursor, const char *name)
{
    if (cursor->state != CWC_CURSOR_STATE_NORMAL)
        return;

    if (name == NULL) {
        cwc_cursor_hide_cursor(cursor);
        return;
    }

    if (cursor->current_name != NULL && strcmp(cursor->current_name, name) == 0)
        return;

    cursor->current_name = name;

    hyprcursor_buffer_fini(cursor);

    // xcursor fallback
    if (!hyprcursor_manager_valid(cursor->hyprcursor_mgr)) {
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, name);
        return;
    }

    // free prev images
    if (cursor->images != NULL)
        hyprcursor_cursor_image_data_free(cursor->images, cursor->images_count);

    cursor->images = hyprcursor_get_cursor_image_data(
        cursor->hyprcursor_mgr, name, cursor->info, &cursor->images_count);

    // xcursor fallback
    if (!cursor->images_count) {
        hyprcursor_cursor_image_data_free(cursor->images, cursor->images_count);
        cursor->images = NULL;
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, name);
        return;
    }

    // cache buffer
    hyprcursor_buffer_init(cursor);

    struct hyprcursor_buffer **buffer_array = cursor->cursor_buffers.data;

    wlr_cursor_set_buffer(cursor->wlr_cursor, &buffer_array[0]->base,
                          cursor->images[0]->hotspotX / cursor->scale,
                          cursor->images[0]->hotspotY / cursor->scale,
                          cursor->scale);

    if (cursor->images_count > 1) {
        cursor->frame_index = 0;
        wl_event_source_timer_update(cursor->animation_timer,
                                     cursor->images[0]->delay);
    } else {
        wl_event_source_timer_update(cursor->animation_timer, 0);
    }
}

void cwc_cursor_set_surface(struct cwc_cursor *cursor,
                            struct wlr_surface *surface,
                            int32_t hotspot_x,
                            int32_t hotspot_y)
{
    if (cursor->state != CWC_CURSOR_STATE_NORMAL)
        return;

    cursor->client_surface = surface;
    cursor->hotspot_x      = hotspot_x;
    cursor->hotspot_y      = hotspot_y;
    wl_list_remove(&cursor->client_side_surface_destroy_l.link);
    if (surface) {
        wl_signal_add(&surface->events.destroy,
                      &cursor->client_side_surface_destroy_l);
    } else {
        wl_list_init(&cursor->client_side_surface_destroy_l.link);
    }
    cursor->current_name = NULL;
    wlr_cursor_set_surface(cursor->wlr_cursor, surface, hotspot_x, hotspot_y);
}

void cwc_cursor_hide_cursor(struct cwc_cursor *cursor)
{
    if (cursor->state != CWC_CURSOR_STATE_NORMAL)
        return;

    cursor->current_name = NULL;
    wlr_cursor_unset_image(cursor->wlr_cursor);
}

void cwc_cursor_update_scale(struct cwc_cursor *cursor)
{
    cursor->scale = 1.0f;
    struct cwc_output *output;
    wl_list_for_each(output, &server.outputs, link)
    {
        if (cursor->scale < output->wlr_output->scale)
            cursor->scale = output->wlr_output->scale;
    }

    if (cursor->info.size != g_config.cursor_size * cursor->scale) {
        struct hyprcursor_cursor_style_info new = {.size = g_config.cursor_size
                                                           * cursor->scale};
        cwc_cursor_hyprcursor_change_style(cursor, new);
    }

    /* reset old buffer and update to new style */
    const char *cursor_before = cursor->current_name;
    cwc_cursor_hide_cursor(cursor);
    cwc_cursor_set_image_by_name(cursor, cursor_before);
}

bool cwc_cursor_hyprcursor_change_style(
    struct cwc_cursor *cursor, struct hyprcursor_cursor_style_info info)
{
    if (!hyprcursor_manager_valid(cursor->hyprcursor_mgr))
        return false;

    // force reset image
    cursor->current_name = NULL;

    hyprcursor_buffer_fini(cursor);
    hyprcursor_style_done(cursor->hyprcursor_mgr, cursor->info);

    info.size = g_config.cursor_size * cursor->scale;

    if (hyprcursor_manager_valid(cursor->hyprcursor_mgr)
        && hyprcursor_load_theme_style(cursor->hyprcursor_mgr, info)) {
        cursor->info      = info;
        cursor->info.size = info.size;
        return true;
    }

    return false;
}

/* set shape protocol */
static void on_request_set_shape(struct wl_listener *listener, void *data)
{
    struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
    struct cwc_seat *seat = event->seat_client->seat->data;
    struct wlr_seat_client *focused_client =
        seat->wlr_seat->pointer_state.focused_client;

    if (!focused_client || event->seat_client != focused_client)
        return;

    cwc_cursor_set_image_by_name(seat->cursor,
                                 wlr_cursor_shape_v1_name(event->shape));
}

static void warp_to_cursor_hint(struct cwc_cursor *cursor,
                                struct wlr_pointer_constraint_v1 *constraint)
{
    if (cursor->seat->pointer_state.focused_surface != constraint->surface)
        return;

    double sx = constraint->current.cursor_hint.x;
    double sy = constraint->current.cursor_hint.y;
    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(constraint->surface);

    if (!toplevel || !constraint->current.cursor_hint.enabled)
        return;

    struct wlr_scene_node *node = &toplevel->container->tree->node;
    int bw                      = toplevel->container->border.thickness;
    wlr_cursor_warp(cursor->wlr_cursor, NULL, sx + node->x + bw,
                    sy + node->y + bw);
    wlr_seat_pointer_warp(cursor->seat, sx, sy);
}

static void on_constraint_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_pointer_constraint *constraint =
        wl_container_of(listener, constraint, destroy_l);
    struct cwc_cursor *cursor = constraint->cursor;
    cwc_log(CWC_DEBUG, "destroying pointer constraint: %p", constraint);

    // warp back to initial position
    warp_to_cursor_hint(cursor, constraint->constraint);

    wl_list_remove(&constraint->destroy_l.link);
    free(constraint);
}

/* cut down version of sway implementation */
static void on_new_pointer_constraint(struct wl_listener *listener, void *data)
{
    struct wlr_pointer_constraint_v1 *wlr_constraint = data;

    struct cwc_pointer_constraint *constraint = calloc(1, sizeof(*constraint));
    constraint->constraint                    = wlr_constraint;
    constraint->cursor =
        ((struct cwc_seat *)wlr_constraint->seat->data)->cursor;
    constraint->destroy_l.notify = on_constraint_destroy;
    wl_signal_add(&wlr_constraint->events.destroy, &constraint->destroy_l);
    struct cwc_cursor *cursor = constraint->cursor;

    cwc_log(CWC_DEBUG, "new pointer constraint: %p", constraint);

    if (wlr_constraint == NULL)
        warp_to_cursor_hint(cursor, wlr_constraint);

    wlr_pointer_constraint_v1_send_activated(wlr_constraint);
}

static void on_new_vpointer(struct wl_listener *listener, void *data)
{
    struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
    struct cwc_seat *seat =
        event->suggested_seat ? event->suggested_seat->data : server.seat;

    cwc_log(WLR_DEBUG, "new virtual pointer (%s): %p", seat->wlr_seat->name,
            event);

    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor,
                                   &event->new_pointer->pointer.base);

    if (event->suggested_output)
        wlr_cursor_map_to_output(seat->cursor->wlr_cursor,
                                 event->suggested_output);
}

void setup_pointer(struct cwc_input_manager *input_mgr)
{
    // constraint
    input_mgr->pointer_constraints =
        wlr_pointer_constraints_v1_create(server.wl_display);
    input_mgr->new_pointer_constraint_l.notify = on_new_pointer_constraint;
    wl_signal_add(&input_mgr->pointer_constraints->events.new_constraint,
                  &input_mgr->new_pointer_constraint_l);

    // virtual pointer
    input_mgr->virtual_pointer_manager =
        wlr_virtual_pointer_manager_v1_create(server.wl_display);
    input_mgr->new_vpointer_l.notify = on_new_vpointer;
    wl_signal_add(
        &input_mgr->virtual_pointer_manager->events.new_virtual_pointer,
        &input_mgr->new_vpointer_l);

    // cursor shape
    input_mgr->cursor_shape_manager =
        wlr_cursor_shape_manager_v1_create(server.wl_display, 1);
    input_mgr->request_set_shape_l.notify = on_request_set_shape;
    wl_signal_add(&input_mgr->cursor_shape_manager->events.request_set_shape,
                  &input_mgr->request_set_shape_l);

    // pointer gestures
    input_mgr->pointer_gestures =
        wlr_pointer_gestures_v1_create(server.wl_display);
}

void cleanup_pointer(struct cwc_input_manager *input_mgr)
{
    wl_list_remove(&input_mgr->new_pointer_constraint_l.link);

    wl_list_remove(&input_mgr->new_vpointer_l.link);

    wl_list_remove(&input_mgr->request_set_shape_l.link);
}
