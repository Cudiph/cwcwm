/* output.c - output/screen management
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

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_tearing_control_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/desktop/transaction.h"
#include "cwc/input/manager.h"
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

void cwc_output_focus(struct cwc_output *output)
{
    // TODO: add signal
    server.focused_output = output;
}

void cwc_output_tiling_layout_update(struct cwc_output *output, int workspace)
{
    if (output == server.fallback_output)
        return;

    enum cwc_layout_mode mode =
        cwc_output_get_current_tag_info(output)->layout_mode;

    workspace = workspace ? workspace : output->state->active_workspace;

    switch (mode) {
    case CWC_LAYOUT_BSP:
        bsp_update_root(output, workspace);
        break;
    case CWC_LAYOUT_MASTER:
        master_arrange_update(output);
        break;
    default:
        break;
    }
}

void cwc_output_tiling_layout_update_container(struct cwc_container *container,
                                               bool update_container_workspace)
{
    if (!cwc_container_is_currently_tiled(container))
        return;

    cwc_output_tiling_layout_update(
        container->output,
        update_container_workspace ? container->workspace : 0);
}

static struct cwc_output_state *
cwc_output_state_create(struct cwc_output *output)
{
    struct cwc_output_state *state = calloc(1, sizeof(struct cwc_output_state));
    state->output                  = output;

    state->active_tag            = 1;
    state->active_workspace      = 1;
    state->max_general_workspace = 9;
    wl_list_init(&state->focus_stack);
    wl_list_init(&state->toplevels);
    wl_list_init(&state->containers);
    wl_list_init(&state->minimized);

    lua_State *L = g_config_get_lua_State();
    for (int i = 0; i < MAX_WORKSPACE; i++) {
        struct cwc_tag_info *tag_info = &state->tag_info[i];

        tag_info->index                       = i;
        tag_info->useless_gaps                = g_config.useless_gaps;
        tag_info->layout_mode                 = CWC_LAYOUT_FLOATING;
        tag_info->pending_transaction         = false;
        tag_info->master_state.master_count   = 1;
        tag_info->master_state.column_count   = 1;
        tag_info->master_state.mwfact         = 0.5;
        tag_info->master_state.current_layout = get_default_master_layout();

        luaC_object_tag_register(L, tag_info);
    }

    return state;
}

static inline void cwc_output_state_save(struct cwc_output *output)
{
    cwc_hhmap_insert(server.output_state_cache, output->wlr_output->name,
                     output->state);
}

/* return true if restored, false otherwise */
static bool cwc_output_state_try_restore(struct cwc_output *output)
{
    output->state =
        cwc_hhmap_get(server.output_state_cache, output->wlr_output->name);

    if (!output->state)
        return false;

    output->state->output         = output;
    struct cwc_output *old_output = output->state->old_output;

    /* restore container to old output */
    struct cwc_container *container;
    wl_list_for_each(container, &server.containers, link)
    {
        if (container->old_prop.output != old_output)
            continue;

        cwc_container_move_to_output(container, output);

        container->bsp_node  = container->old_prop.bsp_node;
        container->tag       = container->old_prop.tag;
        container->workspace = container->old_prop.workspace;

        container->old_prop.bsp_node = NULL;
        container->old_prop.output   = NULL;
    }

    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &server.toplevels, link)
    {
        // only managed toplevel that need reattach
        if (cwc_toplevel_is_unmanaged(toplevel) && toplevel->container
            && toplevel->container->output != output)
            continue;

        wl_list_reattach(&output->state->toplevels,
                         &toplevel->link_output_toplevels);
    }

    /* update output for the layer shell */
    struct cwc_layer_surface *layer_surface;
    wl_list_for_each(layer_surface, &server.layer_shells, link)
    {
        if (layer_surface->output == old_output) {
            layer_surface->output                    = output;
            layer_surface->wlr_layer_surface->output = output->wlr_output;
        }
    }

    /* reset pending_transaction state */
    for (int i = 0; i < MAX_WORKSPACE; i++) {
        struct cwc_tag_info *tag_info = &output->state->tag_info[i];
        tag_info->pending_transaction = false;
    }

    cwc_hhmap_remove(server.output_state_cache, output->wlr_output->name);
    free(old_output);

    return true;
}

