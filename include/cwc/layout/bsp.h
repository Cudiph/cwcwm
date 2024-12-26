#ifndef _CWC_BSP_H
#define _CWC_BSP_H

#include <wlr/types/wlr_scene.h>

struct cwc_output;
struct cwc_container;

enum bsp_node_type {
    BSP_NODE_INTERNAL,
    BSP_NODE_LEAF,
};

enum bsp_split_type {
    BSP_SPLIT_AUTO, // leaf only
    BSP_SPLIT_VERTICAL,
    BSP_SPLIT_HORIZONTAL,
};

struct bsp_node {
    enum bsp_node_type type;

    struct cwc_container *container; // leaf

    struct bsp_node *parent;       // NULL parent indicates root
    struct bsp_node *left, *right; // leaf doesn't have child

    bool enabled;

    enum bsp_split_type split_type;

    /* factor of the left leaf node */
    double left_wfact;

    /* for internal node, node origin in local layout coordinate */
    int x, y;

    /* for internal node, this represent width and height of area occupied by
     * this node.
     */
    int width, height;
};

/* insert toplevel to the bsp tree, toplevle must not already in a tree */
void bsp_insert_container(struct cwc_container *new, int workspace);

/* remove toplevel from the bsp tree */
void bsp_remove_container(struct cwc_container *container);

void bsp_toggle_split(struct bsp_node *node);

void bsp_update_node(struct bsp_node *parent);

void bsp_node_destroy(struct bsp_node *node);

void bsp_node_enable(struct bsp_node *node);
void bsp_node_disable(struct bsp_node *node);

struct bsp_node *bsp_get_root(struct bsp_node *node);

/* update the bsp in O(n) complexity */
void bsp_update_root(struct cwc_output *output, int workspace);

void bsp_last_focused_update(struct cwc_container *container);

struct bsp_root_entry *
bsp_entry_init(struct cwc_output *output, int workspace, struct bsp_node *root);

/* return NULL if not initialized */
struct bsp_root_entry *bsp_entry_get(struct cwc_output *output, int workspace);

/* deinitialize */
void bsp_entry_fini(struct cwc_output *output, int workspace);

#endif // !_CWC_BSP_H
