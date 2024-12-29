#ifndef _CWC_CONTAINER_H
#define _CWC_CONTAINER_H
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/box.h>

#include "cwc/types.h"

struct cwc_toplevel;

enum container_state_mask {
    CONTAINER_STATE_UNMANAGED  = 1 << 0,
    CONTAINER_STATE_FLOATING   = 1 << 1, // false mean tiled
    CONTAINER_STATE_MINIMIZED  = 1 << 2,
    CONTAINER_STATE_MAXIMIZED  = 1 << 3,
    CONTAINER_STATE_FULLSCREEN = 1 << 4,
    CONTAINER_STATE_STICKY     = 1 << 5,
};

struct border_buffer {
    struct wlr_buffer base;
    struct _cairo_surface *surface;
    struct wlr_scene_buffer *scene;
};

struct cwc_border {
    enum cwc_data_type type;
    int thickness;     // border_width
    int width, height; // rectangle
    struct _cairo_pattern *pattern;
    bool enabled;
    struct wlr_scene_tree *attached_tree;
    struct border_buffer *buffer[4]; // clockwise top to left
};

void cwc_border_init(struct cwc_border *border,
                     struct _cairo_pattern *pattern,
                     int rect_w,
                     int rect_h,
                     int thickness);

void cwc_border_destroy(struct cwc_border *border);

void cwc_border_attach_to_scene(struct cwc_border *border,
                                struct wlr_scene_tree *scene_tree);

void cwc_border_set_enabled(struct cwc_border *border, bool enabled);

void cwc_border_set_pattern(struct cwc_border *border,
                            struct _cairo_pattern *pattern);

int cwc_border_get_thickness(struct cwc_border *border);

/* noop if the surface width unchanged */
void cwc_border_resize(struct cwc_border *border, int rect_w, int rect_h);

struct cwc_container {
    enum cwc_data_type type;
    struct wl_list link;
    struct wlr_scene_tree *tree;
    struct wlr_scene_tree *popup_tree; // or anything that should above toplevel
    struct cwc_border border;
    int width, height;
    float opacity;

    struct wlr_box floating_box;
    container_state_bitfield_t state;

    struct cwc_output *output;
    tag_bitfield_t tag;
    int workspace;

    /* node that will be used in bsp layout */
    struct bsp_node *bsp_node;

    /* for restoring to original output when hotplugging or switching vt */
    struct old_output {
        struct cwc_output *output;
        struct bsp_node *bsp_node;
        tag_bitfield_t tag;
        int workspace;
    } old_prop;

    struct wl_list toplevels;

    struct wl_list link_output_container; // cwc_output_state.containers
    struct wl_list link_output_fstack;    // cwc_output.state.focus_stack
    struct wl_list link_output_minimized; // cwc_output.state.minimized
};

void cwc_container_init(struct cwc_output *output,
                        struct cwc_toplevel *toplevel,
                        int border_w);

/* unmanaged container  or unamaged toplevel is not accepted */
void cwc_container_insert_toplevel(struct cwc_container *c,
                                   struct cwc_toplevel *toplevel);

/* will destroy the container when the container is empty */
void cwc_container_remove_toplevel(struct cwc_toplevel *toplevel);

/* the function name should clear enough what it does */
void cwc_container_remove_toplevel_but_dont_destroy_container_when_empty(
    struct cwc_toplevel *toplevel);

void cwc_container_move_to_output(struct cwc_container *container,
                                  struct cwc_output *output);

void cwc_container_focusidx(struct cwc_container *container, int idx);
void cwc_container_swap(struct cwc_container *source,
                        struct cwc_container *target);

struct wlr_box cwc_container_get_box(struct cwc_container *container);
struct cwc_toplevel *
cwc_container_get_front_toplevel(struct cwc_container *cont);

void cwc_container_set_enabled(struct cwc_container *container, bool set);
void cwc_container_set_front_toplevel(struct cwc_toplevel *toplevel);
void cwc_container_set_size(struct cwc_container *container, int w, int h);
void cwc_container_set_position(struct cwc_container *container, int x, int y);
void cwc_container_set_position_gap(struct cwc_container *container,
                                    int x,
                                    int y);
void cwc_container_set_position_global(struct cwc_container *container,
                                       int x,
                                       int y);
void cwc_container_set_fullscreen(struct cwc_container *cont, bool set);
void cwc_container_set_maximized(struct cwc_container *container, bool set);
void cwc_container_set_minimized(struct cwc_container *container, bool set);
void cwc_container_set_floating(struct cwc_container *container, bool set);
void cwc_container_set_sticky(struct cwc_container *container, bool set);

bool cwc_container_is_floating(struct cwc_container *container);
bool cwc_container_is_visible(struct cwc_container *container);
bool cwc_container_is_visible_in_workspace(struct cwc_container *container,
                                           int workspace);

void cwc_container_move_to_tag(struct cwc_container *container, int tagidx);
void cwc_container_to_center(struct cwc_container *container);

void cwc_container_restore_floating_box(struct cwc_container *container);

void cwc_container_for_each_toplevel(struct cwc_container *container,
                                     void (*f)(struct cwc_toplevel *toplevel,
                                               void *data),
                                     void *data);

void cwc_container_raise(struct cwc_container *container);
void cwc_container_lower(struct cwc_container *container);

void cwc_container_for_each_toplevel_top_to_bottom(
    struct cwc_container *container,
    void (*f)(struct cwc_toplevel *toplevel, void *data),
    void *data);

void cwc_container_for_each_bottom_to_top(
    struct cwc_container *container,
    void (*f)(struct cwc_toplevel *toplevel, void *data),
    void *data);

// ======================= MACRO =================================

static inline bool cwc_container_is_unmanaged(struct cwc_container *cont)
{
    return cont->state & CONTAINER_STATE_UNMANAGED;
}

static inline bool cwc_container_is_minimized(struct cwc_container *cont)
{
    return cont->state & CONTAINER_STATE_MINIMIZED;
}

static inline bool cwc_container_is_maximized(struct cwc_container *cont)
{
    return cont->state & CONTAINER_STATE_MAXIMIZED;
}

static inline bool cwc_container_is_fullscreen(struct cwc_container *cont)
{
    return cont->state & CONTAINER_STATE_FULLSCREEN;
}

static inline bool cwc_container_is_sticky(struct cwc_container *cont)
{
    return cont->state & CONTAINER_STATE_STICKY;
}

static inline bool
cwc_container_is_configure_allowed(struct cwc_container *container)
{
    return !cwc_container_is_fullscreen(container)
           && !cwc_container_is_maximized(container);
}

static inline void cwc_container_refresh(struct cwc_container *container)
{
    cwc_container_set_front_toplevel(
        cwc_container_get_front_toplevel(container));
}

static inline float cwc_container_get_opacity(struct cwc_container *container)
{
    return container->opacity;
}

void cwc_container_set_opacity(struct cwc_container *container, float opacity);

#endif // !_CWC_CONTAINER_H
