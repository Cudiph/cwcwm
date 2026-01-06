#ifndef _CWC_SERVER_H
#define _CWC_SERVER_H

#include <wayland-server-core.h>

enum server_init_return_code {
    SERVER_INIT_SUCCESS = 0,
    SERVER_INIT_FAILED  = 1,

    LUACHECK_OK    = 10,
    LUACHECK_ERROR = 11,
};

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

    struct wlr_content_type_manager_v1 *content_type_manager;
    struct wlr_security_context_manager_v1 *security_context_manager;
    struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager;
    struct wlr_screencopy_manager_v1 *screencopy_manager;
    struct wlr_ext_image_copy_capture_manager_v1 *copy_capture_manager;
    struct wlr_data_control_manager_v1 *wlr_data_control_manager;
    struct wlr_ext_data_control_manager_v1 *ext_data_control_manager;
    struct wlr_gamma_control_manager_v1 *gamma_control_manager;
    struct wlr_xdg_output_manager_v1 *xdg_output_manager;

    // desktop
    struct wlr_output_layout *output_layout;
    struct wl_listener output_layout_change_l;
    struct wl_listener new_output_l;
    struct cwc_output *fallback_output;

    struct wlr_output_manager_v1 *output_manager;
    struct wl_listener output_manager_apply_l;
    struct wl_listener output_manager_test_l;

    struct wlr_output_power_manager_v1 *output_power_manager;
    struct wl_listener opm_set_mode_l;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel_l;
    struct wl_listener new_xdg_popup_l;

#ifdef CWC_XWAYLAND
    struct wlr_xwayland *xwayland;
    struct wl_listener xw_ready_l;
    struct wl_listener xw_new_surface_l;
#endif // CWC_XWAYLAND

    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    struct wl_listener new_decoration_l;

    struct wlr_tearing_control_manager_v1 *tearing_manager;
    struct wl_listener new_tearing_object_l;

    struct cwc_session_lock_manager *session_lock;
    struct cwc_idle *idle;

    struct wlr_xdg_activation_v1 *xdg_activation;
    struct wl_listener request_activate_l;

    struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;
    struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

    struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
        *foreign_toplevel_image_capture_source_manager;
    struct wl_listener new_capture_source_request_l;

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
    struct cwc_input_manager *input;
    struct cwc_seat *seat;

    struct wlr_input_method_manager_v2 *input_method_manager;
    struct wl_listener new_input_method_l;

    struct wlr_text_input_manager_v3 *text_input_manager;
    struct wl_listener new_text_input_l;

    // ipc
    int socket_fd;
    char *socket_path;

    // list
    struct wl_list plugins;      // cwc_plugin.link
    struct wl_list outputs;      // cwc_output.link
    struct wl_list toplevels;    // cwc_toplevel.link
    struct wl_list containers;   // cwc_container.link
    struct wl_list layer_shells; // cwc_layer_surface.link
    struct wl_list kbd_kmaps;    // cwc_keybind_map.link
    struct wl_list timers;       // cwc_timer.link

    // maps
    struct cwc_hhmap *output_state_cache;    // struct cwc_output_state
    struct cwc_hhmap *signal_map;            // struct cwc_signal_entry
    struct cwc_keybind_map *main_kbd_kmap;   // struct cwc_keybind_info
    struct cwc_keybind_map *main_mouse_kmap; // struct cwc_keybind_info

    // server wide state
    struct cwc_container *insert_marked; // managed by container.c
    struct cwc_output *focused_output;   // managed by output.c
    int resize_count;                    // frame synchronization
};

/* global server instance from main */
extern struct cwc_server server;

enum server_init_return_code
server_init(struct cwc_server *s, char *config_path, char *library_path);
void server_fini(struct cwc_server *s);

#endif // !_CWC_SERVER_H
