/* container.c - container management
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

#include <cairo.h>
#include <drm_fourcc.h>
#include <math.h>
#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/seat.h"
#include "cwc/layout/bsp.h"
#include "cwc/layout/container.h"
#include "cwc/layout/master.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/types.h"
#include "cwc/util.h"
#include "wlr/util/box.h"

static void cairo_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
    struct border_buffer *border = wl_container_of(wlr_buffer, border, base);
    wlr_buffer_finish(&border->base);
    cairo_surface_destroy(border->surface);
}

static bool cairo_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
                                               uint32_t flags,
                                               void **data,
                                               uint32_t *format,
                                               size_t *stride)
{
    struct border_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
        return false;

    *format = DRM_FORMAT_ARGB8888;
    *data   = cairo_image_surface_get_data(buffer->surface);
    *stride = cairo_image_surface_get_stride(buffer->surface);
    return true;
}

static void cairo_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
    ;
}

static const struct wlr_buffer_impl cairo_border_impl = {
    .destroy               = cairo_buffer_destroy,
    .begin_data_ptr_access = cairo_buffer_begin_data_ptr_access,
    .end_data_ptr_access   = cairo_buffer_end_data_ptr_access,
};

/* the 0 degree is at the left side with clockwise rotation */
static void
find_start_coord(int degree_rot, double radius, double *x, double *y)
{
    // adjust so that it start from the left
    degree_rot += 360 - 45;
    degree_rot %= 360;

    double full_width = radius * 2;

    *x = 0;
    *y = 0;

    int mod45deg = degree_rot % 45;
    int diff     = tan(mod45deg * M_PI / 180) * radius;

    if (degree_rot < 90) {
        *x = diff;
        if (degree_rot >= 45)
            *x += radius;
    } else if (degree_rot < 180) {
        *x = full_width;
        *y = diff;
        if (degree_rot >= 135)
            *y += radius;
    } else if (degree_rot < 270) {
        *y = full_width;
        *x = full_width;
        *x -= diff;
        if (degree_rot >= 225)
            *x -= radius;
    } else {
        *y = full_width;
        *y -= diff;
        if (degree_rot >= 315)
            *y -= radius;
    }
}

static cairo_pattern_t *process_pattern(cairo_pattern_t *reference_pattern,
                                        int rotation,
                                        int bw,
                                        int bh,
                                        int full_w,
                                        int full_h,
                                        enum wlr_direction dir)

{
    double center_x = full_w / 2.0;
    double center_y = full_h / 2.0;

    // we create new pattern so that the gradient can be resized
    cairo_pattern_t *pattern;
    int max_width = MAX(full_w, full_h);
    int min_width = MIN(full_w, full_h);

    double start_x = 0;
    double start_y = 0;
    find_start_coord(rotation, min_width / 2.0, &start_x, &start_y);

    // the end is the reflection of the start point with the center of the
    // square is the origin
    double end_x = min_width - start_x;
    double end_y = min_width - start_y;

    switch (cairo_pattern_get_type(reference_pattern)) {
    case CAIRO_PATTERN_TYPE_LINEAR:
        pattern = cairo_pattern_create_linear(start_x, start_y, end_x, end_y);
        break;
    case CAIRO_PATTERN_TYPE_RADIAL:
        pattern = cairo_pattern_create_radial(center_x, center_y, 0, center_x,
                                              center_y, max_width);
        break;
    default:
        return cairo_pattern_reference(reference_pattern);
    }

    int stop_count;
    switch (cairo_pattern_get_type(pattern)) {
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_LINEAR:
        cairo_pattern_get_color_stop_count(reference_pattern, &stop_count);
        for (int i = 0; i < stop_count; i++) {
            double offset, r, g, b, a;
            cairo_pattern_get_color_stop_rgba(reference_pattern, i, &offset, &r,
                                              &g, &b, &a);
            cairo_pattern_add_color_stop_rgba(pattern, offset, r, g, b, a);
        }
        break;
    default:
        break;
    }

    cairo_matrix_t matrix;
    cairo_matrix_init_scale(&matrix, (double)min_width / full_w,
                            (double)min_width / full_h);

    // translate pattern matrix to their border position
    switch (dir) {
    case WLR_DIRECTION_RIGHT:
        cairo_matrix_translate(&matrix, full_w - bw, bw);
        break;
    case WLR_DIRECTION_DOWN:
        cairo_matrix_translate(&matrix, 0, full_h - bh);
        break;
    case WLR_DIRECTION_LEFT:
        cairo_matrix_translate(&matrix, 0, bw);
        break;
    default:
        break;
    }
    cairo_pattern_set_matrix(pattern, &matrix);

    return pattern;
}

static inline void _draw_border(cairo_surface_t *cr_surf,
                                cairo_pattern_t *pattern,
                                int bw,
                                int bh,
                                enum wlr_direction dir)
{
    cairo_t *cr   = cairo_create(cr_surf);
    double radius = MIN(bw, bh);

    // border radius
    switch (dir) {
    case WLR_DIRECTION_UP:
        cairo_new_sub_path(cr);
        cairo_arc(cr, bh, bh, radius, M_PI, M_PI + M_PI / 2);
        cairo_arc(cr, bw - bh, bh, radius, -M_PI / 2, 0);
        cairo_close_path(cr);
        break;
    case WLR_DIRECTION_DOWN:
        cairo_new_sub_path(cr);
        cairo_arc(cr, bh, 0, radius, M_PI / 2, M_PI);
        cairo_arc(cr, bw - bh, 0, radius, 0, M_PI / 2);
        cairo_close_path(cr);
        break;
    default:
        cairo_rectangle(cr, 0, 0, bw, bh);
        break;
    }

