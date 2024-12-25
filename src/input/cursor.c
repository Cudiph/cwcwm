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

/** Low-level API to manage pointer and pointer device
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.pointer
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
#include <wayland-util.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
#include <wlr/util/region.h>
#include <wlr/xwayland.h>

#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
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
        timespec_to_msec(now) - cursor->last_resize_time_msec;

    if (delta_t_msec > interval_msec) {
        // don't use toplevel_set_container_pos because it'll double configure
        // and cause flicking
        wlr_scene_node_set_position(&toplevel->container->tree->node,
                                    new_box->x, new_box->y);
        cwc_toplevel_set_size_surface(toplevel, new_box->width,
                                      new_box->height);
        clock_gettime(CLOCK_MONOTONIC, &now);
        cursor->last_resize_time_msec = timespec_to_msec(now);
    }

    cursor->pending_box = *new_box;
}

static void process_cursor_resize(struct cwc_cursor *cursor)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;
    double cx                     = cursor->wlr_cursor->x;
    double cy                     = cursor->wlr_cursor->y;

    double border_x = cx - cursor->grab_x;
    double border_y = cy - cursor->grab_y;
    int new_left    = cursor->grab_geobox.x;
    int new_right   = cursor->grab_geobox.x + cursor->grab_geobox.width;
    int new_top     = cursor->grab_geobox.y;
    int new_bottom  = cursor->grab_geobox.y + cursor->grab_geobox.height;

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

    struct wlr_box geo_box = cwc_toplevel_get_geometry(toplevel);

    struct wlr_box new_box = {
        .x      = new_left - geo_box.x,
        .y      = new_top - geo_box.y,
        .width  = new_right - new_left,
        .height = new_bottom - new_top,
    };

    schedule_resize(toplevel, cursor, &new_box);
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
    struct wlr_pointer_constraint_v1 *active_constraint =
        cursor->active_constraint;

    switch (cursor->state) {
    case CWC_CURSOR_STATE_MOVE:
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_move(cursor);
    case CWC_CURSOR_STATE_RESIZE:
        wlr_cursor_move(wlr_cursor, device, dx, dy);
        return process_cursor_resize(cursor);
    default:
        break;
    }

    if (!time_msec) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        time_msec = timespec_to_msec(now);
    }

    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier, wlr_seat);
    wlr_relative_pointer_manager_v1_send_relative_motion(
        server.relative_pointer_manager, wlr_seat, (uint64_t)time_msec * 1000,
        dx, dy, dx_unaccel, dy_unaccel);

    double cx = wlr_cursor->x;
    double cy = wlr_cursor->y;
    double sx, sy;
    struct wlr_surface *surface = scene_surface_at(cx, cy, &sx, &sy);
    struct cwc_output *output   = cwc_output_at(server.output_layout, cx, cy);

    if (output)
        server.focused_output = output;

    // sway + dwl implementation in very simplified way, may contain bugs
    if (active_constraint && device
        && device->type == WLR_INPUT_DEVICE_POINTER) {
        if (active_constraint->surface != surface)
            return;

        double sx_confined, sy_confined;
        if (!wlr_region_confine(&cursor->active_constraint->region, sx, sy,
                                sx + dx, sy + dy, &sx_confined, &sy_confined))
            return;

        if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
            return;

        dx = sx_confined - sx;
        dy = sy_confined - sy;
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(wlr_seat, time_msec, sx, sy);
    } else {
        cwc_cursor_set_image_by_name(cursor, "default");
        wlr_seat_pointer_clear_focus(wlr_seat);
    }

    wlr_cursor_move(wlr_cursor, device, dx, dy);
    cursor->dont_emit_signal = false;
}

/* client side cursor */
static void on_request_set_cursor(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, seat_request_cursor_l);

    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client =
        cursor->seat->pointer_state.focused_client;

    if (event->seat_client == focused_client)
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

    if (new &&cwc_toplevel_is_mapped(new) && !cwc_toplevel_is_unmanaged(new)) {
        cwc_object_emit_signal_simple("client::mouse_enter", L, new);
    }
}

static void on_pointer_focus_change(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, seat_pointer_focus_change_l);
    struct wlr_seat_pointer_focus_change_event *event = data;

    if (cursor->active_constraint
        && cursor->active_constraint->surface != event->new_surface) {
        wlr_pointer_constraint_v1_send_deactivated(cursor->active_constraint);
        cursor->active_constraint = NULL;
    }

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