/* output state will not be destroyed in entire compositor lifetime and used for
 * restoration
 */
static inline void cwc_output_state_destroy(struct cwc_output_state *state)
{
    // unreg tag object
    // free(state);
}

static void _output_configure_scene(struct cwc_output *output,
                                    struct wlr_scene_node *node,
                                    float opacity)
{
    if (node->data) {
        struct cwc_container *container =
            cwc_container_try_from_data_descriptor(node->data);
        if (container)
            opacity = container->opacity;
    }

    if (node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *surface =
            wlr_scene_surface_try_from_buffer(buffer);

        if (surface) {
            const struct wlr_alpha_modifier_surface_v1_state
                *alpha_modifier_state =
                    wlr_alpha_modifier_v1_get_surface_state(surface->surface);
            if (alpha_modifier_state != NULL) {
                opacity *= (float)alpha_modifier_state->multiplier;
            }
        }

        wlr_scene_buffer_set_opacity(buffer, opacity);
    } else if (node->type == WLR_SCENE_NODE_TREE) {
        struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
        struct wlr_scene_node *node;
        wl_list_for_each(node, &tree->children, link)
        {
            _output_configure_scene(output, node, opacity);
        }
    }
}

static bool output_can_tear(struct cwc_output *output)
{
    struct cwc_toplevel *toplevel = cwc_toplevel_get_focused();

    if (!toplevel)
        return false;

    if (cwc_toplevel_is_allow_tearing(toplevel)
        && cwc_output_is_allow_tearing(output))
        return true;

    return false;
}

static bool allow_render(struct cwc_output *output, struct timespec *now)
{
    bool is_waiting = output->waiting_since.tv_sec;
    if (is_waiting) {
        uint64_t delta_waiting =
            timespec_to_msec(now) - timespec_to_msec(&output->waiting_since);

        if (delta_waiting > 500) {
            server.resize_count = -1;
            goto reset;
        }
    }

    if (server.resize_count > 0) {
        if (!is_waiting)
            clock_gettime(CLOCK_MONOTONIC, &output->waiting_since);

        return false;
    }

reset:
    output->waiting_since.tv_sec = 0;
    return true;
}

static void output_repaint(struct cwc_output *output,
                           struct wlr_scene_output *scene_output,
                           struct timespec *now)
{
    _output_configure_scene(output, &server.scene->tree.node, 1.0f);

    if (!wlr_scene_output_needs_frame(scene_output))
        return;

    bool can_tear = output_can_tear(output);
    if (!allow_render(output, now) && !can_tear)
        return;

    struct wlr_output_state pending;
    wlr_output_state_init(&pending);

    if (!wlr_scene_output_build_state(scene_output, &pending, NULL)) {
        wlr_output_state_finish(&pending);
        return;
    }

    if (can_tear) {
        pending.tearing_page_flip = true;

        if (!wlr_output_test_state(output->wlr_output, &pending)) {
            cwc_log(CWC_DEBUG,
                    "Output test failed on '%s', retrying without tearing "
                    "page-flip",
                    output->wlr_output->name);
            pending.tearing_page_flip = false;
        }
    }

    if (!wlr_output_commit_state(output->wlr_output, &pending)) {
        cwc_log(CWC_ERROR, "Page-flip failed on output %s",
                output->wlr_output->name);
    }

    wlr_output_state_finish(&pending);
}

