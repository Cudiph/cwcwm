#ifndef _CWC_LAYER_SHELL_H
#define _CWC_LAYER_SHELL_H

#include "cwc/types.h"
#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct cwc_server;

/* node.data == cwc_layer_surface */
struct cwc_layer_surface {
    enum cwc_data_type type;
    struct wl_list link; // struct cwc_server.layer_shells
    struct wlr_layer_surface_v1 *wlr_layer_surface;
    struct wlr_scene_layer_surface_v1 *scene_layer;
    struct cwc_output *output;
    bool mapped;
    struct wlr_scene_tree *popup_tree;

    struct wl_listener new_popup_l;
    struct wl_listener destroy_l;

    struct wl_listener map_l;
    struct wl_listener unmap_l;
    struct wl_listener commit_l;
};

void arrange_layers(struct cwc_output *output);

#endif // !_CWC_LAYER_SHELL_H