/* scroll wheel */
static void on_cursor_axis(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_axis_l);

    struct wlr_pointer_axis_event *event = data;
    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier,
                                         cursor->seat);

    wlr_seat_pointer_notify_axis(
        cursor->seat, event->time_msec, event->orientation, event->delta,
        event->delta_discrete, event->source, event->relative_direction);
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

    cursor->grab_x                  = cx - toplevel->container->tree->node.x;
    cursor->grab_y                  = cy - toplevel->container->tree->node.y;
    cursor->grabbed_toplevel        = toplevel;
    cursor->name_before_interactive = cursor->current_name;

    // set image first before change the state
    cwc_cursor_set_image_by_name(cursor, "grabbing");
    cursor->state = CWC_CURSOR_STATE_MOVE;
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

    cursor->grabbed_toplevel        = toplevel;
    cursor->name_before_interactive = cursor->current_name;

    double border_x = (toplevel->container->tree->node.x + geo_box.x)
                      + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
    double border_y = (toplevel->container->tree->node.y + geo_box.y)
                      + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
    cursor->grab_x = cx - border_x;
    cursor->grab_y = cy - border_y;

    cursor->grab_geobox = geo_box;
    cursor->grab_geobox.x += toplevel->container->tree->node.x;
    cursor->grab_geobox.y += toplevel->container->tree->node.y;

    cursor->resize_edges = edges;

    cwc_cursor_set_image_by_name(cursor, wlr_xcursor_get_resize_name(edges));
    cursor->state = CWC_CURSOR_STATE_RESIZE;

    // init resize schedule
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    cursor->last_resize_time_msec = timespec_to_msec(now);
}

void stop_interactive()
{
    struct cwc_cursor *cursor = server.seat->cursor;
    if (cursor->state == CWC_CURSOR_STATE_NORMAL)
        return;

    // apply pending change from schedule
    if (cursor->state == CWC_CURSOR_STATE_RESIZE) {
        struct wlr_box pending = cursor->pending_box;
        cwc_container_set_position_global(cursor->grabbed_toplevel->container,
                                          pending.x, pending.y);
        cwc_toplevel_set_size_surface(cursor->grabbed_toplevel, pending.width,
                                      pending.height);
    }

    // cursor fallback
    cursor->state = CWC_CURSOR_STATE_NORMAL;
    if (cursor->name_before_interactive)
        cwc_cursor_set_image_by_name(cursor, cursor->name_before_interactive);
    else
        cwc_cursor_set_image_by_name(cursor, "default");

    struct cwc_toplevel **grabbed = &server.seat->cursor->grabbed_toplevel;

    if (!cwc_toplevel_is_x11(*grabbed))
        wlr_xdg_toplevel_set_resizing((*grabbed)->xdg_toplevel, false);

    *grabbed = NULL;
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
        server.focused_output = cwc_output_at(server.output_layout, cx, cy);

        if (toplevel)
            cwc_toplevel_focus(toplevel, false);

        struct wlr_keyboard *kbd = wlr_seat_get_keyboard(cursor->seat);
        uint32_t modifiers       = kbd ? wlr_keyboard_get_modifiers(kbd) : 0;

        handled |= keybind_mouse_execute(modifiers, event->button, true);

    } else {
        struct wlr_keyboard *kbd = wlr_seat_get_keyboard(cursor->seat);
        uint32_t modifiers       = kbd ? wlr_keyboard_get_modifiers(kbd) : 0;

        stop_interactive();

        // same as keyboard binding always pass release button to client
        keybind_mouse_execute(modifiers, event->button, false);
    }

    // don't notify when either state is have keybind to prevent
    // half state which when the key is pressed but never released
    if (!handled)
        wlr_seat_pointer_notify_button(cursor->seat, event->time_msec,
                                       event->button, event->state);
}

/* cursor render */
static void on_cursor_frame(struct wl_listener *listener, void *data)
{
    struct cwc_cursor *cursor =
        wl_container_of(listener, cursor, cursor_frame_l);

    wlr_seat_pointer_notify_frame(cursor->seat);
}

/* stuff for creating wlr_buffer from cair surface mainly from hypcursor */