static void on_output_frame(struct wl_listener *listener, void *data)
{
    struct cwc_output *output = wl_container_of(listener, output, frame_l);
    struct wlr_scene_output *scene_output = output->scene_output;
    struct timespec now;

    if (!scene_output)
        return;

    clock_gettime(CLOCK_MONOTONIC, &now);
    output_repaint(output, scene_output, &now);

    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void rescue_output_toplevel_container(struct cwc_output *source,
                                             struct cwc_output *target)
{
    struct cwc_container *container;
    struct cwc_container *tmp;
    wl_list_for_each_safe(container, tmp, &source->state->containers,
                          link_output_container)
    {
        if (container->old_prop.output == NULL) {
            container->old_prop.output    = source;
            container->old_prop.bsp_node  = container->bsp_node;
            container->old_prop.workspace = container->workspace;
            container->old_prop.tag       = container->tag;

            container->bsp_node = NULL;
        }

        cwc_container_move_to_output(container, target);
    }

    struct cwc_toplevel *toplevel;
    struct cwc_toplevel *ttmp;
    wl_list_for_each_safe(toplevel, ttmp, &source->state->toplevels,
                          link_output_toplevels)
    {
        wl_list_reattach(target->state->toplevels.prev,
                         &toplevel->link_output_toplevels);
    }
}

static void server_focus_previous_output(struct cwc_output *reference)
{
    if (wl_list_length_at_least(&server.outputs, 2)) {
        struct cwc_output *o;
        wl_list_for_each_reverse(o, &reference->link, link)
        {
            if (&o->link == &server.outputs)
                continue;

            cwc_output_focus(o);
            return;
        }
    }

    cwc_output_focus(server.fallback_output);
}

static void output_layers_fini(struct cwc_output *output);

static void on_output_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_output *output = wl_container_of(listener, output, destroy_l);
    output->state->old_output = output;
    cwc_output_state_save(output);
    cwc_object_emit_signal_simple("screen::destroy", g_config_get_lua_State(),
                                  output);

    cwc_log(CWC_INFO, "destroying output (%s): %p %p", output->wlr_output->name,
            output, output->wlr_output);

    struct cwc_layer_surface *lsurf, *tmp;
    wl_list_for_each_safe(lsurf, tmp, &server.layer_shells, link)
    {
        if (lsurf->output == output)
            wlr_layer_surface_v1_destroy(lsurf->wlr_layer_surface);
    }

    wlr_output_state_finish(&output->pending);
    output_layers_fini(output);
    wlr_scene_output_destroy(output->scene_output);

    wl_list_remove(&output->destroy_l.link);
    wl_list_remove(&output->frame_l.link);
    wl_list_remove(&output->request_state_l.link);

    wl_list_remove(&output->config_commit_l.link);

    // change focus to previous inserted outputs although the order may wrong
    server_focus_previous_output(output);

    struct cwc_output *focused = server.focused_output;

    // update output layout
    wlr_output_layout_remove(server.output_layout, output->wlr_output);
    wlr_output_layout_get_box(server.output_layout, focused->wlr_output,
                              &focused->output_layout_box);

    rescue_output_toplevel_container(output, focused);

    for (int i = 1; i < MAX_WORKSPACE; i++) {
        if (focused != server.fallback_output)
            cwc_output_set_layout_mode(focused, i,
                                       focused->state->tag_info[i].layout_mode);
    }

    transaction_schedule_output(focused);

    luaC_object_unregister(g_config_get_lua_State(), output);
    wl_list_remove(&output->link);

    cwc_output_update_outputs_state();

    // free the output only when restored because the container still need old
    // output reference to remove bsp node.
    output->wlr_output = NULL;
    // free(output);
}

static void output_layer_set_position(struct cwc_output *output, int x, int y)
{
    wlr_scene_node_set_position(&output->layers.background->node, x, y);
    wlr_scene_node_set_position(&output->layers.bottom->node, x, y);
    wlr_scene_node_set_position(&output->layers.top->node, x, y);
    wlr_scene_node_set_position(&output->layers.overlay->node, x, y);
    wlr_scene_node_set_position(&output->layers.session_lock->node, x, y);
}

/* sorting direction is top left to bottom right */
static void sort_output_index()
{
    if (wl_list_empty(&server.outputs))
        return;

    struct cwc_output *o, *sorted_o, *tmp;
    struct wl_list sorted_list_temp;
    wl_list_init(&sorted_list_temp);

    wl_list_for_each_safe(o, tmp, &server.outputs, link)
    {
        struct cwc_output *target = NULL;
        int lowest_y              = INT_MAX;

        wl_list_for_each(sorted_o, &sorted_list_temp, link)
        {
            int sorted_o_y = sorted_o->output_layout_box.y;
            if (o->output_layout_box.y > sorted_o_y)
                continue;

            if (!target) {
                target   = sorted_o;
                lowest_y = sorted_o_y;
                continue;
            }

            if (sorted_o_y < lowest_y) {
                target   = sorted_o;
                lowest_y = sorted_o_y;
            }
        }

        if (target) {
            wl_list_reattach(target->link.prev, &o->link);
        } else {
            wl_list_reattach(sorted_list_temp.prev, &o->link);
        }
    }

    wl_list_for_each_safe(o, tmp, &sorted_list_temp, link)
    {
        wl_list_reattach(server.outputs.prev, &o->link);
    }
}

