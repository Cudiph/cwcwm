/*
 * Copyright © 2020 dwl team
 * See LICENSE.md file for copyright and license details.
 */

#ifndef _CWC_TOPLEVEL_H
#define _CWC_TOPLEVEL_H

#include <stdbool.h>
#include <sys/types.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/xwayland.h>

#include "cwc/layout/container.h"
#include "cwc/types.h"

struct cwc_server;
struct xwayland_props;

//================ XDG SHELL ================

struct cwc_toplevel_decoration {
    struct wlr_xdg_toplevel_decoration_v1 *base;
    struct wl_listener set_decoration_mode_l;
    struct wl_listener destroy_l;
};

struct cwc_toplevel {
    enum cwc_data_type type;
    struct wl_list link; // cwc_server.toplevels
    union {
        struct wlr_xwayland_surface *xwsurface;
        struct wlr_xdg_toplevel *xdg_toplevel;
    };
    struct wlr_scene_tree *surf_tree;
    struct xwayland_props *xwprops; // NULL if type != xwayland
    struct cwc_container *container;

    struct cwc_toplevel_decoration *decoration;
    bool mapped;
    bool tearing_hint;

    struct wl_list link_output_toplevels; // cwc_output.toplevels
    struct wl_list link_container;        // cwc_container.toplevels

    struct wl_listener map_l;
    struct wl_listener unmap_l;
    struct wl_listener commit_l;
    struct wl_listener destroy_l;

    struct wl_listener request_maximize_l;
    struct wl_listener request_minimize_l;
    struct wl_listener request_fullscreen_l;
    struct wl_listener request_move_l;
    struct wl_listener request_resize_l;
};

struct cwc_popup {
    enum cwc_data_type type;
    struct wlr_xdg_popup *xdg_popup;
    struct wlr_scene_tree *scene_tree;

    struct wl_listener popup_destroy_l;
    struct wl_listener popup_commit_l;
};

void on_new_xdg_popup(struct wl_listener *listener, void *data);

void cwc_toplevel_focus(struct cwc_toplevel *toplevel, bool raise);
struct cwc_toplevel *cwc_toplevel_get_focused();

/* find shortest toplevel within 90deg field of view */
struct cwc_toplevel *
cwc_toplevel_get_nearest_by_direction(struct cwc_toplevel *toplevel,
                                      enum wlr_direction dir);

struct wlr_surface *
scene_surface_at(double lx, double ly, double *sx, double *sy);

//================ XWAYLAND ==================

/* additional toplevel props for xwayland shell */
struct xwayland_props {
    struct cwc_toplevel *toplevel;

    struct wl_listener associate_l;
    struct wl_listener dissociate_l;

    struct wl_listener req_configure_l;
    struct wl_listener req_activate_l;
};

void cwc_toplevel_send_close(struct cwc_toplevel *toplevel);
void cwc_toplevel_kill(struct cwc_toplevel *toplevel);

void cwc_toplevel_swap(struct cwc_toplevel *source,
                       struct cwc_toplevel *target);

struct wlr_box cwc_toplevel_get_geometry(struct cwc_toplevel *toplevel);
struct wlr_box cwc_toplevel_get_box(struct cwc_toplevel *toplevel);

struct cwc_toplevel *
cwc_toplevel_try_from_wlr_surface(struct wlr_surface *surface);

struct cwc_toplevel *
cwc_toplevel_at(double lx, double ly, double *sx, double *sy);

struct cwc_toplevel *
cwc_toplevel_at_with_deep_check(double lx, double ly, double *sx, double *sy);

/* resize the toplevel surface  */
void cwc_toplevel_set_size_surface(struct cwc_toplevel *toplevel, int w, int h);

void cwc_toplevel_set_tiled(struct cwc_toplevel *toplevel, uint32_t edges);
void cwc_toplevel_set_ontop(struct cwc_toplevel *toplevel, bool set);
void cwc_toplevel_set_above(struct cwc_toplevel *toplevel, bool set);
void cwc_toplevel_set_below(struct cwc_toplevel *toplevel, bool set);