static void cairo_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
    struct hyprcursor_buffer *buffer =
        wl_container_of(wlr_buffer, buffer, base);

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
                          cursor->images[i]->hotspotX,
                          cursor->images[i]->hotspotY, 1.0f);

    wl_event_source_timer_update(cursor->animation_timer,
                                 cursor->images[i]->delay);
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

    cursor->info.size = g_config.cursor_size;
    wlr_xcursor_manager_destroy(cursor->xcursor_mgr);
    cursor->xcursor_mgr = wlr_xcursor_manager_create(NULL, cursor->info.size);
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, "default");
    cwc_cursor_hyprcursor_change_style(cursor, cursor->info);

    char size[7];
    snprintf(size, 6, "%u", cursor->info.size);
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
    cursor->xcursor_mgr = wlr_xcursor_manager_create(NULL, cursor->info.size);
    cursor->hyprcursor_mgr =
        hyprcursor_manager_create_with_logger(NULL, hyprcursor_logger);
    cursor->state = CWC_CURSOR_STATE_NORMAL;

    // timer
    cursor->animation_timer =
        wl_event_loop_add_timer(server.wl_event_loop, animation_loop, cursor);

    // event listeners
    cursor->seat_request_cursor_l.notify       = on_request_set_cursor;
    cursor->seat_pointer_focus_change_l.notify = on_pointer_focus_change;
    wl_signal_add(&cursor->seat->events.request_set_cursor,
                  &cursor->seat_request_cursor_l);
    wl_signal_add(&cursor->seat->pointer_state.events.focus_change,
                  &cursor->seat_pointer_focus_change_l);

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

    cursor->config_commit_l.notify = on_config_commit;
    wl_signal_add(&g_config.events.commit, &cursor->config_commit_l);

    wlr_cursor_attach_output_layout(cursor->wlr_cursor, server.output_layout);
    // let xcursor theme load first for xwayland (must before change style)
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_mgr, "default");
    cwc_cursor_hyprcursor_change_style(cursor, cursor->info);

    char size[7];
    snprintf(size, 6, "%u", cursor->info.size);
    setenv("XCURSOR_SIZE", size, true);

    return cursor;
}

static void hyprcursor_buffer_fini(struct cwc_cursor *cursor);

void cwc_cursor_destroy(struct cwc_cursor *cursor)
{
    // clean hyprcursor leftover
    if (cursor->images != NULL)
        hyprcursor_cursor_image_data_free(cursor->images, cursor->images_count);
    hyprcursor_buffer_fini(cursor);

    hyprcursor_style_done(cursor->hyprcursor_mgr, cursor->info);
    hyprcursor_manager_free(cursor->hyprcursor_mgr);
    wlr_cursor_destroy(cursor->wlr_cursor);
    wlr_xcursor_manager_destroy(cursor->xcursor_mgr);

    wl_event_source_remove(cursor->animation_timer);

    wl_list_remove(&cursor->seat_request_cursor_l.link);
    wl_list_remove(&cursor->seat_pointer_focus_change_l.link);

    wl_list_remove(&cursor->cursor_motion_l.link);
    wl_list_remove(&cursor->cursor_motion_abs_l.link);
    wl_list_remove(&cursor->cursor_axis_l.link);
    wl_list_remove(&cursor->cursor_button_l.link);
    wl_list_remove(&cursor->cursor_frame_l.link);

    wl_list_remove(&cursor->config_commit_l.link);

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

    if (cursor->cursor_buffers.size)
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
                          cursor->images[0]->hotspotX,
                          cursor->images[0]->hotspotY, 1.0f);

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

    cursor->current_name = NULL;
    wlr_cursor_set_surface(cursor->wlr_cursor, surface, hotspot_x, hotspot_y);
}

void cwc_cursor_hide_cursor(struct cwc_cursor *cursor)
{
    cwc_cursor_set_surface(cursor, NULL, 0, 0);
}

bool cwc_cursor_hyprcursor_change_style(
    struct cwc_cursor *cursor, struct hyprcursor_cursor_style_info info)
{
    if (!hyprcursor_manager_valid(cursor->hyprcursor_mgr))
        return false;

    // force reset image
    cursor->current_name = NULL;

    hyprcursor_style_done(cursor->hyprcursor_mgr, cursor->info);

    if (hyprcursor_load_theme_style(cursor->hyprcursor_mgr, info)) {
        cursor->info = info;
        return true;
    }

    return false;
}