    cairo_set_source(cr, pattern);
    cairo_fill(cr);

    cairo_destroy(cr);
}

static void draw_border(struct border_buffer **border,
                        cairo_pattern_t *pattern,
                        int rotation,
                        int bw,
                        int bh,
                        int full_w,
                        int full_h,
                        enum wlr_direction dir)
{
    struct border_buffer *bb = *border = calloc(1, sizeof(**border));

    wlr_buffer_init(&bb->base, &cairo_border_impl, bw, bh);
    bb->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bw, bh);

    if (cairo_surface_status(bb->surface) != CAIRO_STATUS_SUCCESS)
        return;

    cairo_pattern_t *processed_pattern =
        process_pattern(pattern, rotation, bw, bh, full_w, full_h, dir);
    _draw_border(bb->surface, processed_pattern, bw, bh, dir);
    cairo_pattern_destroy(processed_pattern);
}

static void border_buffer_init(struct cwc_border *border,
                               cairo_pattern_t *pattern,
                               int w,
                               int h,
                               int thickness)
{
    // clockwise top to left
    draw_border(&border->buffer[0], pattern, border->pattern_rotation, w,
                thickness, w, h, WLR_DIRECTION_UP);
    draw_border(&border->buffer[1], pattern, border->pattern_rotation,
                thickness, h - thickness * 2, w, h, WLR_DIRECTION_RIGHT);
    draw_border(&border->buffer[2], pattern, border->pattern_rotation, w,
                thickness, w, h, WLR_DIRECTION_DOWN);
    draw_border(&border->buffer[3], pattern, border->pattern_rotation,
                thickness, h - thickness * 2, w, h, WLR_DIRECTION_LEFT);
}

static bool is_border_valid(struct cwc_border *border)
{
    for (int i = 0; i < 4; ++i) {
        if (border->buffer[i] == NULL)
            return false;
    }

    return true;
}

static void border_buffer_fini(struct cwc_border *border)
{
    if (!is_border_valid(border))
        return;

    for (int i = 0; i < 4; i++) {
        wlr_scene_node_destroy(&border->buffer[i]->scene->node);
        wlr_buffer_drop(&border->buffer[i]->base);
        free(border->buffer[i]);
        border->buffer[i] = NULL;
    }
}

static void border_buffer_redraw(struct cwc_border *border)
{
    border_buffer_fini(border);
    border_buffer_init(border, border->pattern, border->width, border->height,
                       cwc_border_get_thickness(border));

    if (border->attached_tree)
        cwc_border_attach_to_scene(border, border->attached_tree);

    cwc_border_set_enabled(border, border->enabled);
}

void cwc_border_init(struct cwc_border *border,
                     cairo_pattern_t *pattern,
                     int pattern_rotation,
                     int rect_w,
                     int rect_h,
                     int thickness)
{
    if (thickness == 0)
        return;

    border->type             = DATA_TYPE_BORDER;
    border->thickness        = thickness;
    border->width            = rect_w;
    border->height           = rect_h;
    border->pattern_rotation = pattern_rotation;
    border->pattern          = cairo_pattern_reference(pattern);
    border->enabled          = true;
    border->attached_tree    = NULL;

    border_buffer_init(border, pattern, rect_w, rect_h, thickness);
}

void cwc_border_destroy(struct cwc_border *border)
{
    if (!is_border_valid(border))
        return;

    border_buffer_fini(border);
    cairo_pattern_destroy(border->pattern);

    *border = (struct cwc_border){0};
}

void cwc_border_attach_to_scene(struct cwc_border *border,
                                struct wlr_scene_tree *scene_tree)
{
    if (!is_border_valid(border))
        return;

    border->attached_tree = scene_tree;
    for (int i = 0; i < 4; i++) {
        border->buffer[i]->scene =
            wlr_scene_buffer_create(scene_tree, &border->buffer[i]->base);
        wlr_scene_node_lower_to_bottom(&border->buffer[i]->scene->node);
        border->buffer[i]->scene->node.data = border;
    }

    int bw = cwc_border_get_thickness(border);
    wlr_scene_node_set_position(&border->buffer[1]->scene->node,
                                border->width - bw, bw);
    wlr_scene_node_set_position(&border->buffer[2]->scene->node, 0,
                                border->height - bw);
    wlr_scene_node_set_position(&border->buffer[3]->scene->node, 0, bw);
}

void cwc_border_set_enabled(struct cwc_border *border, bool enabled)
{
    if (!is_border_valid(border))
        return;

    for (int i = 0; i < 4; i++)
        wlr_scene_node_set_enabled(&border->buffer[i]->scene->node, enabled);
    border->enabled = enabled;
}

void cwc_border_set_pattern(struct cwc_border *border,
                            struct _cairo_pattern *pattern)
{
    if (!is_border_valid(border))
        return;

    if (pattern == border->pattern)
        return;

    cairo_pattern_destroy(border->pattern);
    border->pattern = cairo_pattern_reference(pattern);
    border_buffer_redraw(border);
}

void cwc_border_set_pattern_rotation(struct cwc_border *border, int rotation)
{

    if (!is_border_valid(border))
        return;

    if (rotation == border->pattern_rotation)
        return;

    rotation                 = CLAMP(rotation, -315, INT_MAX);
    border->pattern_rotation = rotation;
    border_buffer_redraw(border);
}

static void all_toplevel_reposition_tree(struct cwc_toplevel *toplevel,
                                         void *data)
{
    int thickness = *(int *)data;

    wlr_scene_node_set_position(&toplevel->surf_tree->node, thickness,
                                thickness);
}