/* move the toplevel surface */
void cwc_toplevel_set_position(struct cwc_toplevel *toplevel, int x, int y);

bool cwc_toplevel_is_ontop(struct cwc_toplevel *toplevel);
bool cwc_toplevel_is_above(struct cwc_toplevel *toplevel);
bool cwc_toplevel_is_below(struct cwc_toplevel *toplevel);

/* check if toplevel is visible or rendered on the screen */
bool cwc_toplevel_is_visible(struct cwc_toplevel *toplevel);

/* check if toplevel is visible on specified view */
bool cwc_toplevel_is_visible_in_view(struct cwc_toplevel *toplevel, int view);

bool cwc_toplevel_should_float(struct cwc_toplevel *toplevel);

/* surface_node is node from the toplevel */
void layout_coord_to_surface_coord(struct wlr_scene_node *surface_node,
                                   int lx,
                                   int ly,
                                   int *res_x,
                                   int *res_y);

/* normalized device coordinate is coordinate in range [-1, 1]
 *
 * geo_box is geometry from toplevel
 */
void surface_coord_to_normdevice_coord(
    struct wlr_box geo_box, double sx, double sy, double *nx, double *ny);

//================== TOPLEVEL TO CONTAINER ======================

/* Before container was made, the only operation is straight to the toplevel
 * struct. Following functions act as the compability layer. Functions with the
 * same name (suffix) between toplevel and container is equivalent operation.
 */

#define CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(name)                       \
    static inline bool cwc_toplevel_is_##name(struct cwc_toplevel *toplevel)  \
    {                                                                         \
        return cwc_container_is_##name(toplevel->container);                  \
    }                                                                         \
                                                                              \
    static inline void cwc_toplevel_set_##name(struct cwc_toplevel *toplevel, \
                                               bool set)                      \
    {                                                                         \
        cwc_container_set_##name(toplevel->container, set);                   \
    }

CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(floating)
CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(minimized)
CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(maximized)
CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(fullscreen)
CWC_TOPLEVEL_FORWARD_BOOLEAN_TO_CONTAINER(sticky)

static inline void cwc_toplevel_move_to_tag(struct cwc_toplevel *toplevel,
                                            int tagidx)
{
    cwc_container_move_to_tag(toplevel->container, tagidx);
}

static inline void cwc_toplevel_to_center(struct cwc_toplevel *toplevel)
{
    cwc_container_to_center(toplevel->container);
}

static inline float cwc_toplevel_get_opacity(struct cwc_toplevel *toplevel)
{
    return cwc_container_get_opacity(toplevel->container);
}

static inline void cwc_toplevel_set_opacity(struct cwc_toplevel *toplevel,
                                            float opacity)
{
    cwc_container_set_opacity(toplevel->container, opacity);
}

//=============== MACRO ==================

static inline bool cwc_toplevel_is_x11(struct cwc_toplevel *toplevel)
{
    return toplevel->type == DATA_TYPE_XWAYLAND;
}

static inline bool cwc_toplevel_is_unmanaged(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->override_redirect;
    return false;
}

static inline void cwc_toplevel_set_suspended(struct cwc_toplevel *toplevel,
                                              bool set)
{
    if (cwc_toplevel_is_x11(toplevel))
        return;

    wlr_xdg_toplevel_set_suspended(toplevel->xdg_toplevel, set);
}

/* name conflict with the container */
static inline void __cwc_toplevel_set_fullscreen(struct cwc_toplevel *toplevel,
                                                 bool set)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_set_fullscreen(toplevel->xwsurface, set);
        return;
    }

    wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, set);
}

static inline void __cwc_toplevel_set_maximized(struct cwc_toplevel *toplevel,
                                                bool set)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_set_maximized(toplevel->xwsurface, set, set);
        return;
    }

    wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, set);
}

static inline void __cwc_toplevel_set_minimized(struct cwc_toplevel *toplevel,
                                                bool set)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_set_minimized(toplevel->xwsurface, set);
        return;
    }

    wlr_xdg_toplevel_set_suspended(toplevel->xdg_toplevel, set);
    return;
}