void cwc_output_update_outputs_state()
{
    struct wlr_output_configuration_v1 *cfg =
        wlr_output_configuration_v1_create();
    struct cwc_output *output;
    wl_list_for_each(output, &server.outputs, link)
    {
        struct wlr_output_configuration_head_v1 *config_head =
            wlr_output_configuration_head_v1_create(cfg, output->wlr_output);
        struct wlr_box output_box;
        wlr_output_layout_get_box(server.output_layout, output->wlr_output,
                                  &output_box);

        config_head->state.enabled = output->wlr_output->enabled;
        config_head->state.x       = output_box.x;
        config_head->state.y       = output_box.y;

        output->output_layout_box = output_box;
        output_layer_set_position(output, output_box.x, output_box.y);
    }

    wlr_output_manager_v1_set_configuration(server.output_manager, cfg);

    cwc_input_manager_update_cursor_scale();
    sort_output_index();
}

static void on_request_state(struct wl_listener *listener, void *data)
{
    struct cwc_output *output =
        wl_container_of(listener, output, request_state_l);
    struct wlr_output_event_request_state *event = data;

    wlr_output_commit_state(output->wlr_output, event->state);
    cwc_output_update_outputs_state();
    arrange_layers(output);
}

static void on_config_commit(struct wl_listener *listener, void *data)
{
    struct cwc_output *output =
        wl_container_of(listener, output, config_commit_l);
    struct cwc_config *old_config = data;

    if (old_config->useless_gaps == g_config.useless_gaps)
        return;
}

static void output_layers_init(struct cwc_output *output)
{
    output->layers.background = wlr_scene_tree_create(server.root.background);
    output->layers.bottom     = wlr_scene_tree_create(server.root.bottom);
    output->layers.top        = wlr_scene_tree_create(server.root.top);
    output->layers.overlay    = wlr_scene_tree_create(server.root.overlay);
    output->layers.session_lock =
        wlr_scene_tree_create(server.root.session_lock);
}

static void output_layers_fini(struct cwc_output *output)
{
    wlr_scene_node_destroy(&output->layers.background->node);
    wlr_scene_node_destroy(&output->layers.bottom->node);
    wlr_scene_node_destroy(&output->layers.top->node);
    wlr_scene_node_destroy(&output->layers.overlay->node);
    wlr_scene_node_destroy(&output->layers.session_lock->node);
}

static struct cwc_output *cwc_output_create(struct wlr_output *wlr_output)
{
    struct cwc_output *output = calloc(1, sizeof(*output));
    output->type              = DATA_TYPE_OUTPUT;
    output->wlr_output        = wlr_output;
    output->tearing_allowed   = false;
    output->wlr_output->data  = output;

    output->output_layout_box.width  = wlr_output->width;
    output->output_layout_box.height = wlr_output->height;

    output->usable_area = output->output_layout_box;

    if (!cwc_output_state_try_restore(output))
        output->state = cwc_output_state_create(output);
    else
        output->restored = true;

    output_layers_init(output);

    return output;
}