static void
cwc_container_reposition_client_tree(struct cwc_container *container)
{
    int thickness = container->border.thickness;
    wlr_scene_node_set_position(&container->popup_tree->node, thickness,
                                thickness);

    cwc_container_for_each_toplevel(container, all_toplevel_reposition_tree,
                                    &thickness);
}

void cwc_border_set_thickness(struct cwc_border *border, int thickness)
{
    if (!is_border_valid(border))
        return;

    if (thickness == border->thickness)
        return;

    border->thickness = thickness;
    border_buffer_redraw(border);

    struct cwc_container *container =
        wl_container_of(border, container, border);
    cwc_container_reposition_client_tree(container);
    cwc_output_tiling_layout_update(container->output, 0);
}

int cwc_border_get_thickness(struct cwc_border *border)
{
    if (!border->enabled)
        return 0;

    return border->thickness;
}

void cwc_border_resize(struct cwc_border *border, int rect_w, int rect_h)
{
    if (!is_border_valid(border))
        return;

    if (border->width == rect_w && border->height == rect_h)
        return;

    border->width  = rect_w;
    border->height = rect_h;
    border_buffer_redraw(border);
}

//===================== CONTAINER ==========================

static void init_surf_tree(struct cwc_toplevel *toplevel,
                           struct cwc_container *container)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        toplevel->surf_tree = wlr_scene_subsurface_tree_create(
            container->tree, toplevel->xwsurface->surface);

        toplevel->xwsurface->surface->data = toplevel->surf_tree;
        wlr_scene_node_set_position(&container->tree->node,
                                    toplevel->xwsurface->x,
                                    toplevel->xwsurface->y);

        goto assign_common;
    }

    toplevel->surf_tree = wlr_scene_xdg_surface_create(
        container->tree, toplevel->xdg_toplevel->base);

assign_common:
    toplevel->surf_tree->node.data = toplevel;
    wlr_scene_node_place_below(&toplevel->surf_tree->node,
                               &container->popup_tree->node);
}

static void decide_should_tiled(struct cwc_toplevel *toplevel,
                                struct cwc_container *cont)
{
    if (cwc_toplevel_wants_fullscreen(toplevel)) {
        cwc_toplevel_set_fullscreen(toplevel, true);
        return;
    }

    if (cwc_toplevel_wants_maximized(toplevel)) {
        cwc_toplevel_set_maximized(toplevel, true);
        return;
    }

    if (cwc_toplevel_wants_minimized(toplevel)) {
        cwc_toplevel_set_minimized(toplevel, true);
        return;
    }

    if (cwc_toplevel_should_float(toplevel)) {
        cwc_toplevel_set_floating(toplevel, true);

        if (!cwc_toplevel_is_unmanaged(toplevel))
            cwc_toplevel_to_center(toplevel);

        return;
    }

    // setup tiled toplevel
    switch (cont->output->state->tag_info[cont->workspace].layout_mode) {
    case CWC_LAYOUT_FLOATING:
        return;
    case CWC_LAYOUT_MASTER:
        master_arrange_update(cont->output);
        break;
    case CWC_LAYOUT_BSP:
        bsp_insert_container(cont, cont->workspace);
        break;
    default:
        unreachable_();
    }

    cont->state &= ~CONTAINER_STATE_FLOATING;
}

static inline void
_update_to_current_active_tag_and_worskpace(struct cwc_container *cont)
{
    cont->tag       = cont->output->state->active_tag;
    cont->workspace = cont->output->state->active_workspace;
}

void cwc_container_init(struct cwc_output *output,
                        struct cwc_toplevel *toplevel,
                        int border_w)
{
    struct cwc_container *cont = calloc(1, sizeof(*cont));
    cont->type                 = DATA_TYPE_CONTAINER;
    cont->output               = server.focused_output;
    cont->tree                 = wlr_scene_tree_create(server.root.toplevel);
    cont->popup_tree           = wlr_scene_tree_create(cont->tree);
    cont->tree->node.data      = cont;
    cont->opacity              = 1.0f;

    int gaps = cwc_output_get_current_tag_info(cont->output)->useless_gaps;
    struct wlr_box geom      = cwc_toplevel_get_geometry(toplevel);
    cont->width              = geom.width + (g_config.border_width + gaps) * 2;
    cont->height             = geom.height + (g_config.border_width + gaps) * 2;
    cont->floating_box       = output->output_layout_box;
    cont->floating_box.width = cont->width;
    cont->floating_box.height = cont->height;

    _update_to_current_active_tag_and_worskpace(cont);

    // putting toplevel to 0 will make toplevel hidden
    cont->tag       = cont->tag ? cont->tag : 1;
    cont->workspace = cont->workspace ? cont->workspace : 1;

    wl_list_init(&cont->toplevels);
    wl_list_insert(&server.containers, &cont->link);

    wlr_scene_node_raise_to_top(&cont->popup_tree->node);

    cwc_border_init(&cont->border, g_config.border_color_normal,
                    g_config.border_color_rotation_degree, cont->width,
                    cont->height, border_w);
    cwc_border_attach_to_scene(&cont->border, cont->tree);

    // ===== toplevel initialization =====

    toplevel->container = cont;
    wl_list_insert(&cont->toplevels, &toplevel->link_container);

    init_surf_tree(toplevel, cont);
    cwc_container_reposition_client_tree(cont);

    if (cwc_toplevel_is_unmanaged(toplevel)) {
        cont->state |= CONTAINER_STATE_UNMANAGED;
        goto emit_signal;
    }

    wl_list_insert(&cont->output->state->containers,
                   &cont->link_output_container);
    wl_list_insert(&cont->output->state->focus_stack,
                   &cont->link_output_fstack);

    decide_should_tiled(toplevel, cont);

emit_signal:
    lua_State *L = g_config_get_lua_State();
    luaC_object_container_register(L, cont);
    cwc_object_emit_signal_simple("container::new", L, toplevel);
}

