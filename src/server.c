/* server.c - server initialization
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
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_transient_seat_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#ifdef CWC_XWAYLAND
#include <wlr/xwayland/shell.h>
#endif /* ifdef CWC_XWAYLAND */

#include "cwc/desktop/idle.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/session_lock.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/luac.h"
#include "cwc/plugin.h"
#include "cwc/process.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"
#include "private/server.h"

/* sway's implementation */
static bool is_privileged(const struct wl_global *global)
{
    // drm lease check here

    return global == server.output_manager->global
           || global == server.output_power_manager->global
           || global == server.input_method_manager->global
           || global == server.foreign_toplevel_list->global
           || global == server.foreign_toplevel_manager->global
           || global == server.wlr_data_control_manager->global
           || global == server.ext_data_control_manager->global
           || global == server.screencopy_manager->global
           || global == server.copy_capture_manager->global
           || global == server.export_dmabuf_manager->global
           || global == server.security_context_manager->global
           || global == server.gamma_control_manager->global
           || global == server.layer_shell->global
           || global == server.session_lock->manager->global
           || global == server.input->kbd_inhibit_manager->global
           || global == server.input->virtual_kbd_manager->global
           || global == server.input->virtual_pointer_manager->global
           || global == server.input->transient_seat_manager->global
           || global == server.xdg_output_manager->global;
}

static bool filter_global(const struct wl_client *client,
                          const struct wl_global *global,
                          void *data)
{
#ifdef CWC_XWAYLAND
    struct wlr_xwayland *xwayland = server.xwayland;
    if (xwayland && global == xwayland->shell_v1->global) {
        return xwayland->server != NULL && client == xwayland->server->client;
    }
#endif /* ifdef CWC_XWAYLAND */

    // Restrict usage of privileged protocols to unsandboxed clients
    const struct wlr_security_context_v1_state *security_context =
        wlr_security_context_manager_v1_lookup_client(
            server.security_context_manager, client);

    if (is_privileged(global))
        return security_context == NULL;

    return true;
}

/* Since the server is global and everything depends on wayland global registry
 * this should run before everything else
 */
