/* toplevel.c - toplevel/window/client processing
 *
 * Copyright (C) 2020 dwl team
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

#include <float.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/util/box.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "cwc/config.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/layout/container.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/types.h"
#include "cwc/util.h"

//=============== XDG SHELL ====================

/* - */
static void on_foreign_request_maximize(struct wl_listener *listener,
                                        void *data)
{
    struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
    struct cwc_toplevel *toplevel = event->toplevel->data;

    cwc_toplevel_set_maximized(toplevel, event->maximized);
}

static void on_foreign_request_minimize(struct wl_listener *listener,
                                        void *data)
{
    struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
    struct cwc_toplevel *toplevel = event->toplevel->data;

    cwc_toplevel_set_minimized(toplevel, event->minimized);
}

static void on_foreign_request_fullscreen(struct wl_listener *listener,
                                          void *data)
{
    struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
    struct cwc_toplevel *toplevel = event->toplevel->data;

    cwc_toplevel_set_fullscreen(toplevel, event->fullscreen);
}

static void on_foreign_request_activate(struct wl_listener *listener,
                                        void *data)
{
    struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
    struct cwc_toplevel *toplevel = event->toplevel->data;

    cwc_toplevel_jump_to(toplevel, false);
}

static void on_foreign_request_close(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, foreign_request_close_l);

    cwc_toplevel_send_close(toplevel);
}

static void on_foreign_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, foreign_destroy_l);

    wl_list_remove(&toplevel->foreign_request_maximize_l.link);
    wl_list_remove(&toplevel->foreign_request_minimize_l.link);
    wl_list_remove(&toplevel->foreign_request_fullscreen_l.link);
    wl_list_remove(&toplevel->foreign_request_activate_l.link);
    wl_list_remove(&toplevel->foreign_request_close_l.link);
    wl_list_remove(&toplevel->foreign_destroy_l.link);
}

static inline void _init_mapped_managed_toplevel(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_unmanaged(toplevel))
        return;

    wl_list_insert(&server.focused_output->state->toplevels,
                   &toplevel->link_output_toplevels);
    cwc_toplevel_set_tiled(toplevel, WLR_EDGE_TOP | WLR_EDGE_BOTTOM
                                         | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);

    struct wlr_ext_foreign_toplevel_handle_v1_state state = {
        .title  = cwc_toplevel_get_title(toplevel),
        .app_id = cwc_toplevel_get_app_id(toplevel),
    };
    toplevel->ext_foreign_handle = wlr_ext_foreign_toplevel_handle_v1_create(
        server.foreign_toplevel_list, &state);
    toplevel->wlr_foreign_handle =
        wlr_foreign_toplevel_handle_v1_create(server.foreign_toplevel_manager);

    toplevel->ext_foreign_handle->data = toplevel;
    toplevel->wlr_foreign_handle->data = toplevel;

    wlr_foreign_toplevel_handle_v1_output_enter(
        toplevel->wlr_foreign_handle, server.focused_output->wlr_output);

    if (state.app_id)
        wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->wlr_foreign_handle,
                                                  state.app_id);
    if (state.title)
        wlr_foreign_toplevel_handle_v1_set_title(toplevel->wlr_foreign_handle,
                                                 state.title);

    toplevel->foreign_request_maximize_l.notify = on_foreign_request_maximize;
    toplevel->foreign_request_minimize_l.notify = on_foreign_request_minimize;
    toplevel->foreign_request_fullscreen_l.notify =
        on_foreign_request_fullscreen;
    toplevel->foreign_request_activate_l.notify = on_foreign_request_activate;
    toplevel->foreign_request_close_l.notify    = on_foreign_request_close;
    toplevel->foreign_destroy_l.notify          = on_foreign_destroy;
    wl_signal_add(&toplevel->wlr_foreign_handle->events.request_maximize,
                  &toplevel->foreign_request_maximize_l);
    wl_signal_add(&toplevel->wlr_foreign_handle->events.request_minimize,
                  &toplevel->foreign_request_minimize_l);
    wl_signal_add(&toplevel->wlr_foreign_handle->events.request_fullscreen,
                  &toplevel->foreign_request_fullscreen_l);
    wl_signal_add(&toplevel->wlr_foreign_handle->events.request_activate,
                  &toplevel->foreign_request_activate_l);
    wl_signal_add(&toplevel->wlr_foreign_handle->events.request_close,
                  &toplevel->foreign_request_close_l);
    wl_signal_add(&toplevel->wlr_foreign_handle->events.destroy,
                  &toplevel->foreign_destroy_l);
}

static inline void _fini_unmap_managed_toplevel(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_unmanaged(toplevel))
        return;

    wl_list_remove(&toplevel->link_output_toplevels);

    if (toplevel->wlr_foreign_handle) {
        wlr_foreign_toplevel_handle_v1_destroy(toplevel->wlr_foreign_handle);
        toplevel->wlr_foreign_handle = NULL;
    }

    if (toplevel->ext_foreign_handle) {
        wlr_ext_foreign_toplevel_handle_v1_destroy(
            toplevel->ext_foreign_handle);
        toplevel->ext_foreign_handle = NULL;
    }
}

