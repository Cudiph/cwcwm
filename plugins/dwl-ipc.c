#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/container.h"
#include "cwc/plugin.h"
#include "cwc/protocol/dwl_ipc_v2.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/types.h"
#include "dwl-ipc-unstable-v2-protocol.h"

static struct cwc_dwl_ipc_manager_v2 *manager;
static struct wl_listener on_new_dwl_ipc_output_l;

struct cwc_ipc_output {
    struct wl_list link;
    struct cwc_dwl_ipc_output_v2 *output_handle;

    struct wl_listener request_set_tags_l;
    struct wl_listener request_set_client_tags_l;

    struct wl_listener destroy_l;
};

struct cwc_output_addon {
    struct wlr_addon addon;
    struct cwc_output *output;
    struct cwc_toplevel *toplevel;
    struct wl_event_source *tag_update_idle_source;
    struct wl_event_source *prop_change_idle_source;
    struct wl_list ipc_outputs; // struct cwc_ipc_output.link
};

static void ipc_output_addon_destroy(struct wlr_addon *addon)
{
    struct cwc_output_addon *output_addon =
        wl_container_of(addon, output_addon, addon);

    wlr_addon_finish(addon);

    free(output_addon);
}

struct wlr_addon_interface ipc_output_addon_impl = {
    .name = "cwc_ipc_output", .destroy = ipc_output_addon_destroy};

static struct cwc_output_addon *
cwc_output_get_output_addon(struct cwc_output *output)
{
    struct wlr_addon *addon = wlr_addon_find(&output->wlr_output->addons,
                                             output, &ipc_output_addon_impl);
    if (!addon)
        return NULL;

    struct cwc_output_addon *o_addon = wl_container_of(addon, o_addon, addon);
    return o_addon;
}

static void on_request_set_tags(struct wl_listener *listener, void *data)
{
    struct cwc_ipc_output *ipc_output =
        wl_container_of(listener, ipc_output, request_set_tags_l);
    struct cwc_output *output = ipc_output->output_handle->output->data;
    struct cwc_dwl_ipc_output_v2_tags_event *event = data;

    cwc_output_set_active_tag(output, event->tagmask);
}

static void on_request_set_client_tags(struct wl_listener *listener, void *data)
{
    struct cwc_ipc_output *ipc_output =
        wl_container_of(listener, ipc_output, request_set_client_tags_l);
    struct cwc_dwl_ipc_output_v2_client_tags_event *event = data;

    struct cwc_toplevel *focused = cwc_toplevel_get_focused();
    cwc_container_set_tag(focused->container,
                          (focused->container->tag & event->and_tags)
                              ^ event->xor_tags);
}

static void on_ipc_output_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_ipc_output *ipc_output =
        wl_container_of(listener, ipc_output, destroy_l);

    wl_list_remove(&ipc_output->link);

    wl_list_remove(&ipc_output->request_set_tags_l.link);
    wl_list_remove(&ipc_output->request_set_client_tags_l.link);
    wl_list_remove(&ipc_output->destroy_l.link);

    free(ipc_output);
}

static void
get_ipc_output_tag_state(struct cwc_output *cwc_o,
                         struct cwc_tag_info *tag,
                         struct cwc_dwl_ipc_output_v2_tag_state *state)
{
    /* dwl tag index is zero based index while cwc tag start from one */
    *state = (struct cwc_dwl_ipc_output_v2_tag_state){.index = tag->index - 1};
    if (cwc_o->state->active_tag & 1 << (tag->index - 1))
        state->state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;

    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &cwc_o->state->toplevels, link_output_toplevels)
    {
        if (toplevel->container->tag & 1 << (tag->index - 1)) {
            state->clients++;

            if (toplevel == cwc_toplevel_get_focused())
                state->focused = true;

            if (cwc_toplevel_is_urgent(toplevel))
                state->state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;
        }
    }
}