static void on_new_output(struct wl_listener *listener, void *data)
{
    struct wlr_output *wlr_output = data;

    if (wlr_output == server.fallback_output->wlr_output)
        return;

    /* Configures the output created by the backend to use our allocator
     * and our renderer. Must be done once, before commiting the output */
    wlr_output_init_render(wlr_output, server.allocator, server.renderer);

    /* The output may be disabled, switch it on. */
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    /* enable adaptive sync by default if supported */
    if (wlr_output->adaptive_sync_supported)
        wlr_output_state_set_adaptive_sync_enabled(&state, true);

    /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
     * before we can use the output. The mode is a tuple of (width, height,
     * refresh rate), and each monitor supports only a specific set of modes. We
     * just pick the monitor's preferred mode, a more sophisticated compositor
     * would let the user configure it. */
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL)
        wlr_output_state_set_mode(&state, mode);

    /* Atomically applies the new output state. */
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct cwc_output *output = cwc_output_create(wlr_output);
    cwc_output_focus(output);
    rescue_output_toplevel_container(server.fallback_output, output);

    output->destroy_l.notify = on_output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy_l);

    output->frame_l.notify         = on_output_frame;
    output->request_state_l.notify = on_request_state;
    wl_signal_add(&wlr_output->events.frame, &output->frame_l);
    wl_signal_add(&wlr_output->events.request_state, &output->request_state_l);

    output->config_commit_l.notify = on_config_commit;
    wl_signal_add(&g_config.events.commit, &output->config_commit_l);

    wl_list_insert(&server.outputs, &output->link);

    /* Adds this to the output layout. The add_auto function arranges outputs
     * from left-to-right in the order they appear. A more sophisticated
     * compositor would let the user configure the arrangement of outputs in the
     * layout.
     *
     * The output layout utility automatically adds a wl_output global to the
     * display, which Wayland clients can see to find out information about the
     * output (such as DPI, scale factor, manufacturer, etc).
     */
    struct wlr_output_layout_output *layout_output =
        wlr_output_layout_add_auto(server.output_layout, wlr_output);
    output->scene_output = wlr_scene_output_create(server.scene, wlr_output);
    wlr_scene_output_layout_add_output(server.scene_layout, layout_output,
                                       output->scene_output);

    cwc_log(CWC_INFO, "created output (%s): %p %p", wlr_output->name, output,
            output->wlr_output);

    wlr_output_state_init(&output->pending);
    cwc_output_update_outputs_state();
    arrange_layers(output);
    transaction_schedule_tag(cwc_output_get_current_tag_info(output));

    luaC_object_screen_register(g_config_get_lua_State(), output);
    cwc_object_emit_signal_simple("screen::new", g_config_get_lua_State(),
                                  output);
}

static void output_manager_apply(struct wlr_output_configuration_v1 *config,
                                 bool test)
{
    struct wlr_output_configuration_head_v1 *config_head;
    int ok = 1;

    cwc_log(CWC_DEBUG, "%sing new output config", test ? "test" : "apply");

    wl_list_for_each(config_head, &config->heads, link)
    {
        struct wlr_output *wlr_output = config_head->state.output;
        struct cwc_output *output     = wlr_output->data;
        struct wlr_output_state state;

        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, config_head->state.enabled);
        if (!config_head->state.enabled)
            goto apply_or_test;

        if (config_head->state.mode)
            wlr_output_state_set_mode(&state, config_head->state.mode);
        else
            wlr_output_state_set_custom_mode(
                &state, config_head->state.custom_mode.width,
                config_head->state.custom_mode.height,
                config_head->state.custom_mode.refresh);

        wlr_output_state_set_transform(&state, config_head->state.transform);
        wlr_output_state_set_scale(&state, config_head->state.scale);
        wlr_output_state_set_adaptive_sync_enabled(
            &state, config_head->state.adaptive_sync_enabled);

    apply_or_test:
        ok &= test ? wlr_output_test_state(wlr_output, &state)
                   : wlr_output_commit_state(wlr_output, &state);

        /* Don't move monitors if position wouldn't change, this to avoid
         * wlroots marking the output as manually configured.
         * wlr_output_layout_add does not like disabled outputs */
        if (!test)
            wlr_output_layout_add(server.output_layout, wlr_output,
                                  config_head->state.x, config_head->state.y);

        wlr_output_state_finish(&state);

        cwc_output_update_outputs_state();
        arrange_layers(output);
    }

    if (ok)
        wlr_output_configuration_v1_send_succeeded(config);
    else
        wlr_output_configuration_v1_send_failed(config);
    wlr_output_configuration_v1_destroy(config);
}

static void on_output_manager_test(struct wl_listener *listener, void *data)
{
    struct wlr_output_configuration_v1 *config = data;

    output_manager_apply(config, true);
}

static void on_output_manager_apply(struct wl_listener *listener, void *data)
{
    struct wlr_output_configuration_v1 *config = data;

    output_manager_apply(config, false);
}

static void on_opm_set_mode(struct wl_listener *listener, void *data)
{
    struct wlr_output_power_v1_set_mode_event *event = data;

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    wlr_output_state_set_enabled(&state, event->mode);
    wlr_output_commit_state(event->output, &state);

    wlr_output_state_finish(&state);
}

struct tearing_object {
    struct wlr_tearing_control_v1 *tearing_control;