static void on_surface_map(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel = wl_container_of(listener, toplevel, map_l);
    toplevel->mapped              = true;

    cwc_log(CWC_DEBUG, "mapping toplevel (%s): %p",
            cwc_toplevel_get_title(toplevel), toplevel);

    if (server.insert_marked && !cwc_toplevel_is_unmanaged(toplevel)) {
        cwc_container_insert_toplevel(server.insert_marked, toplevel);
    } else {
        int bw = g_config.border_width;
        cwc_container_init(server.focused_output, toplevel,
                           cwc_toplevel_is_unmanaged(toplevel) ? 0 : bw);
    }

    _init_mapped_managed_toplevel(toplevel);

    cwc_object_emit_signal_simple("client::map", g_config_get_lua_State(),
                                  toplevel);
}

static void on_surface_unmap(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, unmap_l);

    cwc_log(CWC_DEBUG, "unmapping toplevel (%s): %p",
            cwc_toplevel_get_title(toplevel), toplevel);

    // stop interactive when the grabbed toplevel is gone
    struct cwc_cursor *cursor = server.seat->cursor;
    if (cursor->grabbed_toplevel == toplevel)
        stop_interactive(cursor);

    _fini_unmap_managed_toplevel(toplevel);

    toplevel->mapped = false;
    cwc_object_emit_signal_simple("client::unmap", g_config_get_lua_State(),
                                  toplevel);

    // some toplevel lua property depends on the container so remove it last
    cwc_container_remove_toplevel(toplevel);
}

static void on_surface_commit(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, commit_l);

    if (toplevel->xdg_toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
        wlr_xdg_toplevel_set_wm_capabilities(
            toplevel->xdg_toplevel,
            WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE
                // | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE
                | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

        if (toplevel->decoration)
            wlr_xdg_toplevel_decoration_v1_set_mode(
                toplevel->decoration->base,
                WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

        return;
    }

    if (toplevel->resize_serial
        && toplevel->resize_serial
               <= toplevel->xdg_toplevel->base->current.configure_serial) {
        server.resize_count--;
        toplevel->resize_serial = 0;
    }

    if (!toplevel->container || toplevel->xdg_toplevel->current.resizing
        || cwc_container_get_front_toplevel(toplevel->container) != toplevel
        || !cwc_output_is_exist(toplevel->container->output)
        || !cwc_toplevel_is_mapped(toplevel))
        return;

    // tiled toplevel already clipped
    if (!cwc_toplevel_is_floating(toplevel))
        return;

    struct wlr_box geom = cwc_toplevel_get_geometry(toplevel);
    wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node, &geom);
    int thickness = cwc_border_get_thickness(&toplevel->container->border);
    cwc_border_resize(&toplevel->container->border, geom.width + thickness * 2,
                      geom.height + thickness * 2);
}

static void on_request_maximize(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, request_maximize_l);

    if (!cwc_toplevel_is_mapped(toplevel))
        return;

    cwc_toplevel_set_maximized(toplevel,
                               cwc_toplevel_wants_maximized(toplevel));
}

static void on_request_minimize(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, request_minimize_l);

    if (toplevel->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);

    // if (!cwc_toplevel_is_mapped(toplevel))
    //     return;
    //
    // cwc_toplevel_set_minimized(toplevel,
    //                            cwc_toplevel_wants_minimized(toplevel));
}

static void on_request_fullscreen(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, request_fullscreen_l);

    if (!cwc_toplevel_is_mapped(toplevel))
        return;

    cwc_toplevel_set_fullscreen(toplevel,
                                cwc_toplevel_wants_fullscreen(toplevel));
}

static void on_request_resize(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, request_resize_l);

    struct wlr_xdg_toplevel_resize_event *event = data;

    cwc_toplevel_focus(toplevel, true);
    start_interactive_resize(toplevel, event->edges);
}

static void on_request_move(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, request_move_l);

    cwc_toplevel_focus(toplevel, true);
    start_interactive_move(toplevel);
}

static void on_toplevel_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, destroy_l);

    cwc_log(CWC_DEBUG, "destroying toplevel (%s): %p",
            cwc_toplevel_get_title(toplevel), toplevel);

    lua_State *L = g_config_get_lua_State();
    cwc_object_emit_signal_simple("client::destroy", L, toplevel);

    wl_list_remove(&toplevel->link);
    wl_list_remove(&toplevel->destroy_l.link);
    wl_list_remove(&toplevel->request_minimize_l.link);
    wl_list_remove(&toplevel->request_maximize_l.link);
    wl_list_remove(&toplevel->request_fullscreen_l.link);
    wl_list_remove(&toplevel->request_resize_l.link);
    wl_list_remove(&toplevel->request_move_l.link);

    wl_list_remove(&toplevel->set_appid_l.link);
    wl_list_remove(&toplevel->set_title_l.link);

    if (cwc_toplevel_is_x11(toplevel)) {
        wl_list_remove(&toplevel->xwprops->associate_l.link);
        wl_list_remove(&toplevel->xwprops->dissociate_l.link);
        wl_list_remove(&toplevel->xwprops->req_configure_l.link);
        wl_list_remove(&toplevel->xwprops->req_activate_l.link);
        free(toplevel->xwprops);
    } else {
        wl_list_remove(&toplevel->map_l.link);
        wl_list_remove(&toplevel->unmap_l.link);
        wl_list_remove(&toplevel->commit_l.link);
    }

    luaC_object_unregister(L, toplevel);
    free(toplevel);
}