void cwc_container_insert_toplevel(struct cwc_container *c,
                                   struct cwc_toplevel *toplevel)
{
    if (cwc_container_is_unmanaged(c) || cwc_toplevel_is_unmanaged(toplevel))
        return;

    toplevel->container = c;
    wl_list_insert(&c->toplevels, &toplevel->link_container);

    if (!toplevel->surf_tree)
        init_surf_tree(toplevel, c);
    else {
        wlr_scene_node_reparent(&toplevel->surf_tree->node, c->tree);
        wlr_scene_node_place_below(&toplevel->surf_tree->node,
                                   &c->popup_tree->node);
    }

    int bw = cwc_border_get_thickness(&c->border);
    wlr_scene_node_set_position(&toplevel->surf_tree->node, bw, bw);

    cwc_container_set_size(c, c->width, c->height);
    cwc_object_emit_signal_varr("container::insert", g_config_get_lua_State(),
                                2, c, toplevel);
}

static void _destroy_container(struct cwc_container *container)
{
    lua_State *L = g_config_get_lua_State();
    cwc_object_emit_signal_simple("container::destroy", L, container);

    if (server.insert_marked == container)
        server.insert_marked = NULL;

    if (!cwc_container_is_unmanaged(container)) {
        wl_list_remove(&container->link_output_container);
        wl_list_remove(&container->link_output_fstack);
    }

    if (container->output->state->tag_info[container->workspace].layout_mode
        == CWC_LAYOUT_MASTER)
        cwc_output_tiling_layout_update(container->output,
                                        container->workspace);
    if (container->bsp_node)
        bsp_remove_container(container, true);

    if (container->old_prop.bsp_node) {
        container->output   = container->old_prop.output;
        container->bsp_node = container->old_prop.bsp_node;
        bsp_remove_container(container, false);
    }

    if (container->link_output_minimized.next
        && container->link_output_minimized.prev)
        wl_list_remove(&container->link_output_minimized);

    luaC_object_unregister(L, container);

    cwc_border_destroy(&container->border);
    wlr_scene_node_destroy(&container->popup_tree->node);
    wlr_scene_node_destroy(&container->tree->node);

    wl_list_remove(&container->link);
    free(container);
}

static void _clear_container_stuff_in_toplevel(struct cwc_toplevel *toplevel)
{
    cwc_object_emit_signal_varr("container::remove", g_config_get_lua_State(),
                                2, toplevel->container, toplevel);

    // toplevel should be inserted to container again when removing from
    // container
    wlr_scene_node_reparent(&toplevel->surf_tree->node, server.temporary_tree);

    cwc_container_refresh(toplevel->container);

    wl_list_remove(&toplevel->link_container);
    toplevel->container = NULL;
}

void cwc_container_remove_toplevel(struct cwc_toplevel *toplevel)
{
    struct cwc_container *cont = toplevel->container;

    _clear_container_stuff_in_toplevel(toplevel);

    if (wl_list_length(&cont->toplevels))
        return;

    _destroy_container(cont);
}

void cwc_container_remove_toplevel_but_dont_destroy_container_when_empty(
    struct cwc_toplevel *toplevel)
{
    _clear_container_stuff_in_toplevel(toplevel);
}

void cwc_container_for_each_toplevel_top_to_bottom(
    struct cwc_container *container,
    void (*f)(struct cwc_toplevel *toplevel, void *data),
    void *data)
{
    struct wlr_scene_node *node;
    struct wlr_scene_node *tmp;
    wl_list_for_each_reverse_safe(node, tmp, &container->tree->children, link)
    {
        if (!node->data)
            continue;

        cwc_data_interface_t *data_iface = node->data;
        if (data_iface->type == DATA_TYPE_XDG_SHELL
            || data_iface->type == DATA_TYPE_XWAYLAND) {
            f(node->data, data);
        }
    }
}

static void all_toplevel_send_output_enter(struct cwc_toplevel *toplevel,
                                           void *data)
{
    struct cwc_output *output = data;

    if (toplevel->wlr_foreign_handle) {
        wlr_foreign_toplevel_handle_v1_output_enter(
            toplevel->wlr_foreign_handle, output->wlr_output);
    }
}

static void all_toplevel_send_output_leave(struct cwc_toplevel *toplevel,
                                           void *data)
{
    struct cwc_output *output = data;

    if (toplevel->wlr_foreign_handle) {
        wlr_foreign_toplevel_handle_v1_output_leave(
            toplevel->wlr_foreign_handle, output->wlr_output);
    }
}

static void __cwc_container_move_to_output(struct cwc_container *container,
                                           struct cwc_output *output,
                                           bool translate)
{
    struct cwc_output *old = container->output;
    if (old == output)
        return;

    bool floating = cwc_container_is_floating(container);

    if (container->bsp_node)
        bsp_remove_container(container, false);

    container->output = output;
    wl_list_reattach(output->state->focus_stack.prev,
                     &container->link_output_fstack);
    wl_list_reattach(output->state->containers.prev,
                     &container->link_output_container);

    if (container->link_output_minimized.next)
        wl_list_reattach(output->state->minimized.prev,
                         &container->link_output_minimized);

    cwc_container_for_each_toplevel(container, all_toplevel_send_output_enter,
                                    output);
    cwc_container_for_each_toplevel(container, all_toplevel_send_output_leave,
                                    old);

    /* don't translate position when move to fallback output or vice versa
     * because it'll ruin the layout since the fallback output is not attached
     * to scene output
     */
    if (!translate || old == server.fallback_output
        || output == server.fallback_output)
        return;

    struct wlr_box oldbox = cwc_container_get_box(container);
    double x, y;
    normalized_region_at(&old->output_layout_box, oldbox.x, oldbox.y, &x, &y);
    x *= output->output_layout_box.width;
    y *= output->output_layout_box.height;
    x += output->output_layout_box.x;
    y += output->output_layout_box.y;

    container->floating_box.x = x;
    container->floating_box.y = y;

    /* set to float when it's floating in previous output to prevent dragged
     * container to get tiled */
    if (floating) {
        cwc_container_set_position(container, x, y);
        cwc_container_move_to_tag(container, output->state->active_workspace);
        container->state |= CONTAINER_STATE_FLOATING;
    }
}