    struct wl_listener set_hint_l;
    struct wl_listener destroy_l;
};

static void on_tearing_object_destroy(struct wl_listener *listener, void *data)
{
    struct tearing_object *obj = wl_container_of(listener, obj, destroy_l);

    wl_list_remove(&obj->set_hint_l.link);
    wl_list_remove(&obj->destroy_l.link);
    free(obj);
}

static void on_tearing_object_set_hint(struct wl_listener *listener, void *data)
{
    struct tearing_object *obj = wl_container_of(listener, obj, set_hint_l);

    struct cwc_toplevel *toplevel =
        cwc_toplevel_try_from_wlr_surface(obj->tearing_control->surface);

    if (toplevel)
        toplevel->tearing_hint = obj->tearing_control->current;
}

static void on_new_tearing_object(struct wl_listener *listener, void *data)
{
    struct wlr_tearing_control_v1 *tearing_control = data;

    struct tearing_object *obj = calloc(1, sizeof(*obj));
    obj->tearing_control       = tearing_control;

    obj->set_hint_l.notify = on_tearing_object_set_hint;
    obj->destroy_l.notify  = on_tearing_object_destroy;
    wl_signal_add(&tearing_control->events.set_hint, &obj->set_hint_l);
    wl_signal_add(&tearing_control->events.destroy, &obj->destroy_l);
}

void setup_output(struct cwc_server *s)
{
    struct wlr_output *headless =
        wlr_headless_add_output(s->headless_backend, 1280, 720);
    wlr_output_set_name(headless, "FALLBACK");
    s->fallback_output = cwc_output_create(headless);

    // wlr output layout
    s->output_layout       = wlr_output_layout_create(s->wl_display);
    s->new_output_l.notify = on_new_output;
    wl_signal_add(&s->backend->events.new_output, &s->new_output_l);

    // output manager
    s->output_manager = wlr_output_manager_v1_create(s->wl_display);
    s->output_manager_test_l.notify  = on_output_manager_test;
    s->output_manager_apply_l.notify = on_output_manager_apply;
    wl_signal_add(&s->output_manager->events.test, &s->output_manager_test_l);
    wl_signal_add(&s->output_manager->events.apply, &s->output_manager_apply_l);

    // output power manager
    s->output_power_manager = wlr_output_power_manager_v1_create(s->wl_display);
    s->opm_set_mode_l.notify = on_opm_set_mode;
    wl_signal_add(&s->output_power_manager->events.set_mode,
                  &s->opm_set_mode_l);

    // tearing manager
    s->tearing_manager =
        wlr_tearing_control_manager_v1_create(s->wl_display, 1);
    s->new_tearing_object_l.notify = on_new_tearing_object;
    wl_signal_add(&s->tearing_manager->events.new_object,
                  &s->new_tearing_object_l);

    // xdg output
    s->xdg_output_manager =
        wlr_xdg_output_manager_v1_create(s->wl_display, s->output_layout);
}

void cleanup_output(struct cwc_server *s)
{
    wl_list_remove(&s->new_output_l.link);

    wl_list_remove(&s->output_manager_test_l.link);
    wl_list_remove(&s->output_manager_apply_l.link);

    wl_list_remove(&s->opm_set_mode_l.link);

    wl_list_remove(&s->new_tearing_object_l.link);
}

void cwc_output_update_visible(struct cwc_output *output)
{
    if (output == server.fallback_output)
        return;

    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        if (cwc_container_is_visible(container)) {
            cwc_container_set_enabled(container, true);
        } else {
            cwc_container_set_enabled(container, false);
        }
    }

    update_idle_inhibitor(NULL);

    cwc_output_focus_newest_focus_visible_toplevel(output);
}

struct cwc_output *cwc_output_get_focused()
{
    return server.focused_output;
}

struct cwc_toplevel *cwc_output_get_newest_toplevel(struct cwc_output *output,
                                                    bool visible)
{
    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &output->state->toplevels, link_output_toplevels)
    {

        if (cwc_toplevel_is_unmanaged(toplevel))
            continue;

        if (visible && !cwc_toplevel_is_visible(toplevel))
            continue;

        return toplevel;
    }

    return NULL;
}