static void ext_foreign_update_handle(struct cwc_toplevel *toplevel)
{
    if (!toplevel->ext_foreign_handle)
        return;

    struct wlr_ext_foreign_toplevel_handle_v1_state state = {
        .title  = cwc_toplevel_get_title(toplevel),
        .app_id = cwc_toplevel_get_app_id(toplevel),
    };
    wlr_ext_foreign_toplevel_handle_v1_update_state(
        toplevel->ext_foreign_handle, &state);
}

static void on_set_title(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, set_title_l);

    ext_foreign_update_handle(toplevel);

    char *title = cwc_toplevel_get_title(toplevel);
    if (toplevel->wlr_foreign_handle && title)
        wlr_foreign_toplevel_handle_v1_set_title(toplevel->wlr_foreign_handle,
                                                 title);
}

static void on_set_app_id(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel *toplevel =
        wl_container_of(listener, toplevel, set_appid_l);

    ext_foreign_update_handle(toplevel);

    char *app_id = cwc_toplevel_get_app_id(toplevel);
    if (toplevel->wlr_foreign_handle && app_id)
        wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->wlr_foreign_handle,
                                                  app_id);
}

/* shared stuff between toplevel for xwayland and xdg_toplevel */
static void cwc_toplevel_init_common_stuff(struct cwc_toplevel *toplevel)
{
    toplevel->destroy_l.notify            = on_toplevel_destroy;
    toplevel->request_maximize_l.notify   = on_request_maximize;
    toplevel->request_minimize_l.notify   = on_request_minimize;
    toplevel->request_fullscreen_l.notify = on_request_fullscreen;
    toplevel->request_resize_l.notify     = on_request_resize;
    toplevel->request_move_l.notify       = on_request_move;

    toplevel->set_title_l.notify = on_set_title;
    toplevel->set_appid_l.notify = on_set_app_id;

    if (cwc_toplevel_is_x11(toplevel)) {
        struct wlr_xwayland_surface *xwsurface = toplevel->xwsurface;
        wl_signal_add(&xwsurface->events.destroy, &toplevel->destroy_l);
        wl_signal_add(&xwsurface->events.request_maximize,
                      &toplevel->request_maximize_l);
        wl_signal_add(&xwsurface->events.request_minimize,
                      &toplevel->request_minimize_l);
        wl_signal_add(&xwsurface->events.request_fullscreen,
                      &toplevel->request_fullscreen_l);
        wl_signal_add(&xwsurface->events.request_resize,
                      &toplevel->request_resize_l);
        wl_signal_add(&xwsurface->events.request_move,
                      &toplevel->request_move_l);

        wl_signal_add(&xwsurface->events.set_title, &toplevel->set_title_l);
        wl_signal_add(&xwsurface->events.set_class, &toplevel->set_appid_l);

    } else {
        struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;
        wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy_l);
        wl_signal_add(&xdg_toplevel->events.request_maximize,
                      &toplevel->request_maximize_l);
        wl_signal_add(&xdg_toplevel->events.request_minimize,
                      &toplevel->request_minimize_l);
        wl_signal_add(&xdg_toplevel->events.request_fullscreen,
                      &toplevel->request_fullscreen_l);
        wl_signal_add(&xdg_toplevel->events.request_resize,
                      &toplevel->request_resize_l);
        wl_signal_add(&xdg_toplevel->events.request_move,
                      &toplevel->request_move_l);

        wl_signal_add(&xdg_toplevel->events.set_title, &toplevel->set_title_l);
        wl_signal_add(&xdg_toplevel->events.set_app_id, &toplevel->set_appid_l);
    }

    wl_list_insert(&server.toplevels, &toplevel->link);

    lua_State *L = g_config_get_lua_State();
    luaC_object_client_register(L, toplevel);
    cwc_object_emit_signal_simple("client::new", L, toplevel);
}

static void on_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_toplevel *xdg_toplevel = data;

    struct cwc_toplevel *toplevel = calloc(1, sizeof(*toplevel));
    toplevel->type                = DATA_TYPE_XDG_SHELL;
    toplevel->xdg_toplevel        = xdg_toplevel;

    xdg_toplevel->base->data = toplevel;

    cwc_log(CWC_DEBUG, "new xdg toplevel (%s): %p",
            cwc_toplevel_get_title(toplevel), toplevel);

    toplevel->map_l.notify    = on_surface_map;
    toplevel->unmap_l.notify  = on_surface_unmap;
    toplevel->commit_l.notify = on_surface_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map_l);
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap,
                  &toplevel->unmap_l);
    wl_signal_add(&xdg_toplevel->base->surface->events.commit,
                  &toplevel->commit_l);

    cwc_toplevel_init_common_stuff(toplevel);
}

static void on_popup_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_popup *popup = wl_container_of(listener, popup, popup_destroy_l);
    cwc_log(CWC_DEBUG, "destroying xdg_popup for parent %p: %p",
            popup->xdg_popup->parent, popup);

    wl_list_remove(&popup->popup_commit_l.link);
    wl_list_remove(&popup->popup_destroy_l.link);

    free(popup);
}

