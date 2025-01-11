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
};

struct hyprcursor_buffer {
    struct wlr_buffer base;
    cairo_surface_t *surface;
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
    struct wlr_box grab_geobox;
    struct cwc_toplevel *grabbed_toplevel;
    const char *name_before_interactive;

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

    struct wlr_pointer_constraint_v1 *active_constraint;
    bool dont_emit_signal;

    struct wl_listener seat_request_cursor_l;
    struct wl_listener seat_pointer_focus_change_l;

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

/* change style (mainly size)
 *
 * return true if success
 */
bool cwc_cursor_hyprcursor_change_style(
    struct cwc_cursor *cursor, struct hyprcursor_cursor_style_info info);

struct cwc_cursor_shape_manager {
    struct wlr_cursor_shape_manager_v1 *manager;
    struct cwc_server *server;

    struct wl_listener request_set_shape_l;
    struct wl_listener destroy_l;
};

struct cwc_pointer_constraint {
    struct wlr_pointer_constraint_v1 *constraint;
    struct cwc_cursor *cursor;

    struct wl_listener destroy_l;
};

/* passing NULL will try to find toplevel below the cursor */
void start_interactive_move(struct cwc_toplevel *toplevel);
void start_interactive_resize(struct cwc_toplevel *toplevel, uint32_t edges);

/* no op when is not from interactive */
void stop_interactive();

#endif // !_CWC_INPUT_CURSOR_H