void cwc_container_move_to_output_without_translate(
    struct cwc_container *container, struct cwc_output *output)
{
    __cwc_container_move_to_output(container, output, false);
}

void cwc_container_move_to_output(struct cwc_container *container,
                                  struct cwc_output *output)
{
    __cwc_container_move_to_output(container, output, true);
}

void cwc_container_for_each_bottom_to_top(
    struct cwc_container *container,
    void (*f)(struct cwc_toplevel *toplevel, void *data),
    void *data)
{
    struct wlr_scene_node *node;
    struct wlr_scene_node *tmp;
    wl_list_for_each_safe(node, tmp, &container->tree->children, link)
    {
        if (!node->data)
            continue;

        cwc_data_interface_t *data_iface = node->data;
        if (data_iface->type == DATA_TYPE_XDG_SHELL
            || data_iface->type == DATA_TYPE_XWAYLAND) {
            f(node->data, data);
        }
    }
}

void cwc_container_for_each_toplevel(struct cwc_container *container,
                                     void (*f)(struct cwc_toplevel *toplevel,
                                               void *data),
                                     void *data)
{
    struct cwc_toplevel *toplevel;
    struct cwc_toplevel *tmp;
    wl_list_for_each_safe(toplevel, tmp, &container->toplevels, link_container)
    {
        f(toplevel, data);
    }
}

struct wlr_box cwc_container_get_box(struct cwc_container *container)
{
    return (struct wlr_box){
        .x      = container->tree->node.x,
        .y      = container->tree->node.y,
        .width  = container->width,
        .height = container->height,
    };
}

struct cwc_toplevel *
cwc_container_get_front_toplevel(struct cwc_container *cont)
{
    struct wlr_scene_node *node;
    wl_list_for_each_reverse(node, &cont->tree->children, link)
    {
        if (!node->data)
            continue;

        cwc_data_interface_t *data_iface = node->data;
        if (data_iface->type == DATA_TYPE_XDG_SHELL
            || data_iface->type == DATA_TYPE_XWAYLAND) {
            return node->data;
        }
    }

    return NULL;
}

void cwc_container_set_front_toplevel(struct cwc_toplevel *toplevel)
{
    if (!toplevel)
        return;

    wlr_scene_node_set_enabled(&toplevel->surf_tree->node, true);
    __cwc_toplevel_set_minimized(toplevel, false);

    struct cwc_container *container = toplevel->container;
    cwc_container_set_size(container, container->width, container->height);
    wlr_scene_node_place_below(&toplevel->surf_tree->node,
                               &container->popup_tree->node);

    struct cwc_toplevel *t;
    wl_list_for_each(t, &toplevel->container->toplevels, link_container)
    {
        if (t == toplevel)
            continue;

        wlr_scene_node_set_enabled(&t->surf_tree->node, false);
        __cwc_toplevel_set_minimized(t, true);
    }
}

static void _focusnext(struct cwc_toplevel *toplevel, int step)
{
    struct cwc_container *container = toplevel->container;
    struct cwc_toplevel *t;
    wl_list_for_each_reverse(t, &toplevel->link_container, link_container)
    {
        if (&t->link_container == &container->toplevels)
            continue;

        if (--step == 0)
            break;
    }

    cwc_container_set_front_toplevel(t);
    cwc_toplevel_focus(t, false);
}

static void _focusprev(struct cwc_toplevel *toplevel, int step)
{
    struct cwc_container *container = toplevel->container;
    struct cwc_toplevel *t;
    wl_list_for_each(t, &toplevel->link_container, link_container)
    {
        if (&t->link_container == &container->toplevels)
            continue;

        if (++step == 0)
            break;
    }

    cwc_container_set_front_toplevel(t);
    cwc_toplevel_focus(t, false);
}

void cwc_container_focusidx(struct cwc_container *container, int idx)
{
    if (!idx)
        return;

    struct cwc_toplevel *top = cwc_container_get_front_toplevel(container);

    if (idx > 0)
        _focusnext(top, idx);
    else
        _focusprev(top, idx);
}

static void _remove_and_save_toplevel_ordering(struct cwc_toplevel *toplevel,
                                               void *data)
{
    struct wl_array *templist = data;
    cwc_container_remove_toplevel_but_dont_destroy_container_when_empty(
        toplevel);
    struct cwc_toplevel **saveptr = wl_array_add(templist, sizeof(&toplevel));
    *saveptr                      = toplevel;
}

void cwc_container_swap(struct cwc_container *source,
                        struct cwc_container *target)
{
    if (source == target)
        return;

    struct cwc_toplevel *stop = cwc_container_get_front_toplevel(source);
    struct cwc_toplevel *ttop = cwc_container_get_front_toplevel(target);
    struct wl_array source_temp_array;
    struct wl_array target_temp_array;
    wl_array_init(&source_temp_array);
    wl_array_init(&target_temp_array);