static void on_popup_commit(struct wl_listener *listener, void *data)
{
    struct cwc_popup *popup = wl_container_of(listener, popup, popup_commit_l);
    struct wlr_xdg_popup *xdg_popup = popup->xdg_popup;

    if (!xdg_popup->base->initial_commit)
        return;

    if (!xdg_popup->parent) {
        wlr_xdg_popup_destroy(xdg_popup);
        unreachable_();
        return;
    }

    struct wlr_xdg_popup *parent_popup =
        wlr_xdg_popup_try_from_wlr_surface(xdg_popup->parent);

    struct cwc_toplevel *toplevel          = NULL;
    struct wlr_layer_surface_v1 *layersurf = NULL;

    // TODO: also unconstraint if parent is the popup
    struct wlr_scene_tree *parent_stree;
    if (parent_popup) {
        struct cwc_popup *parent_popup_cwc = parent_popup->base->data;
        parent_stree                       = parent_popup_cwc->scene_tree;

        goto create_popup;
    }

    toplevel  = cwc_toplevel_try_from_wlr_surface(xdg_popup->parent);
    layersurf = wlr_layer_surface_v1_try_from_wlr_surface(xdg_popup->parent);

    struct wlr_box box = {0};
    struct wlr_scene_node *node;
    if (toplevel) {
        parent_stree = toplevel->container->popup_tree;
        box          = toplevel->container->output->output_layout_box;
        node         = &toplevel->container->tree->node;
    } else if (layersurf) {
        struct cwc_layer_surface *l = layersurf->data;
        node                        = &l->scene_layer->tree->node;
        parent_stree                = l->popup_tree;
        box                         = l->output->output_layout_box;
        box.x                       = 0;
        box.y                       = 0;
    } else {
        unreachable_();
        return;
    }
    box.x -= node->x;
    box.y -= node->y;

    wlr_xdg_popup_unconstrain_from_box(xdg_popup, &box);

create_popup:
    popup->scene_tree =
        wlr_scene_xdg_surface_create(parent_stree, xdg_popup->base);
    popup->scene_tree->node.data = popup;
    wlr_scene_node_raise_to_top(&popup->scene_tree->node);
    wlr_xdg_surface_schedule_configure(xdg_popup->base);
}

void on_new_xdg_popup(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_popup *xdg_popup = data;

    struct cwc_popup *popup = calloc(1, sizeof(*popup));
    popup->type             = DATA_TYPE_POPUP;
    popup->xdg_popup        = xdg_popup;
    xdg_popup->base->data   = popup;

    cwc_log(CWC_DEBUG, "new xdg_popup for parent %p: %p", xdg_popup->parent,
            popup);

    popup->popup_destroy_l.notify = on_popup_destroy;
    popup->popup_commit_l.notify  = on_popup_commit;
    wl_signal_add(&popup->xdg_popup->events.destroy, &popup->popup_destroy_l);
    wl_signal_add(&popup->xdg_popup->base->surface->events.commit,
                  &popup->popup_commit_l);
}

struct cwc_toplevel *wlr_xdg_popup_get_cwc_toplevel(struct wlr_xdg_popup *popup)
{
    struct wlr_surface *parent = popup->parent;
    struct wlr_xdg_surface *xdg_surface;
    while ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(parent))) {
        if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
            return xdg_surface->data;

        if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP
            && xdg_surface->popup)
            parent = xdg_surface->popup->parent;
        else
            break;
    }

    return NULL;
}

static void on_activation_request_activate(struct wl_listener *listener,
                                           void *data)
{
    struct wlr_xdg_activation_v1_request_activate_event *event = data;

    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(event->surface);
    if (!toplevel)
        return;

    if (cwc_toplevel_is_mapped(toplevel)) {
        cwc_toplevel_set_urgent(toplevel, true);
    } else {
        toplevel->urgent = true;
    }
}

void setup_xdg_shell(struct cwc_server *s)
{
    s->xdg_shell                 = wlr_xdg_shell_create(s->wl_display, 6);
    s->new_xdg_toplevel_l.notify = on_new_xdg_toplevel;
    s->new_xdg_popup_l.notify    = on_new_xdg_popup;
    wl_signal_add(&s->xdg_shell->events.new_toplevel, &s->new_xdg_toplevel_l);
    wl_signal_add(&s->xdg_shell->events.new_popup, &s->new_xdg_popup_l);

    s->xdg_activation            = wlr_xdg_activation_v1_create(s->wl_display);
    s->request_activate_l.notify = on_activation_request_activate;
    wl_signal_add(&s->xdg_activation->events.request_activate,
                  &s->request_activate_l);
}

void cleanup_xdg_shell(struct cwc_server *s)
{
    wl_list_remove(&s->new_xdg_toplevel_l.link);
    wl_list_remove(&s->new_xdg_popup_l.link);

    wl_list_remove(&s->request_activate_l.link);
}