static inline void cwc_toplevel_set_activated(struct cwc_toplevel *toplevel,
                                              bool activated)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_activate(toplevel->xwsurface, activated);
        return;
    }

    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, activated);
}

static inline void
cwc_toplevel_set_size(struct cwc_toplevel *toplevel, int w, int h)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        wlr_xwayland_surface_configure(toplevel->xwsurface,
                                       toplevel->xwsurface->x,
                                       toplevel->xwsurface->y, w, h);
        return;
    }

    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, w, h);
}

static inline bool cwc_toplevel_is_mapped(struct cwc_toplevel *toplevel)
{
    return toplevel->mapped;
}

static inline bool cwc_toplevel_is_tileable(struct cwc_toplevel *toplevel)
{
    return cwc_toplevel_is_visible(toplevel)
           && !cwc_toplevel_is_floating(toplevel)
           && !cwc_toplevel_is_fullscreen(toplevel)
           && !cwc_toplevel_is_maximized(toplevel)
           && !cwc_toplevel_is_unmanaged(toplevel);
}

static inline bool
cwc_toplevel_is_front_in_container(struct cwc_toplevel *toplevel)
{
    return cwc_container_get_front_toplevel(toplevel->container) == toplevel;
}

static inline bool
cwc_toplevel_is_configure_allowed(struct cwc_toplevel *toplevel)
{
    return !cwc_toplevel_is_fullscreen(toplevel)
           && !cwc_toplevel_is_maximized(toplevel);
}

static inline bool cwc_toplevel_wants_fullscreen(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->fullscreen;

    return toplevel->xdg_toplevel->requested.fullscreen;
}

static inline bool cwc_toplevel_wants_maximized(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->maximized_horz
               && toplevel->xwsurface->maximized_vert;

    return toplevel->xdg_toplevel->requested.maximized;
}

static inline bool cwc_toplevel_wants_minimized(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->minimized;

    return toplevel->xdg_toplevel->requested.minimized;
}

/* whether toplevel allowed to resize/move interactively */
static inline bool
cwc_toplevel_can_enter_interactive(struct cwc_toplevel *toplevel)
{
    // TODO: interactive move/resize for tiling
    if (!cwc_toplevel_is_floating(toplevel))
        return false;

    return !cwc_toplevel_is_fullscreen(toplevel)
           && !cwc_toplevel_is_maximized(toplevel)
           && !cwc_toplevel_is_unmanaged(toplevel);
}

static inline struct wlr_surface *
cwc_toplevel_get_wlr_surface(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->surface;

    return toplevel->xdg_toplevel->base->surface;
}

static inline struct cwc_toplevel *
cwc_toplevel_get_parent(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel)) {
        if (toplevel->xwsurface->parent)
            return toplevel->xwsurface->parent->data;

        return NULL;
    }

    if (toplevel->xdg_toplevel->parent)
        return toplevel->xdg_toplevel->parent->base->data;

    return NULL;
}

static inline char *cwc_toplevel_get_title(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->title;

    return toplevel->xdg_toplevel->title;
}

static inline pid_t cwc_toplevel_get_pid(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->pid;

    pid_t pid = 0;
    wl_client_get_credentials(toplevel->xdg_toplevel->base->client->client,
                              &pid, NULL, NULL);
    return pid;
}

static inline char *cwc_toplevel_get_app_id(struct cwc_toplevel *toplevel)
{
    if (cwc_toplevel_is_x11(toplevel))
        return toplevel->xwsurface->class;

    return toplevel->xdg_toplevel->app_id;
}

static inline struct cwc_container *
cwc_toplevel_get_container(struct cwc_toplevel *toplevel)
{
    return toplevel->container;
}

static inline void cwc_toplevel_set_allow_tearing(struct cwc_toplevel *toplevel,
                                                  bool set)
{
    toplevel->tearing_hint = set;
}

static inline bool cwc_toplevel_is_allow_tearing(struct cwc_toplevel *toplevel)
{
    return toplevel->tearing_hint;
}

#endif // !_CWC_TOPLEVEL_H
