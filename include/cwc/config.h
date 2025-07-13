#ifndef _CWC_CONFIG_H
#define _CWC_CONFIG_H

#include <libinput.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

struct cwc_config {
    // cwc
    bool tasklist_show_all;

    // client
    int border_color_rotation_degree;
    int border_width;
    struct _cairo_pattern *border_color_focus;  // USE SETTER
    struct _cairo_pattern *border_color_normal; // USE SETTER

    // screen
    int useless_gaps;

    // pointer device
    int cursor_size;
    int cursor_inactive_timeout_ms;
    int cursor_edge_threshold;
    float cursor_edge_snapping_overlay_color[4];

    // kbd
    int repeat_rate;
    int repeat_delay;
    char *xkb_rules;
    char *xkb_model;
    char *xkb_layout;
    char *xkb_variant;
    char *xkb_options;

    // the one and only lua_State
    struct lua_State *_L_but_better_to_use_function_than_directly;

    struct {
        /* data is a struct cwc_config which is the older config for comparison
         */
        struct wl_signal commit;
    } events;

    struct {
        struct cwc_config *old_config;
    } CWC_PRIVATE;
};

extern struct cwc_config g_config;

static inline struct lua_State *g_config_get_lua_State()
{
    return g_config._L_but_better_to_use_function_than_directly;
}

void cwc_config_init();

void cwc_config_commit();

void cwc_config_set_default();

void cwc_config_set_cairo_pattern(struct _cairo_pattern **dest,
                                  struct _cairo_pattern *src);

void cwc_config_set_number_positive(int *dest, int src);

#endif // !_CWC_CONFIG_H