void cwc_toplevel_focus(struct cwc_toplevel *toplevel, bool raise)
{
    struct wlr_seat *seat = server.seat->wlr_seat;
    if (toplevel == NULL || !cwc_toplevel_is_mapped(toplevel)) {
        wlr_seat_keyboard_notify_clear_focus(seat);
        return;
    }

    struct wlr_surface *wlr_surface  = cwc_toplevel_get_wlr_surface(toplevel);
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

    if (wlr_surface == prev_surface)
        return;

    if (!cwc_toplevel_is_unmanaged(toplevel))
        wl_list_reattach(&toplevel->container->output->state->focus_stack,
                         &toplevel->container->link_output_fstack);

    /* don't emit signal in process cursor motion called from this function
     * because it'll ruin the focus stack as it notify enter any random surface
     * under the cursor. */
    struct cwc_cursor *cursor = server.seat->cursor;
    cursor->dont_emit_signal  = true;

    /* set_activate first so the keyboard focus change can validate */
    cwc_toplevel_set_activated(toplevel, true);
    process_cursor_motion(cursor, 0, NULL, 0, 0, 0, 0);
    keyboard_focus_surface(seat->data, wlr_surface);
    cwc_toplevel_set_urgent(toplevel, false);

    if (raise)
        wlr_scene_node_raise_to_top(&toplevel->container->tree->node);
}

void cwc_toplevel_jump_to(struct cwc_toplevel *toplevel, bool merge)
{
    cwc_toplevel_focus(toplevel, true);
    cwc_container_set_front_toplevel(toplevel);

    /* change the tag if its not visible */
    if (!cwc_toplevel_is_visible(toplevel)) {
        if (merge) {
            struct cwc_output *output = toplevel->container->output;
            cwc_output_set_active_tag(output, output->state->active_tag
                                                  | toplevel->container->tag);
        } else {
            cwc_output_set_view_only(toplevel->container->output,
                                     toplevel->container->workspace);
        }
    }

    if (cwc_toplevel_is_minimized(toplevel))
        cwc_toplevel_set_minimized(toplevel, false);
}

struct cwc_toplevel *
cwc_toplevel_get_nearest_by_direction(struct cwc_toplevel *reference,
                                      enum wlr_direction dir)
{
    // TODO: add global direction option
    struct cwc_toplevel **toplevels =
        cwc_output_get_visible_toplevels(reference->container->output);

    int reference_lx, reference_ly;
    wlr_scene_node_coords(&reference->container->tree->node, &reference_lx,
                          &reference_ly);

    double nearest_distance               = DBL_MAX;
    struct cwc_toplevel *nearest_toplevel = NULL;
    int i                                 = 0;
    struct cwc_toplevel *pointed_toplevel = toplevels[i];
    while (pointed_toplevel != NULL) {
        if (pointed_toplevel == reference)
            goto next;

        int lx, ly;
        wlr_scene_node_coords(&pointed_toplevel->container->tree->node, &lx,
                              &ly);

        int x = lx - reference_lx;
        int y = ly - reference_ly;

        if (!x && !y)
            goto next;

        if (!is_direction_match(dir, x, y))
            goto next;

        double _distance = distance(lx, ly, reference_lx, reference_ly);
        if (nearest_distance > _distance) {
            nearest_distance = _distance;
            nearest_toplevel = pointed_toplevel;
        }

    next:
        pointed_toplevel = toplevels[++i];
    }

    free(toplevels);
    return nearest_toplevel;
}

struct cwc_toplevel *cwc_toplevel_get_focused()
{
    struct wlr_surface *surf =
        server.seat->wlr_seat->keyboard_state.focused_surface;
    if (surf)
        return cwc_toplevel_try_from_wlr_surface(surf);

    return NULL;
}

struct wlr_box cwc_toplevel_get_box(struct cwc_toplevel *toplevel)
{
    struct wlr_box box = cwc_toplevel_get_geometry(toplevel);
    wlr_scene_node_coords(&toplevel->surf_tree->node, &box.x, &box.y);

    return box;
}

struct wlr_surface *
scene_surface_at(double lx, double ly, double *sx, double *sy)
{
    struct wlr_scene_node *node_under =
        wlr_scene_node_at(&server.scene->tree.node, lx, ly, sx, sy);

    if (node_under == NULL || node_under->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node_under);
    struct wlr_scene_surface *surface =
        wlr_scene_surface_try_from_buffer(buffer);
    if (surface == NULL) {
        return NULL;
    }

    return surface->surface;
}

