/* bsp.c - binary space partition layout operation
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

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/bsp.h"
#include "cwc/util.h"

enum Position { LEFT, RIGHT, ROOT };

static inline struct bsp_node *
bsp_node_get_sibling(struct bsp_node *parent_node, struct bsp_node *me)
{
    return parent_node->left == me ? parent_node->right : parent_node->left;
}

void bsp_node_destroy(struct bsp_node *node)
{
    if (!node)
        return;

    if (node->left)
        bsp_node_destroy(node->left);

    if (node->right)
        bsp_node_destroy(node->right);

    if (node->container)
        node->container->bsp_node = NULL;

    free(node);
}

static inline void bsp_node_reparent(struct bsp_node *parent,
                                     struct bsp_node *node,
                                     enum Position pos)
{
    if (node->parent) {
        if (node->parent->left == node)
            node->parent->left = NULL;
        else if (node->parent->right == node)
            node->parent->right = NULL;
    }

    switch (pos) {
    case LEFT:
        parent->left = node;
        break;
    case RIGHT:
        parent->right = node;
    case ROOT:
        break;
    }

    node->parent = parent;
}

struct bsp_node *bsp_get_root(struct bsp_node *node)
{
    if (!node->parent)
        return node;

    return bsp_get_root(node->parent);
}

static inline void bsp_node_set_position(struct bsp_node *node, int x, int y)
{
    node->x = x;
    node->y = y;
}

static inline void bsp_node_set_size(struct bsp_node *node, int w, int h)
{
    node->width  = w;
    node->height = h;
}

static inline void bsp_node_leaf_configure(
    struct bsp_node *node, int x, int y, int width, int height)
{
    struct cwc_container *container = node->container;

    if (!cwc_container_is_configure_allowed(container))
        return;

    // set size first so that the floating box not save the new x y
    if (!cwc_container_is_floating(container)) {
        cwc_container_set_size(container, width, height);
        cwc_container_set_position_gap(container, x, y);
    }

    bsp_node_set_position(node, x, y);
    bsp_node_set_size(node, width, height);
}

static struct bsp_node *_bsp_node_leaf_get(struct bsp_node *node, bool to_left)
{
    if (node->type == BSP_NODE_LEAF)
        return node;

    if (to_left)
        return _bsp_node_leaf_get(node->left, true);

    return _bsp_node_leaf_get(node->right, false);
}

static inline struct bsp_node *find_closes_leaf_sibling(struct bsp_node *me)
{
    struct bsp_node *parent = me->parent;
    if (parent->right == me)
        return _bsp_node_leaf_get(parent->left, false);

    return _bsp_node_leaf_get(parent->right, true);
}

void bsp_update_node(struct bsp_node *parent)
{
    struct bsp_node *left  = parent->left;
    struct bsp_node *right = parent->right;

    left->x = parent->x;
    left->y = parent->y;

    // calculate width and height for left and right according to left width
    // factor
    switch (parent->split_type) {
    case BSP_SPLIT_VERTICAL:
        left->width  = parent->width * parent->left_wfact;
        left->height = parent->height;

        right->width  = parent->width - left->width;
        right->height = parent->height;
        right->x      = left->x + left->width;
        right->y      = left->y;
        break;
    case BSP_SPLIT_HORIZONTAL:
        left->width  = parent->width;
        left->height = parent->height * parent->left_wfact;

        right->width  = parent->width;
        right->height = parent->height - left->height;
        right->x      = left->x;
        right->y      = left->y + left->height;
        break;
    case BSP_SPLIT_AUTO:
        unreachable_();
        break;
    }

    if (!right->enabled) {
        left->width  = parent->width;
        left->height = parent->height;
    }

    if (left->enabled) {
        // update position/size for left
        if (left->type == BSP_NODE_LEAF) {
            bsp_node_leaf_configure(left, parent->x, parent->y, left->width,
                                    left->height);
        } else {
            bsp_node_set_position(left, parent->x, parent->y);
            bsp_update_node(left);
        }
    } else {
        right->x      = parent->x;
        right->y      = parent->y;
        right->width  = parent->width;
        right->height = parent->height;
    }

    if (right->enabled) {
        // update position/size for right
        if (right->type == BSP_NODE_LEAF) {
            bsp_node_leaf_configure(right, right->x, right->y, right->width,
                                    right->height);
        } else {
            bsp_update_node(right);
        }
    }
}

void bsp_update_root(struct cwc_output *output, int workspace)
{
    struct bsp_root_entry *entry = bsp_entry_get(output, workspace);
    if (!entry)
        return;

    struct bsp_node *root      = entry->root;
    struct wlr_box usable_area = output->usable_area;

    if (root->type == BSP_NODE_LEAF) {
        bsp_node_leaf_configure(root, usable_area.x, usable_area.y,
                                usable_area.width, usable_area.height);
        return;
    }

    bsp_node_set_size(root, usable_area.width, usable_area.height);
    bsp_node_set_position(root, usable_area.x, usable_area.y);

    bsp_update_node(root);
}

/* enable all the node until root */
static struct bsp_node *_bsp_node_enable(struct bsp_node *node)
{
    node->enabled = true;