struct cwc_toplevel *
cwc_output_get_newest_focus_toplevel(struct cwc_output *output, bool visible)
{
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->focus_stack, link_output_fstack)
    {
        struct cwc_toplevel *toplevel =
            cwc_container_get_front_toplevel(container);
        if (cwc_toplevel_is_unmanaged(toplevel))
            continue;

        if (visible && !cwc_toplevel_is_visible(toplevel))
            continue;

        return toplevel;
    }

    return NULL;
}

struct cwc_output *cwc_output_get_by_name(const char *name)
{
    struct cwc_output *output;
    wl_list_for_each(output, &server.outputs, link)
    {
        if (strcmp(output->wlr_output->name, name) == 0)
            return output;
    }

    return NULL;
}

struct cwc_output *
cwc_output_get_nearest_by_direction(struct cwc_output *reference,
                                    enum wlr_direction dir)
{
    struct cwc_output *nearest_output = NULL;
    double nearest_distance           = DBL_MAX;
    int reference_lx                  = reference->output_layout_box.x;
    int reference_ly                  = reference->output_layout_box.y;

    struct cwc_output *output;
    wl_list_for_each(output, &server.outputs, link)
    {
        if (output == reference)
            continue;

        int lx = output->output_layout_box.x;
        int ly = output->output_layout_box.y;

        int x = lx - reference_lx;
        int y = ly - reference_ly;

        if (!x && !y)
            continue;

        if (!is_direction_match(dir, x, y))
            continue;

        double _distance = distance(lx, ly, reference_lx, reference_ly);
        if (nearest_distance > _distance) {
            nearest_distance = _distance;
            nearest_output   = output;
        }
    }

    return nearest_output;
}

void cwc_output_focus_newest_focus_visible_toplevel(struct cwc_output *output)
{
    struct cwc_toplevel *toplevel =
        cwc_output_get_newest_focus_toplevel(output, true);

    if (toplevel) {
        cwc_toplevel_focus(toplevel, false);
        return;
    }

    wlr_seat_pointer_clear_focus(server.seat->wlr_seat);
    wlr_seat_keyboard_clear_focus(server.seat->wlr_seat);
}

bool cwc_output_is_exist(struct cwc_output *output)
{
    struct cwc_output *_output;
    wl_list_for_each(_output, &server.outputs, link)
    {
        if (_output == output)
            return true;
    }

    return false;
}

//=========== MACRO ===============

struct cwc_output *
cwc_output_at(struct wlr_output_layout *ol, double x, double y)
{
    struct wlr_output *o = wlr_output_layout_output_at(ol, x, y);
    return o ? o->data : NULL;
}

struct cwc_toplevel **
cwc_output_get_visible_toplevels(struct cwc_output *output)
{
    int maxlen                 = wl_list_length(&output->state->toplevels);
    struct cwc_toplevel **list = calloc(maxlen + 1, sizeof(void *));

    int tail_pointer = 0;
    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &output->state->toplevels, link_output_toplevels)
    {
        if (cwc_toplevel_is_visible(toplevel)) {
            list[tail_pointer] = toplevel;
            tail_pointer += 1;
        }
    }

    return list;
}

struct cwc_container **
cwc_output_get_visible_containers(struct cwc_output *output)
{
    int maxlen                  = wl_list_length(&output->state->containers);
    struct cwc_container **list = calloc(maxlen + 1, sizeof(void *));

    int tail_pointer = 0;
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        if (cwc_container_is_visible(container)) {
            list[tail_pointer] = container;
            tail_pointer += 1;
        }
    }

    return list;
}

void cwc_output_set_position(struct cwc_output *output, int x, int y)
{
    wlr_output_layout_add(server.output_layout, output->wlr_output, x, y);
    cwc_output_update_outputs_state();
    arrange_layers(output);
}

//================== TAGS OPERATION ===================

static inline void insert_tiled_toplevel_to_bsp_tree(struct cwc_output *output,
                                                     int workspace)
{
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        if (!cwc_container_is_visible_in_workspace(container, workspace)
            || cwc_container_is_floating(container)
            || container->bsp_node != NULL)
            continue;

        bsp_insert_container(container, workspace);
        if (cwc_container_is_maximized(container)
            || cwc_container_is_fullscreen(container))
            bsp_node_disable(container->bsp_node);
    }
}