static void on_set_decoration_mode(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel_decoration *deco =
        wl_container_of(listener, deco, set_decoration_mode_l);
    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(deco->base->toplevel->base->surface);

    if (toplevel->xdg_toplevel->base->initialized)
        wlr_xdg_toplevel_decoration_v1_set_mode(
            deco->base, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void on_decoration_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_toplevel_decoration *deco =
        wl_container_of(listener, deco, destroy_l);
    wl_list_remove(&deco->destroy_l.link);
    wl_list_remove(&deco->set_decoration_mode_l.link);
    free(deco);
}

static void on_new_toplevel_decoration(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_toplevel_decoration_v1 *deco = data;
    struct cwc_toplevel_decoration *cwc_deco    = malloc(sizeof(*cwc_deco));
    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(deco->toplevel->base->surface);
    toplevel->decoration = cwc_deco;

    cwc_deco->base                         = deco;
    cwc_deco->set_decoration_mode_l.notify = on_set_decoration_mode;
    cwc_deco->destroy_l.notify             = on_decoration_destroy;
    wl_signal_add(&deco->events.request_mode, &cwc_deco->set_decoration_mode_l);
    wl_signal_add(&deco->events.destroy, &cwc_deco->destroy_l);
}

void setup_decoration_manager(struct cwc_server *s)
{
    wlr_server_decoration_manager_set_default_mode(
        wlr_server_decoration_manager_create(s->wl_display),
        WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    s->xdg_decoration_manager =
        wlr_xdg_decoration_manager_v1_create(s->wl_display);

    s->new_decoration_l.notify = on_new_toplevel_decoration;
    wl_signal_add(&s->xdg_decoration_manager->events.new_toplevel_decoration,
                  &s->new_decoration_l);
}

void cleanup_decoration_manager(struct cwc_server *s)
{
    wl_list_remove(&s->new_decoration_l.link);
}

//================ XWAYLAND ==================

/* - */
static void on_request_configure(struct wl_listener *listener, void *data)
{
    struct xwayland_props *props =
        wl_container_of(listener, props, req_configure_l);
    struct cwc_toplevel *toplevel                      = props->toplevel;
    struct wlr_xwayland_surface *surface               = toplevel->xwsurface;
    struct wlr_xwayland_surface_configure_event *event = data;

    if (toplevel->container)
        cwc_container_set_position_global(toplevel->container, event->x,
                                          event->y);

    wlr_xwayland_surface_configure(surface, event->x, event->y, event->width,
                                   event->height);
}

static void on_request_activate(struct wl_listener *listener, void *data)
{
    struct xwayland_props *props =
        wl_container_of(listener, props, req_activate_l);
    struct cwc_toplevel *toplevel = props->toplevel;

    if (!cwc_toplevel_is_unmanaged(toplevel))
        wlr_xwayland_surface_activate(toplevel->xwsurface, true);
}

static void on_associate(struct wl_listener *listener, void *data)
{
    struct xwayland_props *props =
        wl_container_of(listener, props, associate_l);
    struct cwc_toplevel *toplevel = props->toplevel;

    toplevel->map_l.notify   = on_surface_map;
    toplevel->unmap_l.notify = on_surface_unmap;
    wl_signal_add(&toplevel->xwsurface->surface->events.map, &toplevel->map_l);
    wl_signal_add(&toplevel->xwsurface->surface->events.unmap,
                  &toplevel->unmap_l);
}

static void on_dissociate(struct wl_listener *listener, void *data)
{
    struct xwayland_props *props =
        wl_container_of(listener, props, dissociate_l);
    struct cwc_toplevel *toplevel = props->toplevel;

    wl_list_remove(&toplevel->map_l.link);
    wl_list_remove(&toplevel->unmap_l.link);
}

static void on_xwayland_new_surface(struct wl_listener *listener, void *data)
{
    struct wlr_xwayland_surface *xwsurface = data;

    struct cwc_toplevel *toplevel  = calloc(1, sizeof(*toplevel));
    struct xwayland_props *xwprops = calloc(1, sizeof(*xwprops));
    toplevel->type                 = DATA_TYPE_XWAYLAND;
    toplevel->xwsurface            = xwsurface;
    toplevel->xwprops              = xwprops;

    xwsurface->data = toplevel;

    cwc_log(CWC_DEBUG, "new xwayland client (%s): %p",
            cwc_toplevel_get_title(toplevel), toplevel);

    xwprops->toplevel               = toplevel;
    xwprops->associate_l.notify     = on_associate;
    xwprops->dissociate_l.notify    = on_dissociate;
    xwprops->req_configure_l.notify = on_request_configure;
    xwprops->req_activate_l.notify  = on_request_activate;
    wl_signal_add(&xwsurface->events.associate, &xwprops->associate_l);
    wl_signal_add(&xwsurface->events.dissociate, &xwprops->dissociate_l);
    wl_signal_add(&xwsurface->events.request_configure,
                  &xwprops->req_configure_l);
    wl_signal_add(&xwsurface->events.request_activate,
                  &xwprops->req_activate_l);

    cwc_toplevel_init_common_stuff(toplevel);
}

static void on_xwayland_ready(struct wl_listener *listener, void *data)
{
    wlr_xwayland_set_seat(server.xwayland, server.seat->wlr_seat);

    struct wlr_xcursor *xcursor;
    if ((xcursor = wlr_xcursor_manager_get_xcursor(
             server.seat->cursor->xcursor_mgr, "default", 1)))
        wlr_xwayland_set_cursor(
            server.xwayland, xcursor->images[0]->buffer,
            xcursor->images[0]->width * 4, xcursor->images[0]->width,
            xcursor->images[0]->height, xcursor->images[0]->hotspot_x,
            xcursor->images[0]->hotspot_y);
}

void xwayland_init(struct cwc_server *s)
{
    s->xwayland = wlr_xwayland_create(s->wl_display, s->compositor, true);

    if (!s->xwayland) {
        cwc_log(CWC_ERROR, "Cannot initialize xwayland");
        return;
    }

    setenv("DISPLAY", s->xwayland->display_name, true);
    s->xw_ready_l.notify       = on_xwayland_ready;
    s->xw_new_surface_l.notify = on_xwayland_new_surface;
    wl_signal_add(&s->xwayland->events.ready, &s->xw_ready_l);
    wl_signal_add(&s->xwayland->events.new_surface, &s->xw_new_surface_l);
}

void xwayland_fini(struct cwc_server *s)
{
    unsetenv("DISPLAY");

    wl_list_remove(&s->xw_ready_l.link);
    wl_list_remove(&s->xw_new_surface_l.link);

    wlr_xwayland_destroy(s->xwayland);
    s->xwayland = NULL;
}

//================= TOPLEVEL ACTIONS =======================

void cwc_toplevel_send_close(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_close(toplevel->xwsurface);
        return;
    }

    wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
}

void cwc_toplevel_kill(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        xcb_connection_t *conn =
            wlr_xwayland_get_xwm_connection(server.xwayland);
        xcb_kill_client(conn, toplevel->xwsurface->window_id);
        xcb_flush(conn);
        return;
    }

    wl_client_destroy(toplevel->xdg_toplevel->base->client->client);
}

