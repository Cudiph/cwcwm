/* layer_shell.c - wlr layer shell protocol implementation
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

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/layout/master.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

static void on_layer_surface_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_layer_surface *lsurf =
        wl_container_of(listener, lsurf, destroy_l);

    cwc_log(CWC_DEBUG, "destroying layer surface at output %p: %p",
            lsurf->output, lsurf->wlr_layer_surface);

    if (cwc_output_is_exist(lsurf->output))
        wlr_scene_node_destroy(&lsurf->popup_tree->node);

    wl_list_remove(&lsurf->link);
    wl_list_remove(&lsurf->map_l.link);
    wl_list_remove(&lsurf->unmap_l.link);
    wl_list_remove(&lsurf->commit_l.link);
    wl_list_remove(&lsurf->new_popup_l.link);
    wl_list_remove(&lsurf->destroy_l.link);

    free(lsurf);
}

static struct wlr_scene_tree *
layer_shell_get_scene(struct cwc_output *output,
                      enum zwlr_layer_shell_v1_layer layer)
{
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return output->layers.background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return output->layers.bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return output->layers.top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return output->layers.overlay;
    }

    unreachable_();
    return output->layers.bottom;
}

static void arrange_surface(struct cwc_output *output,
                            const struct wlr_box *full_area,
                            struct wlr_box *usable_area,
                            struct wlr_scene_tree *tree,
                            bool exclusive)
{
    struct wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link)
    {
        struct cwc_layer_surface *surface = node->data;
        // surface could be null during destruction
        if (!surface)
            continue;

        if (surface->type != DATA_TYPE_LAYER_SHELL)
            continue;

        if (!surface->wlr_layer_surface->initialized)
            continue;

        if ((surface->wlr_layer_surface->current.exclusive_zone > 0)
            != exclusive)
            continue;

        wlr_scene_layer_surface_v1_configure(surface->scene_layer, full_area,
                                             usable_area);
        wlr_scene_node_set_position(&surface->popup_tree->node,
                                    surface->scene_layer->tree->node.x,
                                    surface->scene_layer->tree->node.y);
    }
}

static void cwc_output_maximized_toplevel_update(struct cwc_output *output)
{
    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &output->state->toplevels, link_output_toplevels)
    {
        if (cwc_toplevel_is_maximized(toplevel))
            cwc_toplevel_set_maximized(toplevel, true);
    }
}

void arrange_layers(struct cwc_output *output)
{
    // switching vt quickly causing race condition (not sure) where output was
    // destroyed and the layer surface is committed with dangling
    // layer_surface->output pointer
    if (!cwc_output_is_exist(output))
        return;

    struct wlr_box usable_area = {0};
    wlr_output_effective_resolution(output->wlr_output, &usable_area.width,
                                    &usable_area.height);
    const struct wlr_box full_area = usable_area;

    // clang-format off
    arrange_surface(output, &full_area, &usable_area, output->layers.overlay, true);
    arrange_surface(output, &full_area, &usable_area, output->layers.top, true);
    arrange_surface(output, &full_area, &usable_area, output->layers.bottom, true);
    arrange_surface(output, &full_area, &usable_area, output->layers.background, true);

    arrange_surface(output, &full_area, &usable_area, output->layers.overlay, false);
    arrange_surface(output, &full_area, &usable_area, output->layers.top, false);
    arrange_surface(output, &full_area, &usable_area, output->layers.bottom, false);
    arrange_surface(output, &full_area, &usable_area, output->layers.background, false);
    // clang-format on

    if (!wlr_box_equal(&usable_area, &output->usable_area)) {
        output->usable_area = usable_area;
        cwc_output_tiling_layout_update(output, 0);
        cwc_output_maximized_toplevel_update(output);
    }

    // lazy implementation: just focus to newest exclusive
    struct cwc_layer_surface *lsurf;
    wl_list_for_each(lsurf, &server.layer_shells, link)
    {
        if (lsurf->wlr_layer_surface->current.keyboard_interactive
            == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
            keyboard_focus_surface(server.seat,
                                   lsurf->wlr_layer_surface->surface);
            server.seat->exclusive_kbd_interactive = lsurf;
            break;
        }
    }
}

static void on_layer_surface_map(struct wl_listener *listener, void *data)
{
    struct cwc_layer_surface *layer_surface =
        wl_container_of(listener, layer_surface, map_l);
    struct wlr_layer_surface_v1 *wlr_layer_surface =
        layer_surface->wlr_layer_surface;

    if (wlr_layer_surface->current.keyboard_interactive
        && (wlr_layer_surface->current.layer
                == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY
            || wlr_layer_surface->current.layer
                   == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
        keyboard_focus_surface(server.seat, wlr_layer_surface->surface);
        arrange_layers(layer_surface->output);
    }
}

static void arrange_layers_idle(void *data)
{
    struct cwc_output *output = data;
    arrange_layers(output);
}

static void on_layer_surface_unmap(struct wl_listener *listener, void *data)
{
    struct cwc_layer_surface *layer_surface =
        wl_container_of(listener, layer_surface, unmap_l);

    if (layer_surface == server.seat->exclusive_kbd_interactive) {
        server.seat->exclusive_kbd_interactive = NULL;
        cwc_output_focus_newest_focus_visible_toplevel(layer_surface->output);
    }

    struct wlr_layer_surface_v1_state *state =
        &layer_surface->wlr_layer_surface->current;
    if (state->exclusive_zone || state->exclusive_edge) {
        wl_event_loop_add_idle(server.wl_event_loop, arrange_layers_idle,
                               layer_surface->output);
    }
}

static void on_layer_surface_commit(struct wl_listener *listener, void *data)
{
    struct cwc_layer_surface *layer_surface =
        wl_container_of(listener, layer_surface, commit_l);
    struct wlr_layer_surface_v1 *wlr_layer_surface =
        layer_surface->wlr_layer_surface;

    if (!wlr_layer_surface->initialized)
        return;

    uint32_t committed = wlr_layer_surface->current.committed;
    if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
        enum zwlr_layer_shell_v1_layer layer_type =
            wlr_layer_surface->current.layer;
        struct wlr_scene_tree *output_layer =
            layer_shell_get_scene(layer_surface->output, layer_type);
        wlr_scene_node_reparent(&layer_surface->scene_layer->tree->node,
                                output_layer);
    }

    if (wlr_layer_surface->initial_commit || committed
        || wlr_layer_surface->surface->mapped != layer_surface->mapped) {
        layer_surface->mapped = wlr_layer_surface->surface->mapped;
        arrange_layers(layer_surface->output);
    }
}

static void on_new_surface(struct wl_listener *listener, void *data)
{
    struct wlr_layer_surface_v1 *layer_surface = data;

    struct cwc_layer_surface *surf = calloc(1, sizeof(*surf));
    surf->type                     = DATA_TYPE_LAYER_SHELL;
    surf->wlr_layer_surface        = layer_surface;

    if (!layer_surface->output)
        layer_surface->output = server.focused_output->wlr_output;

    surf->output = layer_surface->output->data;

    struct wlr_scene_tree *surface_scene_tree =
        layer_shell_get_scene(surf->output, layer_surface->pending.layer);
    surf->scene_layer =
        wlr_scene_layer_surface_v1_create(surface_scene_tree, layer_surface);
    surf->popup_tree = wlr_scene_tree_create(surf->output->layers.overlay);

    layer_surface->data                = surf;
    layer_surface->surface->data       = surf->scene_layer->tree;
    surf->scene_layer->tree->node.data = surf; // used in arrange_surface

    surf->new_popup_l.notify = on_new_xdg_popup;
    surf->destroy_l.notify   = on_layer_surface_destroy;
    wl_signal_add(&surf->wlr_layer_surface->events.new_popup,
                  &surf->new_popup_l);
    wl_signal_add(&surf->wlr_layer_surface->events.destroy, &surf->destroy_l);

    surf->map_l.notify    = on_layer_surface_map;
    surf->unmap_l.notify  = on_layer_surface_unmap;
    surf->commit_l.notify = on_layer_surface_commit;
    wl_signal_add(&layer_surface->surface->events.map, &surf->map_l);
    wl_signal_add(&layer_surface->surface->events.unmap, &surf->unmap_l);
    wl_signal_add(&layer_surface->surface->events.commit, &surf->commit_l);

    wl_list_insert(&server.layer_shells, &surf->link);

    cwc_log(CWC_DEBUG, "created layer surface for output %p: %p",
            layer_surface->output, surf);
}

void setup_layer_shell(struct cwc_server *s)
{
    s->layer_shell = wlr_layer_shell_v1_create(s->wl_display, 4);
    s->layer_shell_surface_l.notify = on_new_surface;
    wl_signal_add(&s->layer_shell->events.new_surface,
                  &s->layer_shell_surface_l);
}

void cleanup_layer_shell(struct cwc_server *s)
{
    wl_list_remove(&s->layer_shell_surface_l.link);
}
