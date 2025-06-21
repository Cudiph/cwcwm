#ifndef _CWC_PROTOCOL_DWL_IPC_V2
#define _CWC_PROTOCOL_DWL_IPC_V2

#include <wayland-server-core.h>

struct cwc_dwl_ipc_manager_v2 {
    struct wl_event_loop *event_loop;
    struct wl_global *global;
    struct wl_list resources; // wl_resource_get_link()
    struct wl_list outputs;   // struct cwc_dwl_ipc_output_v2.link

    uint32_t tags_amount;

    struct {
        struct wl_signal new_output; // struct cwc_dwl_ipc_output_v2
        struct wl_signal destroy;
    } events;

    void *data;

    struct {
        struct wl_listener display_destroy;
    } CWC_PRIVATE;
};

struct cwc_dwl_ipc_manager_v2 *
cwc_dwl_ipc_manager_v2_create(struct wl_display *display);

void cwc_dwl_ipc_manager_v2_destroy(struct cwc_dwl_ipc_manager_v2 *manager);

static inline void
cwc_dwl_ipc_manager_v2_set_tags_amount(struct cwc_dwl_ipc_manager_v2 *manager,
                                       uint32_t amount)
{
    manager->tags_amount = amount;
}

struct cwc_dwl_ipc_output_v2 {
    struct cwc_dwl_ipc_manager_v2 *manager;
    struct wl_list link;
    struct wl_event_source *idle_source;
    struct wl_resource *resource;

    struct wlr_output *output;
    bool active;
    bool fullscreen;
    bool floating;
    char *title;
    char *appid;
    char *layout_symbol;

    struct {
        // struct cwc_dwl_ipc_output_v2_tags_event
        struct wl_signal request_tags;
        // struct cwc_dwl_ipc_output_v2_client_tags_event
        struct wl_signal request_client_tags;
        // uint32_t
        struct wl_signal request_layout;

        struct wl_signal destroy;
    } events;

    void *data;

    struct {
        struct wl_listener output_destroy;
    } CWC_PRIVATE;
};

struct cwc_dwl_ipc_output_v2_tag_state {
    uint32_t index;
    uint32_t state; // ZDWL_IPC_OUTPUT_V2_TAG_STATE_ENUM
    uint32_t clients;
    bool focused;
};

void cwc_dwl_ipc_output_v2_toggle_visibility(
    struct cwc_dwl_ipc_output_v2 *output);
void cwc_dwl_ipc_output_v2_update_tag(
    struct cwc_dwl_ipc_output_v2 *output,
    struct cwc_dwl_ipc_output_v2_tag_state *state);

void cwc_dwl_ipc_output_v2_set_title(struct cwc_dwl_ipc_output_v2 *output,
                                     const char *title);
void cwc_dwl_ipc_output_v2_set_appid(struct cwc_dwl_ipc_output_v2 *output,
                                     const char *appid);
void cwc_dwl_ipc_output_v2_set_layout_symbol(
    struct cwc_dwl_ipc_output_v2 *output, const char *layout_symbol);

void cwc_dwl_ipc_output_v2_set_active(struct cwc_dwl_ipc_output_v2 *output,
                                      bool active);
void cwc_dwl_ipc_output_v2_set_fullscreen(struct cwc_dwl_ipc_output_v2 *output,
                                          bool fullscreen);
void cwc_dwl_ipc_output_v2_set_floating(struct cwc_dwl_ipc_output_v2 *output,
                                        bool floating);

struct cwc_dwl_ipc_output_v2_tags_event {
    uint32_t tagmask;
    uint32_t toggle_tagset;
};

struct cwc_dwl_ipc_output_v2_client_tags_event {
    uint32_t and_tags;
    uint32_t xor_tags;
};

#endif // !_CWC_PROTOCOL_DWL_IPC_V2