void cwc_toplevel_swap(struct cwc_toplevel *source, struct cwc_toplevel *target)
{
    struct cwc_container *c_src = source->container;
    struct cwc_container *d_src = target->container;
    if (c_src == d_src || source == target)
        return;

    cwc_container_remove_toplevel_but_dont_destroy_container_when_empty(source);
    cwc_container_remove_toplevel_but_dont_destroy_container_when_empty(target);
    cwc_container_insert_toplevel(c_src, target);
    cwc_container_insert_toplevel(d_src, source);
    wl_list_swap(&source->link_output_toplevels,
                 &target->link_output_toplevels);
    wl_list_swap(&source->link, &target->link);

    cwc_container_refresh(c_src);
    cwc_container_refresh(d_src);

    cwc_object_emit_signal_varr("client::swap", g_config_get_lua_State(), 2,
                                source, target);
}

struct cwc_toplevel *
cwc_toplevel_try_from_wlr_surface(struct wlr_surface *surface)
{
    if (!surface)
        return NULL;

    struct wlr_xdg_toplevel *xdg_toplevel =
        wlr_xdg_toplevel_try_from_wlr_surface(surface);

    if (xdg_toplevel) {
        cwc_data_interface_t *data = xdg_toplevel->base->data;
        if (data->type == DATA_TYPE_XDG_SHELL)
            return (struct cwc_toplevel *)data;
    }

    struct wlr_xwayland_surface *wlr_xwayland =
        wlr_xwayland_surface_try_from_wlr_surface(surface);

    if (wlr_xwayland) {
        cwc_data_interface_t *data = wlr_xwayland->data;
        if (data->type == DATA_TYPE_XWAYLAND)
            return (struct cwc_toplevel *)data;
    }

    return NULL;
}

struct wlr_box cwc_toplevel_get_geometry(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        return (struct wlr_box){
            .x      = toplevel->xwsurface->x,
            .y      = toplevel->xwsurface->y,
            .width  = toplevel->xwsurface->width,
            .height = toplevel->xwsurface->height,
        };
    }

    return toplevel->xdg_toplevel->base->geometry;
}

void cwc_toplevel_set_size_surface(struct cwc_toplevel *toplevel, int w, int h)
{
    int gaps = cwc_output_get_current_tag_info(toplevel->container->output)
                   ->useless_gaps;
    int outside_width =
        (cwc_border_get_thickness(&toplevel->container->border) + gaps) * 2;

    cwc_container_set_size(toplevel->container, w + outside_width,
                           h + outside_width);
}

void cwc_toplevel_set_position(struct cwc_toplevel *toplevel, int x, int y)
{
    int bw = cwc_border_get_thickness(&toplevel->container->border);
    cwc_container_set_position(toplevel->container, x - bw, y - bw);
}

void cwc_toplevel_set_position_global(struct cwc_toplevel *toplevel,
                                      int x,
                                      int y)
{
    int bw = cwc_border_get_thickness(&toplevel->container->border);
    cwc_container_set_position_global(toplevel->container, x - bw, y - bw);
}

struct cwc_toplevel *
cwc_toplevel_at(double lx, double ly, double *sx, double *sy)
{
    struct wlr_surface *surf      = scene_surface_at(lx, ly, sx, sy);
    struct cwc_toplevel *toplevel = NULL;

    if (surf)
        toplevel = cwc_toplevel_try_from_wlr_surface(surf);

    if (toplevel)
        return toplevel;

    return NULL;
}

