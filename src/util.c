/* util.c - utility functions and data structure
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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "cwc/util.h"

bool wl_list_length_at_least(struct wl_list *list, int more_than_or_equal_to)
{
    int count         = 0;
    struct wl_list *e = list->next;
    while (e != list) {
        e = e->next;
        if (++count >= more_than_or_equal_to)
            return true;
    }

    return false;
}

void wl_list_swap(struct wl_list *x, struct wl_list *y)
{
    if (x == y)
        return;

    if (x->next == y) {
        wl_list_remove(x);
        wl_list_insert(y, x);
        return;
    }

    if (x->prev == y) {
        wl_list_remove(y);
        wl_list_insert(x, y);
        return;
    }

    struct wl_list *x_prev = x->prev;
    wl_list_remove(x);
    wl_list_insert(y, x);
    wl_list_remove(y);
    wl_list_insert(x_prev, y);
}

void wl_list_reattach(struct wl_list *older_sibling, struct wl_list *elm)
{
    wl_list_remove(elm);
    wl_list_insert(older_sibling, elm);
}

bool _cwc_assert(bool condition, const char *format, ...)
{
    if (condition)
        return true;

    va_list args;
    va_start(args, format);
    _wlr_vlog(WLR_ERROR, format, args);
    vfprintf(stderr, format, args);
    va_end(args);

#ifndef NDEBUG
    raise(SIGABRT);
#endif

    return false;
}

void normalized_region_at(
    struct wlr_box *region, double x, double y, double *nx, double *ny)
{
    if (nx)
        *nx = (x - (double)region->x) / region->width;

    if (ny)
        *ny = (y - (double)region->y) / region->height;
}

double distance(int lx, int ly, int lx2, int ly2)
{
    int x_diff = abs(lx2 - lx);
    int y_diff = abs(ly2 - ly);

    return sqrt(pow(x_diff, 2) + pow(y_diff, 2));
}

bool is_direction_match(enum wlr_direction dir, int x, int y)
{
    cwc_assert(x || y, "both x and y cannot be zero");

    double angle = atan2(y, x) * (180 / M_PI);

    switch (dir) {
    case WLR_DIRECTION_UP:
        if (angle > -45 || angle < -135)
            return false;
        break;
    case WLR_DIRECTION_RIGHT:
        if (angle > -45 && angle < 45)
            break;
        else
            return false;
    case WLR_DIRECTION_DOWN:
        if (angle < 45 || angle > 135)
            return false;
        break;
    case WLR_DIRECTION_LEFT:
        if (angle > 135 || angle < -135)
            break;
        else
            return false;
    }

    return true;
}

uint32_t
get_snap_edges(struct wlr_box *output_box, int cx, int cy, int threshold)
{
    uint32_t edges              = 0;
    int output_right_edge_diff  = output_box->x + output_box->width - cx;
    int output_left_edge_diff   = cx - output_box->x;
    int output_bottom_edge_diff = output_box->y + output_box->height - cy;
    int output_top_edge_diff    = cy - output_box->y;

    if (output_right_edge_diff >= 0 && output_right_edge_diff < threshold) {
        edges |= WLR_EDGE_RIGHT;
    } else if (output_left_edge_diff >= 0
               && output_left_edge_diff < threshold) {
        edges |= WLR_EDGE_LEFT;
    }

    if (output_bottom_edge_diff >= 0 && output_bottom_edge_diff < threshold) {
        edges |= WLR_EDGE_BOTTOM;
    } else if (output_top_edge_diff >= 0 && output_top_edge_diff < threshold) {
        edges |= WLR_EDGE_TOP;
    }

    return edges;
}

bool get_cwc_datadir(char *dst, int buff_size)
{
    char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (!xdg_data_dirs || strlen(xdg_data_dirs) == 0) {
        xdg_data_dirs = "/usr/local/share:/usr/share";
        setenv("XDG_DATA_DIRS", xdg_data_dirs, true);
    }

    xdg_data_dirs  = strdup(xdg_data_dirs);
    char *save_ptr = xdg_data_dirs;
    char *data_dir;
    while ((data_dir = strtok_r(save_ptr, ":", &save_ptr))) {
        strncpy(dst, data_dir, buff_size - 1);
        strncat(dst, "/cwc", buff_size - 1);
        int ok = access(dst, R_OK) == 0;
        if (!ok)
            continue;

        free(xdg_data_dirs);
        return true;
    }

    free(xdg_data_dirs);
    strncpy(dst, "/usr/share/cwc", buff_size);
    return false;
}
