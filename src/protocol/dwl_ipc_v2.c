/* dwl_ipc_v2.c - dwl IPC protocol impl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output.h>

#include "cwc/protocol/dwl_ipc_v2.h"
#include "cwc/util.h"
#include "dwl-ipc-unstable-v2-protocol.h"

#define DWL_IPC_VERSION 2

static void output_idle_send_frame(void *data)
{
    struct cwc_dwl_ipc_output_v2 *output = data;
    zdwl_ipc_output_v2_send_frame(output->resource);
    output->idle_source = NULL;
}

static void output_update_idle_source(struct cwc_dwl_ipc_output_v2 *output)
{
    if (output->idle_source)
        return;

    output->idle_source = wl_event_loop_add_idle(
        output->manager->event_loop, output_idle_send_frame, output);
}

void cwc_dwl_ipc_output_v2_toggle_visibility(
    struct cwc_dwl_ipc_output_v2 *output)
{
    zdwl_ipc_output_v2_send_toggle_visibility(output->resource);
}

void cwc_dwl_ipc_output_v2_update_tag(
    struct cwc_dwl_ipc_output_v2 *output,
    struct cwc_dwl_ipc_output_v2_tag_state *state)
{
    zdwl_ipc_output_v2_send_tag(output->resource, state->index, state->state,
                                state->clients, state->focused);

    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_active(struct cwc_dwl_ipc_output_v2 *output,
                                      bool active)
{
    output->active = active;
    zdwl_ipc_output_v2_send_active(output->resource, active);
    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_title(struct cwc_dwl_ipc_output_v2 *output,
                                     const char *title)
{
    free(output->title);
    output->title = strdup(title);
    if (!output->title) {
        cwc_log(CWC_ERROR, "failed to allocate memory for ipc output title");
        return;
    }

    zdwl_ipc_output_v2_send_title(output->resource, title);
    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_appid(struct cwc_dwl_ipc_output_v2 *output,
                                     const char *appid)
{
    free(output->appid);
    output->appid = strdup(appid);
    if (!output->appid) {
        cwc_log(CWC_ERROR, "failed to allocate memory for ipc output appid");
        return;
    }

    zdwl_ipc_output_v2_send_appid(output->resource, appid);
    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_layout_symbol(
    struct cwc_dwl_ipc_output_v2 *output, const char *layout_symbol)
{
    free(output->layout_symbol);
    output->layout_symbol = strdup(layout_symbol);
    if (!output->layout_symbol) {
        cwc_log(CWC_ERROR,
                "failed to allocate memory for ipc output layout symbol");
        return;
    }

    zdwl_ipc_output_v2_send_layout_symbol(output->resource, layout_symbol);
    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_fullscreen(struct cwc_dwl_ipc_output_v2 *output,
                                          bool fullscreen)
{
    output->fullscreen = fullscreen;
    zdwl_ipc_output_v2_send_fullscreen(output->resource, fullscreen);
    output_update_idle_source(output);
}

void cwc_dwl_ipc_output_v2_set_floating(struct cwc_dwl_ipc_output_v2 *output,
                                        bool floating)
{
    output->floating = floating;
    zdwl_ipc_output_v2_send_floating(output->resource, floating);
    output_update_idle_source(output);
}

static void dwl_ipc_output_handle_release(struct wl_client *client,
                                          struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void dwl_ipc_output_handle_set_tags(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t tagmask,
                                           uint32_t toggle_tagset)
{
    struct cwc_dwl_ipc_output_v2 *output = wl_resource_get_user_data(resource);
    struct cwc_dwl_ipc_output_v2_tags_event event = {
        .tagmask       = tagmask,
        .toggle_tagset = toggle_tagset,
    };
    wl_signal_emit_mutable(&output->events.request_tags, &event);
}

static void dwl_ipc_output_handle_set_client_tags(struct wl_client *client,
                                                  struct wl_resource *resource,
                                                  uint32_t and_tags,
                                                  uint32_t xor_tags)
{
    struct cwc_dwl_ipc_output_v2 *output = wl_resource_get_user_data(resource);
    struct cwc_dwl_ipc_output_v2_client_tags_event event = {
        .and_tags = and_tags,
        .xor_tags = xor_tags,
    };
    wl_signal_emit_mutable(&output->events.request_client_tags, &event);
}

static void dwl_ipc_output_handle_set_layout(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t index)
{
    struct cwc_dwl_ipc_output_v2 *output = wl_resource_get_user_data(resource);
    wl_signal_emit_mutable(&output->events.request_layout, &index);
}

struct zdwl_ipc_output_v2_interface dwl_ipc_output_impl = {
    .release         = dwl_ipc_output_handle_release,
    .set_tags        = dwl_ipc_output_handle_set_tags,
    .set_client_tags = dwl_ipc_output_handle_set_client_tags,
    .set_layout      = dwl_ipc_output_handle_set_layout,
};

static void cwc_dwl_ipc_output_v2_destroy(struct cwc_dwl_ipc_output_v2 *output);
static void dwl_ipc_output_resource_destroy(struct wl_resource *resource)
{
    cwc_dwl_ipc_output_v2_destroy(wl_resource_get_user_data(resource));
}

static void on_output_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_dwl_ipc_output_v2 *ipc_o =
        wl_container_of(listener, ipc_o, output_destroy);

    wl_resource_destroy(ipc_o->resource);
}

static struct cwc_dwl_ipc_output_v2 *
cwc_dwl_ipc_output_v2_create(struct wl_client *client,
                             struct wl_resource *manager_resource,
                             uint32_t id,
                             struct wl_resource *output_resource)
{
    struct cwc_dwl_ipc_output_v2 *ipc_output = calloc(1, sizeof(*ipc_output));
    if (!ipc_output)
        return NULL;

    ipc_output->resource =
        wl_resource_create(client, &zdwl_ipc_output_v2_interface,
                           wl_resource_get_version(manager_resource), id);
    if (!ipc_output->resource) {
        free(ipc_output);
        return NULL;
    }

    wl_resource_set_implementation(ipc_output->resource, &dwl_ipc_output_impl,
                                   ipc_output, dwl_ipc_output_resource_destroy);
    wl_resource_set_user_data(ipc_output->resource, ipc_output);

    ipc_output->manager = wl_resource_get_user_data(manager_resource);
    ipc_output->output  = wlr_output_from_resource(output_resource);

    wl_list_insert(&ipc_output->manager->resources,
                   wl_resource_get_link(ipc_output->resource));
    wl_list_insert(&ipc_output->manager->outputs, &ipc_output->link);

    wl_signal_init(&ipc_output->events.request_tags);
    wl_signal_init(&ipc_output->events.request_client_tags);
    wl_signal_init(&ipc_output->events.request_layout);
    wl_signal_init(&ipc_output->events.destroy);

    ipc_output->output_destroy.notify = on_output_destroy;
    wl_signal_add(&ipc_output->output->events.destroy,
                  &ipc_output->output_destroy);

    return ipc_output;
}

static void cwc_dwl_ipc_output_v2_destroy(struct cwc_dwl_ipc_output_v2 *output)
{
    if (!output)
        return;

    wl_signal_emit_mutable(&output->events.destroy, output);

    assert(wl_list_empty(&output->events.request_layout.listener_list));
    assert(wl_list_empty(&output->events.request_client_tags.listener_list));
    assert(wl_list_empty(&output->events.request_tags.listener_list));
    assert(wl_list_empty(&output->events.destroy.listener_list));

    if (output->idle_source)
        wl_event_source_remove(output->idle_source);

    wl_list_remove(&output->link);
    wl_list_remove(&output->output_destroy.link);
    wl_list_remove(wl_resource_get_link(output->resource));

    free(output->title);
    free(output->appid);
    free(output);
}

static void dwl_ipc_manager_handle_release(struct wl_client *client,
                                           struct wl_resource *manager_resource)
{
    wl_resource_destroy(manager_resource);
}

static const struct zdwl_ipc_manager_v2_interface dwl_ipc_manager_impl;

static struct cwc_dwl_ipc_manager_v2 *
dwl_ipc_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource, &zdwl_ipc_manager_v2_interface,
                                   &dwl_ipc_manager_impl));
    return wl_resource_get_user_data(resource);
}

static void
dwl_ipc_manager_handle_get_output(struct wl_client *client,
                                  struct wl_resource *manager_resource,
                                  uint32_t id,
                                  struct wl_resource *output_resource)
{
    struct cwc_dwl_ipc_manager_v2 *manager =
        dwl_ipc_manager_from_resource(manager_resource);

    struct cwc_dwl_ipc_output_v2 *ipc_output = cwc_dwl_ipc_output_v2_create(
        client, manager_resource, id, output_resource);
    if (!ipc_output) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_signal_emit_mutable(&manager->events.new_output, ipc_output);
}

static void dwl_ipc_manager_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

static const struct zdwl_ipc_manager_v2_interface dwl_ipc_manager_impl = {
    .release    = dwl_ipc_manager_handle_release,
    .get_output = dwl_ipc_manager_handle_get_output,
};

static void dwl_ipc_manager_bind(struct wl_client *client,
                                 void *data,
                                 uint32_t version,
                                 uint32_t id)
{
    struct cwc_dwl_ipc_manager_v2 *manager = data;
    struct wl_resource *manager_resource =
        wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
    if (!manager_resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(manager_resource, &dwl_ipc_manager_impl,
                                   manager, dwl_ipc_manager_resource_destroy);

    zdwl_ipc_manager_v2_send_tags(manager_resource, manager->tags_amount);

    wl_list_insert(&manager->resources, wl_resource_get_link(manager_resource));
}

static void handle_display_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_dwl_ipc_manager_v2 *manager =
        wl_container_of(listener, manager, display_destroy);
    cwc_dwl_ipc_manager_v2_destroy(manager);
}

struct cwc_dwl_ipc_manager_v2 *
cwc_dwl_ipc_manager_v2_create(struct wl_display *display)
{
    struct cwc_dwl_ipc_manager_v2 *manager = calloc(1, sizeof(*manager));
    if (!manager)
        return NULL;

    manager->tags_amount = 9;
    manager->event_loop  = wl_display_get_event_loop(display);
    manager->global =
        wl_global_create(display, &zdwl_ipc_manager_v2_interface,
                         DWL_IPC_VERSION, manager, dwl_ipc_manager_bind);

    if (!manager->global) {
        free(manager->global);
        return NULL;
    }

    wl_signal_init(&manager->events.destroy);
    wl_signal_init(&manager->events.new_output);

    wl_list_init(&manager->resources);
    wl_list_init(&manager->outputs);

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

void cwc_dwl_ipc_manager_v2_destroy(struct cwc_dwl_ipc_manager_v2 *manager)
{
    wl_signal_emit_mutable(&manager->events.destroy, manager);

    assert(wl_list_empty(&manager->events.new_output.listener_list));
    assert(wl_list_empty(&manager->events.destroy.listener_list));

    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);

    free(manager);
}