struct cwc_toplevel *
cwc_toplevel_at_with_deep_check(double lx, double ly, double *sx, double *sy)
{
    struct wlr_scene_node *under =
        wlr_scene_node_at(&server.scene->tree.node, lx, ly, NULL, NULL);

    if (!under || under->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    // search for container node
    bool found                    = false;
    struct wlr_scene_tree *parent = wl_container_of(under, parent, node);
    while ((parent = parent->node.parent)) {
        if (!parent->node.data)
            continue;

        cwc_data_interface_t *data = parent->node.data;
        if (data->type != DATA_TYPE_CONTAINER)
            continue;

        found = true;
        break;
    }

    if (!found)
        return NULL;

    // search for the first toplevel in the container tree
    struct cwc_toplevel *toplevel = NULL;
    struct wlr_scene_node *node;
    wl_list_for_each(node, &parent->children, link)
    {
        if (!node->data)
            continue;

        cwc_data_interface_t *data = node->data;
        if (data->type != DATA_TYPE_XWAYLAND
            && data->type != DATA_TYPE_XDG_SHELL)
            continue;

        toplevel = node->data;
    }

    if (!toplevel)
        return NULL;

    if (sx)
        *sx = lx - toplevel->container->tree->node.x;
    if (sy)
        *sy = ly - toplevel->container->tree->node.y;

    return toplevel;
}

struct cwc_toplevel *cwc_toplevel_at_tiled(double lx, double ly)
{
    struct cwc_container *container;
    wl_list_for_each(container, &server.containers, link)
    {
        if (cwc_container_is_floating(container)
            || !cwc_container_is_visible(container))
            continue;

        struct wlr_box box = cwc_container_get_box(container);

        if (wlr_box_contains_point(&box, lx, ly))
            return cwc_container_get_front_toplevel(container);
    }

    return NULL;
}

inline bool cwc_toplevel_is_visible(struct cwc_toplevel *toplevel)
{
    if (cwc_container_is_visible(toplevel->container)
        && (cwc_container_get_front_toplevel(toplevel->container) == toplevel))
        return true;

    return false;
}

bool cwc_toplevel_should_float(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        struct wlr_xwayland_surface *surface = toplevel->xwsurface;
        xcb_size_hints_t *size_hints         = surface->size_hints;
        if (surface->modal)
            return 1;

        return size_hints && size_hints->min_width > 0
               && size_hints->min_height > 0
               && (size_hints->max_width == size_hints->min_width
                   || size_hints->max_height == size_hints->min_height);
    }

    struct wlr_xdg_toplevel_state state = toplevel->xdg_toplevel->current;
    return toplevel->xdg_toplevel->parent
           || (state.min_width != 0 && state.min_height != 0
               && (state.min_width == state.max_width
                   || state.min_height == state.max_height));
}

void cwc_toplevel_set_tiled(struct cwc_toplevel *toplevel, uint32_t edges)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_set_maximized(toplevel->xwsurface,
                                           edges != WLR_EDGE_NONE,
                                           edges != WLR_EDGE_NONE);
        return;
    }

    if (wl_resource_get_version(toplevel->xdg_toplevel->resource)
        >= XDG_TOPLEVEL_STATE_TILED_RIGHT_SINCE_VERSION) {
        wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel, edges);
    } else {
        wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel,
                                       edges != WLR_EDGE_NONE);
    }
}

bool cwc_toplevel_is_ontop(struct cwc_toplevel *toplevel)
{
    if (toplevel->container->tree->node.parent == server.root.top)
        return true;

    return false;
}

void cwc_toplevel_set_ontop(struct cwc_toplevel *toplevel, bool set)
{
    if (set) {
        wlr_scene_node_reparent(&toplevel->container->tree->node,
                                server.root.top);
        return;
    }

    wlr_scene_node_reparent(&toplevel->container->tree->node,
                            server.root.toplevel);
}

bool cwc_toplevel_is_above(struct cwc_toplevel *toplevel)
{
    if (toplevel->container->tree->node.parent == server.root.above)
        return true;

    return false;
}

void cwc_toplevel_set_above(struct cwc_toplevel *toplevel, bool set)
{
    if (set) {
        wlr_scene_node_reparent(&toplevel->container->tree->node,
                                server.root.above);
        return;
    }

    wlr_scene_node_reparent(&toplevel->container->tree->node,
                            server.root.toplevel);
}

bool cwc_toplevel_is_below(struct cwc_toplevel *toplevel)
{
    if (toplevel->container->tree->node.parent == server.root.below)
        return true;

    return false;
}

void cwc_toplevel_set_below(struct cwc_toplevel *toplevel, bool set)
{
    if (set) {
        wlr_scene_node_reparent(&toplevel->container->tree->node,
                                server.root.below);
        return;
    }

    wlr_scene_node_reparent(&toplevel->container->tree->node,
                            server.root.toplevel);
}

bool cwc_toplevel_is_urgent(struct cwc_toplevel *toplevel)
{
    return toplevel->urgent;
}

void cwc_toplevel_set_urgent(struct cwc_toplevel *toplevel, bool set)
{
    toplevel->urgent = set;
    cwc_object_emit_signal_simple("client::property::urgent",
                                  g_config_get_lua_State(), toplevel);
}

void layout_coord_to_surface_coord(
    struct wlr_scene_node *surface_node, int lx, int ly, int *res_x, int *res_y)
{
    int sx, sy;
    wlr_scene_node_coords(surface_node, &sx, &sy);

    *res_x = lx - sx;
    *res_y = ly - sy;
}

void surface_coord_to_normdevice_coord(
    struct wlr_box geo_box, double sx, double sy, double *nx, double *ny)
{
    *nx = sx / ((double)geo_box.width / 2) - 1;
    *ny = sy / ((double)geo_box.height / 2) - 1;
}