    if (!node->parent)
        return node;

    return _bsp_node_enable(node->parent);
}

void bsp_node_enable(struct bsp_node *node)
{
    struct bsp_node *root = _bsp_node_enable(node);

    if (root->type == BSP_NODE_INTERNAL)
        bsp_update_node(root);
    else
        bsp_update_root(root->container->output, root->container->workspace);
}

/* recursively disabled node if no one in the child node is enabled */
static struct bsp_node *_bsp_node_disable(struct bsp_node *node)
{
    node->enabled = false;

    struct bsp_node *parent = node->parent;
    if (!parent)
        return node;

    // if both left and right is disabled then also disabled parent
    if (!parent->left->enabled && !parent->right->enabled)
        return _bsp_node_disable(parent);

    return node;
}

void bsp_node_disable(struct bsp_node *node)
{
    struct bsp_node *last_updated = _bsp_node_disable(node);

    if (last_updated->type == BSP_NODE_INTERNAL && last_updated->parent)
        bsp_update_node(last_updated->parent);
    else if (last_updated->type == BSP_NODE_LEAF)
        bsp_update_root(last_updated->container->output,
                        last_updated->container->workspace);
}

void bsp_last_focused_update(struct cwc_container *container)
{
    struct bsp_root_entry *entry =
        bsp_entry_get(container->output, container->workspace);

    if (!entry)
        return;

    entry->last_focused = container;
}

/* automatically freed when wlr_node destoryed */
static struct bsp_node *bsp_node_internal_create(struct bsp_node *parent,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 enum bsp_split_type split,
                                                 enum Position pos)
{
    struct bsp_node *node_data = calloc(1, sizeof(*node_data));
    node_data->type            = BSP_NODE_INTERNAL;
    node_data->enabled         = true;
    node_data->split_type      = split;

    bsp_node_reparent(parent, node_data, pos);
    bsp_node_set_position(node_data, x, y);
    bsp_node_set_size(node_data, width, height);

    // set 1:1 ratio when first creating
    node_data->left_wfact = 0.5;

    return node_data;
}

/* automatically freed when wlr_node destoryed */
static struct bsp_node *bsp_node_leaf_create(struct bsp_node *parent,
                                             struct cwc_container *container,
                                             enum Position pos)
{
    struct bsp_node *node_data = calloc(1, sizeof(*node_data));
    node_data->type            = BSP_NODE_LEAF;
    node_data->container       = container;
    node_data->enabled         = true;
    node_data->parent          = parent;

    bsp_node_reparent(parent, node_data, pos);

    return node_data;
}

static void _bsp_insert_container(struct bsp_root_entry *root_entry,
                                  struct cwc_container *sibling,
                                  struct cwc_container *new)
{
    struct bsp_node *left = sibling->bsp_node;

    struct wlr_box old_geom = {
        .x      = left->x,
        .y      = left->y,
        .width  = left->width,
        .height = left->height,
    };

    enum bsp_split_type split = old_geom.width >= old_geom.height
                                    ? BSP_SPLIT_VERTICAL
                                    : BSP_SPLIT_HORIZONTAL;

    struct bsp_node *grandparent_node = left->parent;

    // parent internal node
    struct bsp_node *parent_node = NULL;

    // update grandparent left/right node to point to new parent
    if (grandparent_node) {
        if (grandparent_node->left == left)
            parent_node = bsp_node_internal_create(
                grandparent_node, old_geom.x, old_geom.y, old_geom.width,
                old_geom.height, split, LEFT);
        else if (grandparent_node->right == left)
            parent_node = bsp_node_internal_create(
                grandparent_node, old_geom.x, old_geom.y, old_geom.width,
                old_geom.height, split, RIGHT);
        else
            unreachable_();
    } else {
        parent_node = bsp_node_internal_create(grandparent_node, old_geom.x,
                                               old_geom.y, old_geom.width,
                                               old_geom.height, split, ROOT);
    }