static int setup_wayland_core(struct cwc_server *s)
{
    struct wl_display *dpy = s->wl_display = wl_display_create();
    s->wl_event_loop                       = wl_display_get_event_loop(dpy);

    wl_display_set_global_filter(s->wl_display, filter_global, NULL);
    wl_display_set_default_max_buffer_size(dpy, 1024 * 1024);

    if (!(s->backend = wlr_backend_autocreate(s->wl_event_loop, &s->session))) {
        cwc_log(CWC_ERROR, "Failed to create wlr backend");
        return EXIT_FAILURE;
    }

    s->headless_backend = wlr_headless_backend_create(s->wl_event_loop);
    if (!s->headless_backend) {
        cwc_log(CWC_ERROR, "Failed to create headless backend");
        return EXIT_FAILURE;
    }

    wlr_multi_backend_add(s->backend, s->headless_backend);

    struct wlr_renderer *drw;
    if (!(drw = s->renderer = wlr_renderer_autocreate(s->backend))) {
        cwc_log(CWC_ERROR, "Failed to create renderer");
        return EXIT_FAILURE;
    }

    s->scene = wlr_scene_create();
    wlr_renderer_init_wl_shm(drw, dpy);

    if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
        wlr_drm_create(dpy, drw);
        wlr_scene_set_linux_dmabuf_v1(
            s->scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 5, drw));
    }

    int drm_fd;
    if ((drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 && drw->features.timeline
        && s->backend->features.timeline)
        wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);

    s->allocator = wlr_allocator_autocreate(s->backend, drw);
    if (s->allocator == NULL) {
        cwc_log(CWC_ERROR, "failed to create wlr_allocator");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* return non zero if error */
enum server_init_return_code
server_init(struct cwc_server *s, char *config_path, char *library_path)
{
    cwc_log(CWC_INFO, "Initializing cwc server...");

    if (setup_wayland_core(s) != EXIT_SUCCESS)
        return SERVER_INIT_FAILED;

    struct wl_display *dpy = s->wl_display;
    s->compositor          = wlr_compositor_create(dpy, 6, s->renderer);

    // initialize list
    wl_list_init(&s->plugins);
    wl_list_init(&s->outputs);
    wl_list_init(&s->toplevels);
    wl_list_init(&s->containers);
    wl_list_init(&s->layer_shells);
    wl_list_init(&s->kbd_kmaps);
    wl_list_init(&s->timers);

    // initialize map so that luaC can insert something at startup
    s->main_kbd_kmap      = cwc_keybind_map_create(NULL);
    s->main_mouse_kmap    = cwc_keybind_map_create(NULL);
    s->output_state_cache = cwc_hhmap_create(8);
    s->signal_map         = cwc_hhmap_create(50);
    s->input              = cwc_input_manager_get();

    int lua_status = luaC_init();
    keybind_register_common_key();

    // wlroots plug and play
    wlr_subcompositor_create(dpy);
    wlr_data_device_manager_create(dpy);
    wlr_primary_selection_v1_device_manager_create(dpy);
    wlr_viewporter_create(dpy);
    wlr_single_pixel_buffer_manager_v1_create(dpy);
    wlr_fractional_scale_manager_v1_create(dpy, 1);
    wlr_presentation_create(dpy, s->backend, 2);
    wlr_alpha_modifier_v1_create(dpy);
    wlr_ext_output_image_capture_source_manager_v1_create(dpy, 1);

    s->content_type_manager     = wlr_content_type_manager_v1_create(dpy, 1);
    s->security_context_manager = wlr_security_context_manager_v1_create(dpy);
    s->export_dmabuf_manager    = wlr_export_dmabuf_manager_v1_create(dpy);
    s->screencopy_manager       = wlr_screencopy_manager_v1_create(dpy);
    s->copy_capture_manager =
        wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
    s->wlr_data_control_manager = wlr_data_control_manager_v1_create(dpy);
    s->ext_data_control_manager =
        wlr_ext_data_control_manager_v1_create(dpy, 1);

    s->gamma_control_manager = wlr_gamma_control_manager_v1_create(dpy);
    wlr_scene_set_gamma_control_manager_v1(s->scene, s->gamma_control_manager);

    struct wlr_xdg_foreign_registry *foreign_registry =
        wlr_xdg_foreign_registry_create(dpy);
    wlr_xdg_foreign_v1_create(dpy, foreign_registry);
    wlr_xdg_foreign_v2_create(dpy, foreign_registry);

    // root scene graph
    s->temporary_tree = wlr_scene_tree_create(&s->scene->tree);
    wlr_scene_node_set_enabled(&s->temporary_tree->node, false);

    struct wlr_scene_tree *main_scene = s->main_tree =
        wlr_scene_tree_create(&s->scene->tree);
    s->root.background   = wlr_scene_tree_create(main_scene);
    s->root.bottom       = wlr_scene_tree_create(main_scene);
    s->root.below        = wlr_scene_tree_create(main_scene);
    s->root.toplevel     = wlr_scene_tree_create(main_scene);
    s->root.above        = wlr_scene_tree_create(main_scene);
    s->root.top          = wlr_scene_tree_create(main_scene);
    s->root.overlay      = wlr_scene_tree_create(main_scene);
    s->root.session_lock = wlr_scene_tree_create(main_scene);

    // desktop
    setup_output(s);
    setup_xdg_shell(s);
    setup_decoration_manager(s);
#ifdef CWC_XWAYLAND
    xwayland_init(s);
#endif // CWC_XWAYLAND

    s->foreign_toplevel_list = wlr_ext_foreign_toplevel_list_v1_create(dpy, 1);
    s->foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);

    s->scene_layout =
        wlr_scene_attach_output_layout(s->scene, s->output_layout);

    cwc_idle_init(s);
    setup_cwc_session_lock(s);
    setup_layer_shell(s);
    setup_transaction(s);

    // inputs
    setup_pointer(s->input);
    setup_keyboard(s->input);
    setup_seat(s->input);
    setup_text_input(s);

    setup_ipc(s);
    setup_process(s);

    const char *socket = wl_display_add_socket_auto(dpy);
    if (!socket)
        return SERVER_INIT_FAILED;

    if (!wlr_backend_start(s->backend)) {
        cwc_log(CWC_ERROR, "Failed to start wlr backend");
        return SERVER_INIT_FAILED;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    cwc_log(CWC_INFO, "Starting Wayland compositor on WAYLAND_DISPLAY=%s",
            socket);

    if (luacheck) {
        if (lua_status) {
            return LUACHECK_ERROR;
        } else {
            return LUACHECK_OK;
        }
    }

    return SERVER_INIT_SUCCESS;
}

void server_fini(struct cwc_server *s)
{
    cwc_log(CWC_INFO, "Shutting down cwc...");
    wl_display_destroy_clients(s->wl_display);

    cwc_signal_emit_c("cwc::shutdown", NULL);

    cleanup_process(s);
    cleanup_ipc(s);

    cleanup_text_input(s);
    cleanup_seat(s->input);
    cleanup_keyboard(s->input);
    cleanup_pointer(s->input);

    cleanup_output(s);
    cleanup_xdg_shell(s);
    cleanup_decoration_manager(s);
    cleanup_layer_shell(s);

    cwc_plugin_stop_plugins(&s->plugins);
    cwc_input_manager_destroy();

    cwc_idle_fini(s);
#ifdef CWC_XWAYLAND
    xwayland_fini(s);
#endif // CWC_XWAYLAND

    wlr_output_layout_destroy(s->output_layout);
    wlr_allocator_destroy(s->allocator);
    wlr_renderer_destroy(s->renderer);
    wl_display_destroy(s->wl_display);
    wlr_scene_node_destroy(&s->scene->tree.node);
}