    cwc_container_for_each_toplevel(source, _remove_and_save_toplevel_ordering,
                                    &source_temp_array);
    cwc_container_for_each_toplevel(target, _remove_and_save_toplevel_ordering,
                                    &target_temp_array);

    struct cwc_toplevel **toplevel;
    wl_array_for_each(toplevel, &source_temp_array)
    {
        cwc_container_insert_toplevel(target, *toplevel);
    }

    wl_array_for_each(toplevel, &target_temp_array)
    {
        cwc_container_insert_toplevel(source, *toplevel);
    }

    cwc_container_set_front_toplevel(stop);
    cwc_container_set_front_toplevel(ttop);

    wl_array_release(&source_temp_array);
    wl_array_release(&target_temp_array);

    cwc_object_emit_signal_varr("container::swap", g_config_get_lua_State(), 2,
                                source, target);
}

inline bool cwc_container_is_floating(struct cwc_container *cont)
{
    return (cont->state & CONTAINER_STATE_FLOATING)
           || cont->output->state->tag_info[cont->workspace].layout_mode
                  == CWC_LAYOUT_FLOATING;
}

static void all_toplevel_set_suspended(struct cwc_toplevel *toplevel,
                                       void *data)
{
    bool set = data;
    __cwc_toplevel_set_minimized(toplevel, set);
}

void cwc_container_set_enabled(struct cwc_container *container, bool set)
{
    wlr_scene_node_set_enabled(&container->tree->node, set);
    if (set) {
        cwc_container_refresh(container);
    } else {
        cwc_container_for_each_toplevel(container, all_toplevel_set_suspended,
                                        (void *)false);
    }
}

#define EMIT_PROP_SIGNAL_FOR_FRONT_TOPLEVEL(propname, container)  \
    cwc_object_emit_signal_simple("client::property::" #propname, \
                                  g_config_get_lua_State(),       \
                                  cwc_container_get_front_toplevel(container))

void cwc_container_set_floating(struct cwc_container *container, bool set)
{
    // don't change the floating state when maximize and fullscreen cuz the
    // behavior is confusing.
    if (!cwc_container_is_configure_allowed(container))
        return;

    if (set) {
        cwc_container_restore_floating_box(container);
        if (container->bsp_node)
            bsp_node_disable(container->bsp_node);
        container->state |= CONTAINER_STATE_FLOATING;
        cwc_output_tiling_layout_update(container->output,
                                        container->workspace);
    } else if (cwc_container_is_floating(container)) {
        container->state &= ~CONTAINER_STATE_FLOATING;
        if (container->bsp_node) {
            bsp_node_enable(container->bsp_node);
        } else if (cwc_output_is_current_layout_bsp(container->output)) {
            bsp_insert_container(container, container->workspace);
        }
        cwc_output_tiling_layout_update(container->output,
                                        container->workspace);
    }

    EMIT_PROP_SIGNAL_FOR_FRONT_TOPLEVEL(floating, container);
}

void cwc_container_set_sticky(struct cwc_container *container, bool set)
{
    if (set) {
        container->state |= CONTAINER_STATE_STICKY;
        return;
    }

    container->state &= ~CONTAINER_STATE_STICKY;
    cwc_output_update_visible(container->output);
}

static void all_toplevel_set_fullscreen(struct cwc_toplevel *toplevel,
                                        void *data)
{
    bool set = data;

    if (set) {
        cwc_toplevel_set_size_surface(
            toplevel, toplevel->container->output->wlr_output->width,
            toplevel->container->output->wlr_output->height);
        cwc_toplevel_set_position(toplevel, 0, 0);
        wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node, NULL);
    }

    __cwc_toplevel_set_fullscreen(toplevel, set);

    if (toplevel->wlr_foreign_handle)
        wlr_foreign_toplevel_handle_v1_set_fullscreen(
            toplevel->wlr_foreign_handle, set);
}

void cwc_container_set_fullscreen(struct cwc_container *container, bool set)
{
    struct bsp_node *bsp_node = container->bsp_node;
    cwc_border_set_enabled(&container->border, !set);

    if (set) {
        // set first so the set_size don't save the fullscreen dimension as
        // floating box
        container->state |= CONTAINER_STATE_FULLSCREEN;

        if (bsp_node)
            bsp_node_disable(bsp_node);

        float black[4]           = {0, 0, 0, 1};
        container->fullscreen_bg = wlr_scene_rect_create(
            container->tree, container->output->output_layout_box.width,
            container->output->output_layout_box.height, black);
        wlr_scene_node_lower_to_bottom(&container->fullscreen_bg->node);
    } else {
        // set first so bsp is allowing it to configure
        container->state &= ~CONTAINER_STATE_FULLSCREEN;

        if (cwc_container_is_floating(container))
            cwc_container_restore_floating_box(container);
        else if (container->bsp_node)
            bsp_node_enable(bsp_node);

        if (container->fullscreen_bg) {
            wlr_scene_node_destroy(&container->fullscreen_bg->node);
            container->fullscreen_bg = NULL;
        }

        if (container->state & CONTAINER_STATE_MAXIMIZED)
            cwc_container_set_maximized(container, true);
    }

    cwc_container_for_each_toplevel(container, all_toplevel_set_fullscreen,
                                    (void *)set);

    master_arrange_update(container->output);

    EMIT_PROP_SIGNAL_FOR_FRONT_TOPLEVEL(fullscreen, container);
}

