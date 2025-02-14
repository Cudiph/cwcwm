#ifndef _CWC_OUTPUT_H
#define _CWC_OUTPUT_H

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>

#include "cwc/types.h"

struct cwc_server;

/* output state that can be restored in case the output will come back.
 * wlroots patch 0f255b46 remove automatic reset on vt switch and switching vt
 * will destroy the wlr_output.
 */
struct cwc_output_state {
    struct wl_list toplevels;   // cwc_toplevel.link_output_toplevels
    struct wl_list focus_stack; // cwc_container.link_output_fstack
    struct wl_list containers;  // cwc_container.link_output_container
    struct wl_list minimized;   // cwc_container.link_output_minimized

    struct cwc_output *output;
    struct cwc_output *old_output;

    /* the tag used to decide if client visible */
    tag_bitfield_t active_tag;
    /* act as tag info index cuz multiple tag can be active */
    int active_workspace;
    /* workspace count that will be shown in the bar and for cycling */
    int max_general_workspace;

    /* use array for now too lazy to manage the memory */
    struct cwc_tag_info tag_info[MAX_WORKSPACE + 1];
};

/* wlr_output.data == cwc_output */
struct cwc_output {
    enum cwc_data_type type;
    struct wl_list link; // server.outputs
    struct wlr_output *wlr_output;
    struct wlr_box usable_area;
    struct wlr_box output_layout_box;

    struct wlr_scene_output *scene_output;
    struct cwc_output_state *state;
    struct wlr_output_state pending;
    bool pending_initialized;
    bool restored;
    bool tearing_allowed;

    struct timespec waiting_since;

    /* direct children of the root with the same name */
    struct {
        struct wlr_scene_tree *background;   // layer_shell
        struct wlr_scene_tree *bottom;       // layer_shell
        struct wlr_scene_tree *top;          // layer_shell
        struct wlr_scene_tree *overlay;      // layer_shell
        struct wlr_scene_tree *session_lock; // session_lock
    } layers;

    struct wl_listener frame_l;
    struct wl_listener request_state_l;
    struct wl_listener destroy_l;
    struct wl_listener presentation_l;

    struct wl_listener config_commit_l;
};

/* set workspace to zero to update the current workspace */
void cwc_output_tiling_layout_update(struct cwc_output *output, int workspace);
void cwc_output_update_visible(struct cwc_output *output);

/* free it after use, NULL indicates the end of the array */
struct cwc_toplevel **
cwc_output_get_visible_toplevels(struct cwc_output *output);

/* free it after use, NULL indicates the end of the array */
struct cwc_container **
cwc_output_get_visible_containers(struct cwc_output *output);

/* unmanaged toplevel is skipped */
struct cwc_toplevel *cwc_output_get_newest_toplevel(struct cwc_output *output,
                                                    bool visible);
struct cwc_toplevel *
cwc_output_get_newest_focus_toplevel(struct cwc_output *output, bool visible);

/* exact name */
struct cwc_output *cwc_output_get_by_name(const char *name);

struct cwc_output *
cwc_output_at(struct wlr_output_layout *ol, double x, double y);

/* refocus toplevel after for example return from session lock  */
void cwc_output_focus_newest_focus_visible_toplevel(struct cwc_output *output);

/* update output management configuration and other states */
void cwc_output_update_outputs_state();

/* set output position in the global coordinate */
void cwc_output_set_position(struct cwc_output *output, int x, int y);

//================== TAGS ===================

/* if workspace is 0, it will use the active workspace of the output */

/* set inner gap width */
void cwc_output_set_useless_gaps(struct cwc_output *output,
                                 int workspace,
                                 int width);

/* set master width factor */
void cwc_output_set_mwfact(struct cwc_output *output,
                           int workspace,
                           double factor);

/* set a tag to be the only active tag */
void cwc_output_set_view_only(struct cwc_output *output, int workspace);

/* update active tag of the output */
void cwc_output_set_active_tag(struct cwc_output *output,
                               tag_bitfield_t newtag);

/* set layout mode at the specified workspace */
void cwc_output_set_layout_mode(struct cwc_output *output,
                                int workspace,
                                enum cwc_layout_mode mode);

void cwc_output_set_strategy_idx(struct cwc_output *output, int idx);

/* return the workspace number, 0 if not found */
int cwc_tag_find_first_tag(tag_bitfield_t tag);

//================== MACRO ==================

struct cwc_output *cwc_output_get_focused();
bool cwc_output_is_exist(struct cwc_output *output);

static inline struct cwc_tag_info *
cwc_output_get_current_tag_info(struct cwc_output *output)
{
    return &output->state->tag_info[output->state->active_workspace];
}

static inline bool cwc_output_is_current_layout_float(struct cwc_output *output)
{
    return cwc_output_get_current_tag_info(output)->layout_mode
           == CWC_LAYOUT_FLOATING;
}

static inline bool
cwc_output_is_current_layout_master(struct cwc_output *output)
{
    return cwc_output_get_current_tag_info(output)->layout_mode
           == CWC_LAYOUT_MASTER;
}

static inline bool cwc_output_is_current_layout_bsp(struct cwc_output *output)
{
    return cwc_output_get_current_tag_info(output)->layout_mode
           == CWC_LAYOUT_BSP;
}

static inline void
cwc_output_tiling_layout_update_all_general_workspace(struct cwc_output *output)
{
    for (int i = 1; i < output->state->max_general_workspace; i++) {
        cwc_output_tiling_layout_update(output, i);
    }
}

static inline bool cwc_output_is_allow_tearing(struct cwc_output *output)
{
    return output->tearing_allowed;
}

static inline void cwc_output_set_allow_tearing(struct cwc_output *output,
                                                bool set)
{
    output->tearing_allowed = set;
}

#define cwc_output_from_tag_info(ptr)                                       \
    (struct cwc_output                                                      \
         *)((struct cwc_output_state *)((char *)(ptr)                       \
                                        - offsetof(struct cwc_output_state, \
                                                   tag_info[ptr->index])))  \
        ->output

#endif // !_CWC_OUTPUT_H
#define _CWC_OUTPUT_H