static void on_new_dwl_ipc_output(struct wl_listener *listener, void *data)
{
    struct cwc_dwl_ipc_output_v2 *output_handle = data;
    struct cwc_output *output                   = output_handle->output->data;

    struct cwc_toplevel *toplevel =
        cwc_output_get_newest_focus_toplevel(output, true);

    if (toplevel) {
        cwc_dwl_ipc_output_v2_set_appid(output_handle,
                                        cwc_toplevel_get_app_id(toplevel));
        cwc_dwl_ipc_output_v2_set_title(output_handle,
                                        cwc_toplevel_get_title(toplevel));
    }

    cwc_dwl_ipc_output_v2_set_active(
        output_handle, cwc_output_get_focused() == output_handle->output->data);

    for (int i = 1; i <= output->state->max_general_workspace; i++) {
        struct cwc_dwl_ipc_output_v2_tag_state state;
        struct cwc_tag_info *tag = cwc_output_get_tag(output, i);
        get_ipc_output_tag_state(output, tag, &state);
        cwc_dwl_ipc_output_v2_update_tag(output_handle, &state);
    }

    struct cwc_ipc_output *ipc_output = calloc(1, sizeof(*ipc_output));
    ipc_output->output_handle         = output_handle;
    output_handle->data               = ipc_output;

    ipc_output->request_set_tags_l.notify        = on_request_set_tags;
    ipc_output->request_set_client_tags_l.notify = on_request_set_client_tags;
    ipc_output->destroy_l.notify                 = on_ipc_output_destroy;
    wl_signal_add(&output_handle->events.request_tags,
                  &ipc_output->request_set_tags_l);
    wl_signal_add(&output_handle->events.request_client_tags,
                  &ipc_output->request_set_client_tags_l);
    wl_signal_add(&output_handle->events.destroy, &ipc_output->destroy_l);

    wl_list_init(&ipc_output->link);
    struct cwc_output_addon *o_addon = cwc_output_get_output_addon(output);
    if (!o_addon)
        return;
    wl_list_insert(&o_addon->ipc_outputs, &ipc_output->link);
}

static void on_client_should_title_reset(void *data)
{
    struct cwc_toplevel *toplevel         = data;
    struct cwc_output *output             = toplevel->container->output;
    struct cwc_output_addon *output_addon = cwc_output_get_output_addon(output);
    if (!output_addon || output != cwc_output_get_focused())
        return;

    struct cwc_ipc_output *ipc_output;
    wl_list_for_each(ipc_output, &output_addon->ipc_outputs, link)
    {
        cwc_dwl_ipc_output_v2_set_title(ipc_output->output_handle, "");
        cwc_dwl_ipc_output_v2_set_appid(ipc_output->output_handle, "");
    }
}

static void on_screen_new(void *data)
{
    struct cwc_output *output      = data;
    struct cwc_output_addon *exist = cwc_output_get_output_addon(output);
    if (exist)
        return;

    struct cwc_output_addon *output_addon = calloc(1, sizeof(*output_addon));
    output_addon->output                  = output;
    wl_list_init(&output_addon->ipc_outputs);

    wlr_addon_init(&output_addon->addon, &output->wlr_output->addons, output,
                   &ipc_output_addon_impl);
}

static void update_all_tag_state_idle(void *data)
{
    struct cwc_output_addon *output_addon = data;
    struct cwc_output *output             = output_addon->output;

    output_addon->tag_update_idle_source = NULL;

    struct cwc_dwl_ipc_output_v2_tag_state
        states[output->state->max_general_workspace + 2];
    for (int i = 1; i <= output->state->max_general_workspace; i++) {
        struct cwc_tag_info *tag = cwc_output_get_tag(output, i);
        get_ipc_output_tag_state(output, tag, &states[i]);
    }

    struct cwc_ipc_output *ipc_output;
    wl_list_for_each(ipc_output, &output_addon->ipc_outputs, link)
    {
        for (int i = 1; i <= output->state->max_general_workspace; i++) {
            cwc_dwl_ipc_output_v2_update_tag(ipc_output->output_handle,
                                             &states[i]);
        }
    }
}