static void all_toplevel_set_maximized(struct cwc_toplevel *toplevel,
                                       void *data)
{
    bool set = data;
    __cwc_toplevel_set_maximized(toplevel, set);

    if (set) {
        struct wlr_box usable_area = toplevel->container->output->usable_area;
        cwc_toplevel_set_size_surface(toplevel, usable_area.width,
                                      usable_area.height);
        cwc_toplevel_set_position(toplevel, usable_area.x, usable_area.y);
        wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node, NULL);
    }

    if (toplevel->wlr_foreign_handle)
        wlr_foreign_toplevel_handle_v1_set_maximized(
            toplevel->wlr_foreign_handle, set);
}

void cwc_container_set_maximized(struct cwc_container *container, bool set)
{
    struct bsp_node *bsp_node = container->bsp_node;
    cwc_border_set_enabled(&container->border, !set);

    if (cwc_container_is_fullscreen(container)) {
        set = true;
        cwc_container_set_fullscreen(container, false);
    }

    if (set) {
        container->state |= CONTAINER_STATE_MAXIMIZED;

        if (bsp_node)
            bsp_node_disable(bsp_node);

    } else {
        container->state &= ~CONTAINER_STATE_MAXIMIZED;

        if (cwc_container_is_floating(container))
            cwc_container_restore_floating_box(container);
        else if (container->bsp_node)
            bsp_node_enable(bsp_node);
    }

    cwc_container_for_each_toplevel(container, all_toplevel_set_maximized,
                                    (void *)set);

    master_arrange_update(container->output);

    EMIT_PROP_SIGNAL_FOR_FRONT_TOPLEVEL(maximized, container);
}

static void all_toplevel_set_minimized(struct cwc_toplevel *toplevel,
                                       void *data)
{
    bool set = data;
    __cwc_toplevel_set_minimized(toplevel, set);

    if (toplevel->wlr_foreign_handle)
        wlr_foreign_toplevel_handle_v1_set_minimized(
            toplevel->wlr_foreign_handle, set);
}

void cwc_container_set_minimized(struct cwc_container *container, bool set)
{
    wlr_scene_node_set_enabled(&container->tree->node, !set);
    struct bsp_node *bsp_node = container->bsp_node;
    if (set) {
        struct cwc_output *o = container->output;
        wl_list_insert(&o->state->minimized, &container->link_output_minimized);

        if (bsp_node)
            bsp_node_disable(bsp_node);

        container->state |= CONTAINER_STATE_MINIMIZED;
        cwc_output_focus_newest_focus_visible_toplevel(container->output);
    } else {
        container->state &= ~CONTAINER_STATE_MINIMIZED;

        if (container->link_output_minimized.next)
            wl_list_remove(&container->link_output_minimized);

        if (bsp_node)
            bsp_node_enable(bsp_node);
    }

    cwc_container_for_each_toplevel(container, all_toplevel_set_minimized,
                                    (void *)set);

    master_arrange_update(container->output);

    EMIT_PROP_SIGNAL_FOR_FRONT_TOPLEVEL(minimized, container);
}

static void all_toplevel_set_size(struct cwc_toplevel *toplevel, void *data)
{
    struct wlr_box *box = data;
    struct wlr_box geom = cwc_toplevel_get_geometry(toplevel);

    int surf_w = box->width;
    int surf_h = box->height;

    if (geom.width == surf_w && geom.height == surf_h
        && !cwc_toplevel_is_x11(toplevel))
        return;

    struct wlr_box clip = {
        .x      = 0,
        .y      = 0,
        .width  = surf_w,
        .height = surf_h,
    };

    if (!cwc_toplevel_is_x11(toplevel)) {
        // when floating we respect the min width
        if (cwc_toplevel_is_floating(toplevel)) {
            surf_w = MAX(surf_w, toplevel->xdg_toplevel->current.min_width);
            surf_h = MAX(surf_h, toplevel->xdg_toplevel->current.min_height);
            clip.width  = surf_w;
            clip.height = surf_h;
        }

        clip.x = geom.x;
        clip.y = geom.y;

        if (cwc_toplevel_is_visible(toplevel) && !toplevel->resize_serial)
            server.resize_count = MAX(1, server.resize_count + 1);
    }

    toplevel->resize_serial = cwc_toplevel_set_size(toplevel, surf_w, surf_h);
    wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node, &clip);
    box->width  = surf_w;
    box->height = surf_h;
}

static inline bool
cwc_container_should_save_floating_box(struct cwc_container *container)
{
    return cwc_container_is_floating(container)
           && !cwc_container_is_fullscreen(container)
           && !cwc_container_is_maximized(container);
}

static inline void update_container_output(struct cwc_container *container)
{
    struct wlr_box box        = cwc_container_get_box(container);
    int x                     = box.x + (box.width / 2);
    int y                     = box.y + (box.height / 2);
    struct cwc_output *output = cwc_output_at(server.output_layout, x, y);

    if (!output || output == container->output)
        return;

    cwc_container_move_to_output_without_translate(container, output);
}

void cwc_container_set_size(struct cwc_container *container, int w, int h)
{
    int gaps = cwc_output_get_current_tag_info(container->output)->useless_gaps;

    int bw            = cwc_border_get_thickness(&container->border);
    int outside_width = (bw + gaps) * 2;

    int surface_w = MAX(w - outside_width, MIN_WIDTH);
    int surface_h = MAX(h - outside_width, MIN_WIDTH);

    struct wlr_box rect = {.width = surface_w, .height = surface_h};
    cwc_container_for_each_bottom_to_top(container, all_toplevel_set_size,
                                         &rect);

    int cont_w = rect.width + bw * 2;
    int cont_h = rect.height + bw * 2;

    cwc_border_resize(&container->border, cont_w, cont_h);

    if (cwc_container_should_save_floating_box(container)) {
        container->floating_box.width  = w;
        container->floating_box.height = h;
    }

    cont_w += gaps * 2;
    cont_h += gaps * 2;

    container->width  = cont_w;
    container->height = cont_h;
}