    // set parent_tree to root if the sibling is the root
    if (left == root_entry->root) {
        parent_node->x          = new->output->usable_area.x;
        parent_node->y          = new->output->usable_area.y;
        parent_node->width      = new->output->usable_area.width;
        parent_node->height     = new->output->usable_area.height;
        parent_node->left_wfact = 0.5;
        root_entry->root        = parent_node;
    }

    struct bsp_node *right = new->bsp_node =
        bsp_node_leaf_create(parent_node, new, RIGHT);

    bsp_node_reparent(parent_node, left, LEFT);
    left->parent       = parent_node;
    parent_node->left  = left;
    parent_node->right = right;

    bsp_node_enable(right);
}

void bsp_insert_container(struct cwc_container *new, int workspace)
{
    struct cwc_output *output         = new->output;
    struct bsp_root_entry *root_entry = bsp_entry_get(output, workspace);

    cwc_assert(new->bsp_node == NULL, "toplevel already has bsp node");
    new->state &= ~CONTAINER_STATE_FLOATING;

    // init root entry if not yet
    if (!root_entry) {
        new->bsp_node = bsp_node_leaf_create(NULL, new, ROOT);
        root_entry    = bsp_entry_init(output, workspace, new->bsp_node);

        bsp_update_root(new->output, new->workspace);
        goto update_last_focused;
    }

    struct cwc_container *sibling = root_entry->last_focused;
    _bsp_insert_container(root_entry, sibling, new);

update_last_focused:
    root_entry->last_focused = new;
}

void bsp_remove_container(struct cwc_container *container, bool update)
{
    // TODO: container->workspace can be changed when at minimizing focusing.
    // the bsp_node I guess adding the field of where the real workspace is will
    // fix
    struct bsp_root_entry *bspentry =
        bsp_entry_get(container->output, container->workspace);
    struct bsp_node *grandparent_node = NULL;
    struct bsp_node *cont_node        = container->bsp_node;

    // if the toplevel is the root then destroy itself and the bsp_root_entry
    if (cont_node == bspentry->root) {
        bsp_entry_fini(container->output, container->workspace);
        return;
    }

    struct bsp_node *parent_node = cont_node->parent;
    struct bsp_node *sibling_node =
        bsp_node_get_sibling(parent_node, cont_node);

    if (bspentry->last_focused == container)
        bspentry->last_focused = find_closes_leaf_sibling(cont_node)->container;

    // if the parent is root change the sibling to root
    if (parent_node == bspentry->root) {
        bspentry->root = sibling_node;
        bsp_node_reparent(NULL, sibling_node, ROOT);
        goto destroy_node;
    }

    grandparent_node = parent_node->parent;

    // update grandparent child
    if (grandparent_node->left == parent_node) {
        bsp_node_reparent(grandparent_node, sibling_node, LEFT);
    } else if (grandparent_node->right == parent_node) {
        bsp_node_reparent(grandparent_node, sibling_node, RIGHT);
    } else {
        unreachable_();
    }

destroy_node:
    bsp_node_reparent(NULL, cont_node, ROOT);
    bsp_node_destroy(parent_node);
    bsp_node_destroy(cont_node);
    container->bsp_node = NULL;

    if (update) {
        if (grandparent_node)
            bsp_update_node(grandparent_node);
        else
            bsp_update_root(container->output, container->workspace);
    }
}

void bsp_toggle_split(struct bsp_node *node)
{
    if (!node)
        return;

    if (node->type == BSP_NODE_LEAF)
        node = node->parent;

    if (node->split_type == BSP_SPLIT_VERTICAL)
        node->split_type = BSP_SPLIT_HORIZONTAL;
    else
        node->split_type = BSP_SPLIT_VERTICAL;

    bsp_update_node(node);
}

struct bsp_root_entry *
bsp_entry_init(struct cwc_output *output, int workspace, struct bsp_node *root)
{
    struct bsp_root_entry *entry =
        &output->state->tag_info[workspace].bsp_root_entry;
    entry->root = root;
    return entry;
}

struct bsp_root_entry *bsp_entry_get(struct cwc_output *output, int workspace)
{
    struct bsp_root_entry *entry =
        &output->state->tag_info[workspace].bsp_root_entry;
    if (entry->root == NULL)
        return NULL;
    return entry;
}

void bsp_entry_fini(struct cwc_output *output, int workspace)
{
    struct bsp_root_entry *entry = bsp_entry_get(output, workspace);
    if (entry) {
        if (entry->root)
            bsp_node_destroy(entry->root);

        entry->root         = NULL;
        entry->last_focused = NULL;
    }
}
