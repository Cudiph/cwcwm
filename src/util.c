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
#include <wayland-util.h>

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
