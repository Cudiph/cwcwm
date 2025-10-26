#ifndef _CWC_INPUT_CURSOR_H
#define _CWC_INPUT_CURSOR_H

#include <hyprcursor/hyprcursor.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>

struct cwc_server;

enum cwc_cursor_state {
    CWC_CURSOR_STATE_NORMAL,

    CWC_CURSOR_STATE_MOVE,
    CWC_CURSOR_STATE_RESIZE,

    CWC_CURSOR_STATE_MOVE_BSP,
    CWC_CURSOR_STATE_RESIZE_BSP,

    CWC_CURSOR_STATE_MOVE_MASTER,
    CWC_CURSOR_STATE_RESIZE_MASTER,
};

enum cwc_cursor_pseudo_btn {
    SCROLL_LEFT = 0x13371,
    SCROLL_UP,
    SCROLL_RIGHT,
    SCROLL_DOWN,
};

struct hyprcursor_buffer {
    struct wlr_buffer base;
    cairo_surface_t *surface;
};

struct bsp_grab {
    struct bsp_node *horizontal;
    struct bsp_node *vertical;
    double wfact_horizontal;
    double wfact_vertical;
};

struct cwc_cursor {
    struct wlr_seat *seat;
    struct wlr_cursor *wlr_cursor;
    struct wlr_xcursor_manager *xcursor_mgr;
    struct hyprcursor_manager_t *hyprcursor_mgr;
    const char *current_name;

    // interactive
    enum cwc_cursor_state state;
    uint32_t resize_edges;
    double grab_x, grab_y;
    union {
        struct wlr_box grab_float;
        struct bsp_grab grab_bsp;
    };
    struct cwc_toplevel *grabbed_toplevel;
    const char *name_before_interactive;
    struct wlr_scene_rect *snap_overlay;

    // resize scheduling
    uint64_t last_resize_time_msec;
    struct wlr_box pending_box;

    // hyprcursor
    struct hyprcursor_cursor_style_info info;
    hyprcursor_cursor_image_data **images;
    int images_count;
    int frame_index; // point to animation frame in cursor_buffers
    struct wl_array cursor_buffers; // struct hyprcursor_buffer *
    struct wl_event_source *animation_timer;
    float scale;

    // states
    struct wlr_pointer_constraint_v1 *active_constraint;
    bool dont_emit_signal;
    bool grab;
    bool send_events;
    struct cwc_output *last_output;

    // cursor inactive timeout
    bool hidden;
    const char *name_before_hidden;
    struct wl_event_source *inactive_timer;
    struct wlr_surface *client_surface;
    int hotspot_x, hotspot_y;
    struct wl_listener client_side_surface_destroy_l;

    struct wl_listener cursor_motion_l;
    struct wl_listener cursor_motion_abs_l;
    struct wl_listener cursor_axis_l;
    struct wl_listener cursor_button_l;
    struct wl_listener cursor_frame_l;

    struct wl_listener swipe_begin_l;
    struct wl_listener swipe_update_l;
    struct wl_listener swipe_end_l;

    struct wl_listener pinch_begin_l;
    struct wl_listener pinch_update_l;
    struct wl_listener pinch_end_l;

    struct wl_listener hold_begin_l;
    struct wl_listener hold_end_l;

    struct wl_listener config_commit_l;
    struct wl_listener destroy_l;
};

void process_cursor_motion(struct cwc_cursor *cursor,
                           uint32_t time_msec,
                           struct wlr_input_device *device,
                           double dx,
                           double dy,
                           double dx_unaccel,
                           double dy_unaccel);

struct cwc_cursor *cwc_cursor_create(struct wlr_seat *seat);

void cwc_cursor_destroy(struct cwc_cursor *cursor);

/* name should follow shape in cursor shape protocol */
void cwc_cursor_set_image_by_name(struct cwc_cursor *cursor, const char *name);

void cwc_cursor_set_surface(struct cwc_cursor *cursor,
                            struct wlr_surface *surface,
                            int32_t hotspot_x,
                            int32_t hotspot_y);

void cwc_cursor_hide_cursor(struct cwc_cursor *cursor);

void cwc_cursor_update_scale(struct cwc_cursor *cursor);

/* change style (mainly size)
 *
 * return true if success
 */
bool cwc_cursor_hyprcursor_change_style(
    struct cwc_cursor *cursor, struct hyprcursor_cursor_style_info info);

struct cwc_pointer_constraint {
    struct wlr_pointer_constraint_v1 *constraint;
    struct cwc_cursor *cursor;

    struct wl_listener destroy_l;
};

/* passing NULL will try to find toplevel below the cursor */
void start_interactive_move(struct cwc_toplevel *toplevel);
void start_interactive_resize(struct cwc_toplevel *toplevel, uint32_t edges);

/* no op when is not from interactive */
void stop_interactive(struct cwc_cursor *cursor);

/* signal data struct */
struct cwc_pointer_move_event {
    struct cwc_cursor *cursor;
    double dx;
    double dy;
    double dx_unaccel;
    double dy_unaccel;
};

struct cwc_pointer_button_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_button_event *event;
};

struct cwc_pointer_axis_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_axis_event *event;
};

struct cwc_pointer_swipe_begin_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_swipe_begin_event *event;
};

struct cwc_pointer_swipe_update_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_swipe_update_event *event;
};

struct cwc_pointer_swipe_end_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_swipe_end_event *event;
};

struct cwc_pointer_pinch_begin_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_pinch_begin_event *event;
};

struct cwc_pointer_pinch_update_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_pinch_update_event *event;
};

struct cwc_pointer_pinch_end_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_pinch_end_event *event;
};

struct cwc_pointer_hold_begin_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_hold_begin_event *event;
};

struct cwc_pointer_hold_end_event {
    struct cwc_cursor *cursor;
    struct wlr_pointer_hold_end_event *event;
};

#endif // !_CWC_INPUT_CURSOR_H