static void update_tag_idle_source(struct cwc_output *output)
{
    struct cwc_output_addon *output_addon = cwc_output_get_output_addon(output);
    if (!output_addon)
        return;

    if (output_addon->tag_update_idle_source)
        return;

    output_addon->output                 = output;
    output_addon->tag_update_idle_source = wl_event_loop_add_idle(
        server.wl_event_loop, update_all_tag_state_idle, output_addon);
}

static void on_client_prop_change_and_update_tag(void *data)
{
    struct cwc_toplevel *toplevel = data;
    update_tag_idle_source(toplevel->container->output);
}

static void update_prop_idle(void *data)
{
    struct cwc_output_addon *output_addon = data;
    struct cwc_toplevel *toplevel         = output_addon->toplevel;

    output_addon->prop_change_idle_source = NULL;
    if (toplevel != cwc_toplevel_get_focused())
        return;

    struct cwc_ipc_output *ipc_output;
    wl_list_for_each(ipc_output, &output_addon->ipc_outputs, link)
    {
        char *title = cwc_toplevel_get_title(toplevel);
        char *appid = cwc_toplevel_get_app_id(toplevel);
        title       = title ? title : "";
        appid       = appid ? appid : "";
        cwc_dwl_ipc_output_v2_set_fullscreen(
            ipc_output->output_handle, cwc_toplevel_is_fullscreen(toplevel));
        cwc_dwl_ipc_output_v2_set_floating(ipc_output->output_handle,
                                           cwc_toplevel_is_floating(toplevel));
        cwc_dwl_ipc_output_v2_set_title(ipc_output->output_handle, title);
        cwc_dwl_ipc_output_v2_set_appid(ipc_output->output_handle, appid);
    }
}

static void on_client_prop_change(void *data)
{
    struct cwc_toplevel *toplevel = data;
    if (toplevel != cwc_toplevel_get_focused() || !toplevel->container)
        return;
    struct cwc_output_addon *output_addon =
        cwc_output_get_output_addon(toplevel->container->output);
    if (!output_addon)
        return;

    if (output_addon->prop_change_idle_source)
        return;

    output_addon->toplevel                = toplevel;
    output_addon->prop_change_idle_source = wl_event_loop_add_idle(
        server.wl_event_loop, update_prop_idle, output_addon);
}

static void on_screen_prop_active_tag(void *data)
{
    struct cwc_output *output = data;
    update_tag_idle_source(output);
}

static void on_screen_focus(void *data)
{
    struct cwc_output *output             = data;
    struct cwc_output_addon *output_addon = cwc_output_get_output_addon(output);
    if (!output_addon)
        return;

    struct cwc_ipc_output *ipc_output;
    wl_list_for_each(ipc_output, &output_addon->ipc_outputs, link)
    {
        cwc_dwl_ipc_output_v2_set_active(ipc_output->output_handle, true);
    }
}

static void on_screen_unfocus(void *data)
{
    struct cwc_output *output             = data;
    struct cwc_output_addon *output_addon = cwc_output_get_output_addon(output);
    if (!output_addon)
        return;

    struct cwc_ipc_output *ipc_output;
    wl_list_for_each(ipc_output, &output_addon->ipc_outputs, link)
    {
        cwc_dwl_ipc_output_v2_set_active(ipc_output->output_handle, false);
    }
}

static void register_addon()
{
    struct cwc_output *o;
    wl_list_for_each(o, &server.outputs, link)
    {
        struct cwc_output_addon *exist = cwc_output_get_output_addon(o);
        if (exist)
            continue;

        struct cwc_output_addon *o_addon = calloc(1, sizeof(*o_addon));
        o_addon->output                  = o;
        wl_list_init(&o_addon->ipc_outputs);

        wlr_addon_init(&o_addon->addon, &o->wlr_output->addons, o,
                       &ipc_output_addon_impl);
    }
}