/* set shape protocol */
static void on_request_set_shape(struct wl_listener *listener, void *data)
{
    struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
    cwc_cursor_set_image_by_name(server.seat->cursor,
                                 wlr_cursor_shape_v1_name(event->shape));
}

static void on_shape_manager_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_cursor_shape_manager *mgr =
        wl_container_of(listener, mgr, destroy_l);

    wl_list_remove(&mgr->request_set_shape_l.link);
    wl_list_remove(&mgr->destroy_l.link);
    mgr->manager                      = NULL;
    mgr->server->cursor_shape_manager = NULL;

    free(mgr);
}

void setup_cursor_shape_manager(struct cwc_server *s) {}

static void warp_to_cursor_hint(struct cwc_cursor *cursor)
{
    struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;
    double sx = constraint->current.cursor_hint.x;
    double sy = constraint->current.cursor_hint.y;
    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(cursor->active_constraint->surface);

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
    if (cursor->active_constraint == constraint->constraint) {
        warp_to_cursor_hint(cursor);
        cursor->active_constraint = NULL;
    }

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

    // sway implementation
    if (cursor->active_constraint == wlr_constraint)
        return;

    if (cursor->active_constraint) {
        if (wlr_constraint == NULL)
            warp_to_cursor_hint(cursor);

        wlr_pointer_constraint_v1_send_deactivated(cursor->active_constraint);
    }

    cursor->active_constraint = wlr_constraint;
    wlr_pointer_constraint_v1_send_activated(wlr_constraint);
}

static void on_new_vpointer(struct wl_listener *listener, void *data)
{
    struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
    struct cwc_seat *seat =
        event->suggested_seat ? event->suggested_seat->data : server.seat;

    cwc_log(WLR_DEBUG, "new virtual pointer: %p", event);

    wlr_cursor_attach_input_device(seat->cursor->wlr_cursor,
                                   &event->new_pointer->pointer.base);

    if (event->suggested_output)
        wlr_cursor_map_to_output(seat->cursor->wlr_cursor,
                                 event->suggested_output);
}

void setup_pointer(struct cwc_server *s)
{
    // constraint
    s->pointer_constraints = wlr_pointer_constraints_v1_create(s->wl_display);
    s->new_pointer_constraint_l.notify = on_new_pointer_constraint;
    wl_signal_add(&s->pointer_constraints->events.new_constraint,
                  &s->new_pointer_constraint_l);

    // virtual pointer
    s->virtual_pointer_manager =
        wlr_virtual_pointer_manager_v1_create(s->wl_display);
    s->new_vpointer_l.notify = on_new_vpointer;
    wl_signal_add(&s->virtual_pointer_manager->events.new_virtual_pointer,
                  &s->new_vpointer_l);

    // cursor shape
    struct cwc_cursor_shape_manager *mgr = calloc(1, sizeof(*mgr));
    s->cursor_shape_manager              = mgr;
    mgr->server                          = s;
    mgr->manager = wlr_cursor_shape_manager_v1_create(s->wl_display, 1);
    mgr->request_set_shape_l.notify = on_request_set_shape;
    mgr->destroy_l.notify           = on_shape_manager_destroy;
    wl_signal_add(&mgr->manager->events.request_set_shape,
                  &mgr->request_set_shape_l);
    wl_signal_add(&mgr->manager->events.destroy, &mgr->destroy_l);
}

//============= LUA ===============

/** Register a mouse binding.
 *
 * @staticfct bind
 * @tparam table|number modifier Table of modifier or modifier bitfield
 * @tparam number mouse_btn Button from linux input-event-codes
 * @tparam func on_press Function to execute when pressed
 * @tparam[opt] func on_release Function to execute when released
 * @tparam[opt] table data Additional data
 * @tparam[opt] string data.group Keybinding group
 * @tparam[opt] string data.description Keybinding description
 * @noreturn
 * @see cuteful.enum.modifier
 * @see cuteful.enum.mouse_btn
 * @see cwc.kbd.bind
 */