void cwc_output_set_view_only(struct cwc_output *output, int workspace)
{
    int single_tag = 1 << (workspace - 1);
    if (output->state->active_workspace == workspace
        && output->state->active_tag == single_tag)
        return;

    if (workspace)
        output->state->active_tag = single_tag;
    else
        output->state->active_tag = 0;
    output->state->active_workspace = workspace;

    transaction_schedule_tag(cwc_output_get_current_tag_info(output));
    transaction_schedule_output(output);

    lua_State *L = g_config_get_lua_State();
    cwc_object_emit_signal_simple("screen::prop::active_tag", L, output);
    cwc_object_emit_signal_simple("screen::prop::active_workspace", L, output);
    cwc_object_emit_signal_simple("screen::prop::selected_tag", L,
                                  cwc_output_get_current_tag_info(output));
}

void cwc_output_set_active_tag(struct cwc_output *output, tag_bitfield_t newtag)
{
    if (newtag == output->state->active_tag)
        return;

    if (!(newtag >> (output->state->active_workspace - 1) & 1))
        output->state->active_workspace = cwc_tag_find_first_tag(newtag);

    output->state->active_tag = newtag;
    transaction_schedule_output(output);
    transaction_schedule_tag(cwc_output_get_current_tag_info(output));

    cwc_object_emit_signal_simple("screen::prop::active_tag",
                                  g_config_get_lua_State(), output);
}

static void restore_floating_box_for_all(struct cwc_output *output)
{
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        if (cwc_container_is_floating(container)
            && cwc_container_is_visible(container)
            && cwc_container_is_configure_allowed(container))
            cwc_container_restore_floating_box(container);
    }
}

void cwc_output_set_layout_mode(struct cwc_output *output,
                                int workspace,
                                enum cwc_layout_mode mode)
{

    if (mode < 0 || mode >= CWC_LAYOUT_LENGTH)
        return;

    if (!workspace)
        workspace = output->state->active_workspace;

    output->state->tag_info[workspace].layout_mode = mode;

    switch (mode) {
    case CWC_LAYOUT_BSP:
        insert_tiled_toplevel_to_bsp_tree(output, workspace);
        break;
    case CWC_LAYOUT_FLOATING:
        restore_floating_box_for_all(output);
        break;
    default:
        break;
    }

    transaction_schedule_tag(cwc_output_get_tag(output, workspace));
}

void cwc_output_set_strategy_idx(struct cwc_output *output, int idx)
{
    struct cwc_tag_info *info         = cwc_output_get_current_tag_info(output);
    enum cwc_layout_mode current_mode = info->layout_mode;

    switch (current_mode) {
    case CWC_LAYOUT_BSP:
        break;
    case CWC_LAYOUT_MASTER:
        if (idx > 0)
            while (idx--)
                info->master_state.current_layout =
                    info->master_state.current_layout->next;
        else if (idx < 0)
            while (idx++)
                info->master_state.current_layout =
                    info->master_state.current_layout->prev;
        transaction_schedule_tag(cwc_output_get_current_tag_info(output));
        break;
    default:
        return;
    }
}

void cwc_output_set_useless_gaps(struct cwc_output *output,
                                 int workspace,
                                 int gaps_width)
{
    if (!workspace)
        workspace = output->state->active_workspace;

    workspace  = CLAMP(workspace, 1, MAX_WORKSPACE);
    gaps_width = MAX(0, gaps_width);

    output->state->tag_info[workspace].useless_gaps = gaps_width;
    transaction_schedule_tag(cwc_output_get_tag(output, workspace));
}

void cwc_output_set_mwfact(struct cwc_output *output,
                           int workspace,
                           double factor)
{
    if (!workspace)
        workspace = output->state->active_workspace;

    workspace = CLAMP(workspace, 1, MAX_WORKSPACE);
    factor    = CLAMP(factor, 0.1, 0.9);

    struct cwc_tag_info *tag = cwc_output_get_tag(output, workspace);
    tag->master_state.mwfact = factor;
    transaction_schedule_tag(tag);
}

int cwc_tag_find_first_tag(tag_bitfield_t tag)
{
    for (int i = 1; i < MAX_WORKSPACE; i++) {
        if (tag & 1)
            return i;
        tag >>= 1;
    }

    return 0;
}