static int dwl_ipc_setup()
{
    register_addon();

    manager = cwc_dwl_ipc_manager_v2_create(server.wl_display);
    cwc_dwl_ipc_manager_v2_set_tags_amount(manager, MAX_WORKSPACE);

    on_new_dwl_ipc_output_l.notify = on_new_dwl_ipc_output;
    wl_signal_add(&manager->events.new_output, &on_new_dwl_ipc_output_l);

    cwc_signal_connect("client::focus", on_client_prop_change);
    cwc_signal_connect("client::unfocus", on_client_should_title_reset);
    cwc_signal_connect("client::unmap", on_client_should_title_reset);
    cwc_signal_connect("client::property::urgent",
                       on_client_prop_change_and_update_tag);
    cwc_signal_connect("client::property::tag",
                       on_client_prop_change_and_update_tag);
    cwc_signal_connect("client::property::fullscreen", on_client_prop_change);
    cwc_signal_connect("client::property::floating", on_client_prop_change);
    cwc_signal_connect("client::prop::title", on_client_prop_change);
    cwc_signal_connect("client::prop::appid", on_client_prop_change);

    cwc_signal_connect("screen::new", on_screen_new);
    cwc_signal_connect("screen::focus", on_screen_focus);
    cwc_signal_connect("screen::unfocus", on_screen_unfocus);
    cwc_signal_connect("screen::prop::active_tag", on_screen_prop_active_tag);

    return 0;
}

static void unregister_addon()
{
    struct cwc_output *o;
    wl_list_for_each(o, &server.outputs, link)
    {
        struct cwc_output_addon *output_addon = cwc_output_get_output_addon(o);
        if (!output_addon)
            continue;

        struct cwc_ipc_output *ipc_output, *tmp;
        wl_list_for_each_safe(ipc_output, tmp, &output_addon->ipc_outputs, link)
        {
            wl_resource_destroy(ipc_output->output_handle->resource);
        }

        wlr_addon_finish(&output_addon->addon);
        free(output_addon);
    }
}

static void dwl_ipc_cleanup()
{
    unregister_addon();

    wl_list_remove(&on_new_dwl_ipc_output_l.link);
    cwc_dwl_ipc_manager_v2_destroy(manager);

    cwc_signal_disconnect("client::focus", on_client_prop_change);
    cwc_signal_disconnect("client::unfocus", on_client_should_title_reset);
    cwc_signal_disconnect("client::unmap", on_client_should_title_reset);
    cwc_signal_disconnect("client::property::urgent",
                          on_client_prop_change_and_update_tag);
    cwc_signal_disconnect("client::property::tag",
                          on_client_prop_change_and_update_tag);
    cwc_signal_disconnect("client::property::fullscreen",
                          on_client_prop_change);
    cwc_signal_disconnect("client::property::floating", on_client_prop_change);
    cwc_signal_disconnect("client::prop::title", on_client_prop_change);
    cwc_signal_disconnect("client::prop::appid", on_client_prop_change);

    cwc_signal_disconnect("screen::new", on_screen_new);
    cwc_signal_disconnect("screen::focus", on_screen_focus);
    cwc_signal_disconnect("screen::unfocus", on_screen_unfocus);
    cwc_signal_disconnect("screen::prop::active_tag",
                          on_screen_prop_active_tag);
}

plugin_init(dwl_ipc_setup);
plugin_exit(dwl_ipc_cleanup);

PLUGIN_NAME("dwl-ipc");
PLUGIN_VERSION("0.1.0");
PLUGIN_DESCRIPTION("dwl IPC plugin");
PLUGIN_LICENSE("GPLv3");
PLUGIN_AUTHOR("Dwi Asmoro Bangun <dwiaceromo@gmail.com>");