static int luaC_pointer_bind(lua_State *L)
{
    uint32_t button = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    uint32_t modifiers = 0;
    if (lua_istable(L, 1)) {
        int len = lua_objlen(L, 1);

        for (int i = 0; i < len; ++i) {
            lua_rawgeti(L, 1, i + 1);
            modifiers |= luaL_checkint(L, -1);
        }

    } else if (lua_isnumber(L, 1)) {
        modifiers = lua_tonumber(L, 1);
    } else {
        luaL_error(L,
                   "modifiers only accept array of number or modifier bitmask");
    }

    bool on_press_is_function   = lua_isfunction(L, 3);
    bool on_release_is_function = lua_isfunction(L, 4);

    if (!on_press_is_function && !on_release_is_function) {
        luaL_error(L, "callback function is not provided");
        return 0;
    }

    struct cwc_keybind_info info = {0};
    info.type                    = CWC_KEYBIND_TYPE_LUA;

    if (on_press_is_function) {
        lua_pushvalue(L, 3);
        info.luaref_press = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    if (on_release_is_function) {
        lua_pushvalue(L, 4);
        info.luaref_release = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    keybind_mouse_register(modifiers, button, info);

    return 0;
}

/** Clear all mouse binding.
 *
 * @staticfct clear
 * @noreturn
 */
static int luaC_pointer_clear(lua_State *L)
{
    keybind_mouse_clear();
    return 0;
}

/** Get main seat pointer position.
 *
 * @staticfct get_position
 * @treturn table Pointer coords with structure {x,y}
 */
static int luaC_pointer_get_position(lua_State *L)
{
    double x = server.seat->cursor->wlr_cursor->x;
    double y = server.seat->cursor->wlr_cursor->y;

    lua_createtable(L, 0, 2);
    lua_pushnumber(L, x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, y);
    lua_setfield(L, -2, "y");

    return 1;
}

/** Set main seat pointer position.
 *
 * @staticfct set_position
 * @noreturn
 */
static int luaC_pointer_set_position(lua_State *L)
{
    int x = luaL_checkint(L, 1);
    int y = luaL_checkint(L, 2);

    wlr_cursor_warp(server.seat->cursor->wlr_cursor, NULL, x, y);

    return 1;
}

/** Start interactive move for client under the cursor.
 *
 * @staticfct move_interactive
 * @noreturn
 */
static int luaC_pointer_move_interactive(lua_State *L)
{
    start_interactive_move(NULL);
    return 0;
}

/** Start interactive resize for client under the cursor.
 *
 * @staticfct resize_interactive
 * @noreturn
 */
static int luaC_pointer_resize_interactive(lua_State *L)
{
    start_interactive_resize(NULL, 0);
    return 0;
}

/** Stop interactive mode.
 *
 * @staticfct stop_interactive
 * @noreturn
 */
static int luaC_pointer_stop_interactive(lua_State *L)
{
    stop_interactive();
    return 0;
}

/** Set cursor size.
 *
 * @configfct set_cursor_size
 * @tparam integer size Cursor size
 * @noreturn
 */
static int luaC_pointer_set_cursor_size(lua_State *L)
{
    int size = luaL_checkint(L, 1);

    g_config.cursor_size = size;
    return 0;
}

/** Set mouse sensitivity.
 *
 * @configfct set_sensitivity
 * @tparam number sensitivity Number in range [-1, 1]
 * @noreturn
 */
static int luaC_pointer_set_sensitivity(lua_State *L)
{
    double sens          = luaL_checknumber(L, 1);
    g_config.sensitivity = sens;

    return 0;
}

/**
 * @configfct set_scroll_method.
 * @tparam integer enum `SCROLL_XXX` enum
 * @noreturn
 * @see cuteful.enum.pointer
 */
static int luaC_pointer_set_scroll_method(lua_State *L)
{
    int scroll_method      = luaL_checkint(L, 1);
    g_config.scroll_method = scroll_method;

    return 0;
}

/** Set click method.
 *
 * @configfct set_click_method
 * @tparam integer enum `CLICK_METHOD_XXX` enum
 * @noreturn
 * @see cuteful.enum.pointer
 */
static int luaC_pointer_set_click_method(lua_State *L)
{
    int click_method      = luaL_checkint(L, 1);
    g_config.click_method = click_method;

    return 0;
}

/** Set send events method.
 *
 * @configfct set_send_events_mode
 * @tparam integer enum `SEND_EVENTS_XXX` enum
 * @noreturn
 * @see cuteful.enum.pointer
 */
static int luaC_pointer_set_send_events_method(lua_State *L)
{
    int send_events_mode      = luaL_checkint(L, 1);
    g_config.send_events_mode = send_events_mode;

    return 0;
}

/** Set acceleration profile.
 *
 * @configfct set_accel_profile
 * @tparam integer enum `ACCEL_PROFILE_XXX` enum
 * @noreturn
 * @see cuteful.enum.pointer
 */
static int luaC_pointer_set_accel_profile(lua_State *L)
{
    int accel_profile      = luaL_checkint(L, 1);
    g_config.accel_profile = accel_profile;

    return 0;
}

/** Set tap button map.
 *
 * @configfct set_tap_button_map
 * @tparam integer enum `TAP_MAP_XXX` enum
 * @noreturn
 * @see cuteful.enum.pointer
 */
static int luaC_pointer_set_tap_button_map(lua_State *L)
{
    int tap_button_map      = luaL_checkint(L, 1);
    g_config.tap_button_map = tap_button_map;

    return 0;
}

/** (Trackpad) Enable tap to click.
 *
 * @configfct set_tap_to_click
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_tap_to_click(lua_State *L)
{
    bool set                = lua_toboolean(L, 1);
    g_config.tap_button_map = set;

    return 0;
}

/** (Trackpad) Enable tap and drag.
 *
 * @configfct set_tap_and_drag
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_tap_and_drag(lua_State *L)
{
    bool set              = lua_toboolean(L, 1);
    g_config.tap_and_drag = set;

    return 0;
}

/** (Trackpad) Enable drag lock.
 *
 * @configfct set_drag_lock
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_drag_lock(lua_State *L)
{
    bool set           = lua_toboolean(L, 1);
    g_config.drag_lock = set;

    return 0;
}

/** (Trackpad) Enable natural scrolling.
 *
 * @configfct set_natural_scrolling
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_natural_scrolling(lua_State *L)
{
    bool set                   = lua_toboolean(L, 1);
    g_config.natural_scrolling = set;

    return 0;
}

/** (Trackpad) Enable disable while typing.
 *
 * @configfct set_disable_while_typing
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_disable_while_typing(lua_State *L)
{
    bool set                      = lua_toboolean(L, 1);
    g_config.disable_while_typing = set;

    return 0;
}

/** (Trackpad) Enable left handed
 *
 * @configfct set_left_handed
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_left_handed(lua_State *L)
{
    bool set             = lua_toboolean(L, 1);
    g_config.left_handed = set;

    return 0;
}

/** (Trackpad) Enable middle button emulation.
 *
 * @configfct set_middle_button_emulation
 * @tparam boolean set
 * @noreturn
 */
static int luaC_pointer_set_middle_btn_emulation(lua_State *L)
{
    bool set                         = lua_toboolean(L, 1);
    g_config.middle_button_emulation = set;

    return 0;
}

void luaC_pointer_setup(lua_State *L)
{
    luaL_Reg pointer_staticlibs[] = {
        {"bind",                        luaC_pointer_bind                    },
        {"clear",                       luaC_pointer_clear                   },

        {"get_position",                luaC_pointer_get_position            },
        {"set_position",                luaC_pointer_set_position            },

        {"move_interactive",            luaC_pointer_move_interactive        },
        {"resize_interactive",          luaC_pointer_resize_interactive      },
        {"stop_interactive",            luaC_pointer_stop_interactive        },

        {"set_cursor_size",             luaC_pointer_set_cursor_size         },
        {"set_sensitivity",             luaC_pointer_set_sensitivity         },
        {"set_scroll_method",           luaC_pointer_set_scroll_method       },
        {"set_click_method",            luaC_pointer_set_click_method        },
        {"set_send_events_mode",        luaC_pointer_set_send_events_method  },
        {"set_accel_profile",           luaC_pointer_set_accel_profile       },
        {"set_tap_button_map",          luaC_pointer_set_tap_button_map      },

        {"set_tap_to_click",            luaC_pointer_set_tap_to_click        },
        {"set_tap_and_drag",            luaC_pointer_set_tap_and_drag        },
        {"set_drag_lock",               luaC_pointer_set_drag_lock           },
        {"set_natural_scrolling",       luaC_pointer_set_natural_scrolling   },
        {"set_disable_while_typing",    luaC_pointer_set_disable_while_typing},
        {"set_left_handed",             luaC_pointer_set_left_handed         },
        {"set_middle_button_emulation", luaC_pointer_set_middle_btn_emulation},

        {NULL,                          NULL                                 },
    };

    lua_newtable(L);
    luaL_register(L, NULL, pointer_staticlibs);
    lua_setfield(L, -2, "pointer");
}
