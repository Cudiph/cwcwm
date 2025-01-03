#ifndef _CWC_SERVER_H
#define _CWC_SERVER_H

#include <wayland-server-core.h>

struct cwc_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;

    struct wlr_backend *backend;
    struct wlr_backend *headless_backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *compositor;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wlr_session *session;

    // desktop
    struct wlr_output_layout *output_layout;
    struct wl_listener new_output_l;
    struct cwc_output *fallback_output; // TODO: create a fallback output

    struct wlr_output_manager_v1 *output_manager;
    struct wl_listener output_manager_apply_l;
    struct wl_listener output_manager_test_l;

    struct wlr_output_power_manager_v1 *output_power_manager;
    struct wl_listener opm_set_mode_l;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel_l;
    struct wl_listener new_xdg_popup_l;

    struct wlr_xwayland *xwayland;
    struct wl_listener xw_ready_l;
    struct wl_listener xw_new_surface_l;

    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    struct wl_listener new_decoration_l;

    struct wlr_tearing_control_manager_v1 *tearing_manager;
    struct wl_listener new_tearing_object_l;

    struct cwc_session_lock_manager *session_lock;
    struct cwc_idle *idle;

    struct wlr_scene_tree *main_tree;
    struct wlr_scene_tree *temporary_tree;
    // sorted from back to front
    struct {
        struct wlr_scene_tree *background;   // layer_shell
        struct wlr_scene_tree *bottom;       // layer_shell
        struct wlr_scene_tree *below;        // toplevel below normal toplevel
        struct wlr_scene_tree *toplevel;     // regular toplevel belong here
        struct wlr_scene_tree *above;        // toplevel above normal toplevel
        struct wlr_scene_tree *top;          // layer_shell
        struct wlr_scene_tree *overlay;      // layer_shell
        struct wlr_scene_tree *session_lock; // session_lock
    } root;
    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener layer_shell_surface_l;

    // inputs
    struct cwc_seat *seat;
    struct cwc_cursor_shape_manager *cursor_shape_manager;
    struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;

    struct wlr_pointer_constraints_v1 *pointer_constraints;
    struct wl_listener new_pointer_constraint_l;

    struct wlr_virtual_keyboard_manager_v1 *virtual_kbd_manager;
    struct wl_listener new_vkbd_l;
    struct wlr_virtual_pointer_manager_v1 *virtual_pointer_manager;
    struct wl_listener new_vpointer_l;

    // list
    struct wl_list plugins;      // cwc_plugin.link
    struct wl_list outputs;      // cwc_output.link
    struct wl_list toplevels;    // cwc_toplevel.link
    struct wl_list containers;   // cwc_container.link
    struct wl_list layer_shells; // cwc_layer_surface.link

    // maps
    struct cwc_hhmap *output_state_cache; // struct cwc_output_state
    struct cwc_hhmap *signal_map;         // struct cwc_signal_entry
    struct cwc_hhmap *keybind_kbd_map;    // struct cwc_keybind_info
    struct cwc_hhmap *keybind_mouse_map;  // struct cwc_keybind_info

    // server wide state
    struct cwc_container *insert_marked; // managed by container.c
    struct cwc_output *focused_output;   // managed by output.c
    int resize_count;                    // frame synchronization
};

/* global server instance from main */
extern struct cwc_server server;

int server_init(struct cwc_server *s, char *config_path, char *library_path);
void server_fini(struct cwc_server *s);

void spawn(char **argv);
void spawn_with_shell(const char *const command);

#endif // !_CWC_SERVER_H