static void all_toplevel_update_xwsurface(struct cwc_toplevel *toplevel,
                                          void *data)
{
    struct wlr_box *xy = data;
    if (cwc_toplevel_is_x11(toplevel)) {
        struct wlr_xwayland_surface *xwsurface = toplevel->xwsurface;
        wlr_xwayland_surface_configure(xwsurface, xy->x, xy->y,
                                       xwsurface->width, xwsurface->height);
    }
}

void cwc_container_set_position_global(struct cwc_container *container,
                                       int x,
                                       int y)
{
    wlr_scene_node_set_position(&container->tree->node, x, y);

    struct wlr_box xy = {.x = x, .y = y};
    cwc_container_for_each_toplevel(container, all_toplevel_update_xwsurface,
                                    &xy);

    if (cwc_container_should_save_floating_box(container)) {
        container->floating_box.x = x;
        container->floating_box.y = y;
    }

    update_container_output(container);
}

void cwc_container_set_position_gap(struct cwc_container *container,
                                    int x,
                                    int y)
{
    int gaps = cwc_output_get_current_tag_info(container->output)->useless_gaps;
    int pos_x = x + gaps;
    int pos_y = y + gaps;
    cwc_container_set_position(container, pos_x, pos_y);
}

void cwc_container_set_position(struct cwc_container *container, int x, int y)
{
    x += container->output->output_layout_box.x;
    y += container->output->output_layout_box.y;

    cwc_container_set_position_global(container, x, y);
}

void cwc_container_set_box_global(struct cwc_container *container,
                                  struct wlr_box *box)
{
    wlr_scene_node_set_position(&container->tree->node, box->x, box->y);
    cwc_container_set_size(container, box->width, box->height);
}

void cwc_container_set_box_global_gap(struct cwc_container *container,
                                      struct wlr_box *box)
{
    int gaps = cwc_output_get_current_tag_info(container->output)->useless_gaps;
    int pos_x = box->x + gaps;
    int pos_y = box->y + gaps;

    wlr_scene_node_set_position(&container->tree->node, pos_x, pos_y);
    cwc_container_set_size(container, box->width, box->height);
}

void cwc_container_set_box(struct cwc_container *container, struct wlr_box *box)
{
    struct wlr_box newbox = *box;
    newbox.x              = box->x + container->output->output_layout_box.x;
    newbox.y              = box->y + container->output->output_layout_box.y;

    cwc_container_set_box_global(container, &newbox);
}

void cwc_container_set_box_gap(struct cwc_container *container,
                               struct wlr_box *box)
{
    struct wlr_box newbox = *box;
    newbox.x              = box->x + container->output->output_layout_box.x;
    newbox.y              = box->y + container->output->output_layout_box.y;

    cwc_container_set_box_global_gap(container, &newbox);
}

void cwc_container_restore_floating_box(struct cwc_container *container)
{
    struct wlr_box *float_box = &container->floating_box;
    cwc_container_set_position_global(container, float_box->x, float_box->y);
    cwc_container_set_size(container, float_box->width, float_box->height);
}

bool cwc_container_is_visible(struct cwc_container *container)
{
    if (cwc_container_is_sticky(container))
        return true;

    if (!container->output->state->active_tag
        || cwc_container_is_minimized(container))
        return false;

    return (container->output->state->active_tag & container->tag);
}

bool cwc_container_is_visible_in_workspace(struct cwc_container *container,
                                           int workspace)
{
    if (!container->output->state->active_workspace
        || !container->output->state->active_tag
        || cwc_container_is_minimized(container))
        return false;

    return (workspace == container->workspace);
}

void cwc_container_move_to_tag(struct cwc_container *container, int workspace)
{
    if (container->workspace == workspace)
        return;

    if (container->bsp_node)
        bsp_remove_container(container, true);

    container->tag       = 1 << (workspace - 1);
    container->workspace = workspace;

    struct cwc_tag_info *tag_info =
        &container->output->state->tag_info[workspace];
    if (tag_info->layout_mode == CWC_LAYOUT_BSP)
        bsp_insert_container(container, workspace);

    cwc_output_tiling_layout_update(container->output, container->workspace);
    cwc_output_update_visible(container->output);
}

void cwc_container_to_center(struct cwc_container *container)
{
    if (!cwc_container_is_configure_allowed(container))
        return;

    struct wlr_box usable_area = container->output->usable_area;
    int x                      = usable_area.width / 2 - container->width / 2;
    int y                      = usable_area.height / 2 - container->height / 2;
    x                          = x < usable_area.x ? usable_area.x : x;
    y                          = y < usable_area.y ? usable_area.y : y;
    cwc_container_set_position(container, x, y);
}

void cwc_container_raise(struct cwc_container *container)
{
    wlr_scene_node_raise_to_top(&container->tree->node);

    cwc_object_emit_signal_simple("client::raised", g_config_get_lua_State(),
                                  cwc_container_get_front_toplevel(container));
}

void cwc_container_lower(struct cwc_container *container)
{
    wlr_scene_node_lower_to_bottom(&container->tree->node);

    cwc_object_emit_signal_simple("client::lowered", g_config_get_lua_State(),
                                  cwc_container_get_front_toplevel(container));
}

void cwc_container_set_opacity(struct cwc_container *container, float opacity)
{
    opacity            = CLAMP(opacity, 0.0, 1.0);
    container->opacity = opacity;

    wlr_output_schedule_frame(container->output->wlr_output);
}
