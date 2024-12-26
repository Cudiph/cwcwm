/* general types */

#ifndef _CWC_TYPES_H
#define _CWC_TYPES_H

#include <stdint.h>
#include <wayland-util.h>

// hard limit of minimum toplevel width
#define MIN_WIDTH 20

// max 30 workspace/tags because the tag is using uint32_t
#define MAX_WORKSPACE 30
typedef uint32_t tag_bitfield_t;
typedef uint32_t container_state_bitfield_t;

/* Documented data pointer
 *
 * wlr_surface->data == wlr_scene_tree
 *
 * node.data == cwc_data_interface_t
 *
 * wlr_xwayland_surface.data == toplevel
 *
 * wlr_xdg_surface.data == (toplevel || popup)
 */

/* data type should be the first entry so that it act like union.
 *
 * The use case is for object which are stored in data that the type
 * cannot be sure like data in the `scene_node.data`. For data that is already
 * obvious such as in `wlr_seat.data`, it already obvious enough that it point
 * to cwc_seat hence it's unnecessary to put the data type.
 */
enum cwc_data_type {
    CWC_DATA_EMPTY = 0,
    CWC_DATA_RESERVED,

    DATA_TYPE_OUTPUT,

    DATA_TYPE_XDG_SHELL,
    DATA_TYPE_XWAYLAND,
    DATA_TYPE_LAYER_SHELL,
    DATA_TYPE_POPUP,

    DATA_TYPE_BORDER,
    DATA_TYPE_CONTAINER,
};

/* common interface, better use this instead of the object to check the type */
typedef struct cwc_data_interface {
    enum cwc_data_type type;
} cwc_data_interface_t;

static inline struct cwc_container *
cwc_container_try_from_data_descriptor(void *data)
{
    cwc_data_interface_t *cwc_data = data;
    if (cwc_data->type == DATA_TYPE_CONTAINER)
        return data;

    return NULL;
}

/* layout stuff */
enum cwc_layout_mode {
    CWC_LAYOUT_FLOATING,
    CWC_LAYOUT_MASTER,
    CWC_LAYOUT_BSP,
    CWC_LAYOUT_LENGTH,
};

struct bsp_root_entry {
    struct bsp_node *root; // NULL indicate empty bsp
    struct cwc_container *last_focused;
};

struct master_state {
    int master_count;
    int column_count;
    double mwfact;
    struct layout_interface *current_layout;
};

/* contains information of a single view only tag or a traditional workspace */
struct cwc_tag_info {
    char *label;
    enum cwc_layout_mode layout_mode;

    int useless_gaps;
    struct bsp_root_entry bsp_root_entry;
    struct master_state master_state;
};

#endif // !_CWC_TYPES_H
