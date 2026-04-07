/* util-vec.c - dynamic array data structure
 *
 * Copyright (C) 2026 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cwc/util.h"

struct cwc_vec *cwc_vec_create(int elem_sizeof, int reserved)
{
    struct cwc_vec *vec = calloc(1, sizeof(*vec));
    if (!vec)
        return NULL;

    switch (elem_sizeof) {
    case sizeof(int8_t):
    case sizeof(int16_t):
    case sizeof(int32_t):
    case sizeof(int64_t):
        break;
    default:
        fprintf(stderr, "non standard size\n");
        abort();
    }

    if (reserved < 4)
        reserved = 4;

    vec->elem_sizeof = elem_sizeof;
    vec->alloc       = reserved;
    vec->data        = calloc(1, reserved * elem_sizeof);

    return vec;
}

void cwc_vec_destroy(struct cwc_vec *vec)
{
    free(vec->data);
    free(vec);
}

static void assign_idx(struct cwc_vec *vec, size_t idx, uint64_t packed_data)
{
    switch (vec->elem_sizeof) {
    case sizeof(int8_t):
        ((int8_t *)(vec->data))[idx] = packed_data;
        break;
    case sizeof(int16_t):
        ((int16_t *)(vec->data))[idx] = packed_data;
        break;
    case sizeof(int32_t):
        ((int32_t *)(vec->data))[idx] = packed_data;
        break;
    case sizeof(int64_t):
        ((int64_t *)(vec->data))[idx] = packed_data;
        break;
    default:
        break;
    }
}

static bool check_expand(struct cwc_vec *vec)
{
    if (++vec->count > vec->alloc) {
        size_t alloc      = vec->alloc * 2;
        void *reallocated = realloc(vec->data, alloc * vec->elem_sizeof);
        if (!reallocated)
            return false;

        vec->alloc = alloc;
        vec->data  = reallocated;
    }

    return true;
}

bool cwc_vec_push(struct cwc_vec *vec, void *data)
{
    if (!check_expand(vec))
        return false;

    uint64_t a           = 0xffffffffffffffff >> (64 - vec->elem_sizeof * 8);
    uint64_t packed_data = (uint64_t)data & a;
    size_t idx           = vec->count - 1;

    assign_idx(vec, idx, packed_data);

    return true;
}

bool cwc_vec_push_at(struct cwc_vec *vec, size_t idx, void *data)
{
    if (!check_expand(vec))
        return false;

    char *cdata     = vec->data;
    char *src       = cdata + (vec->elem_sizeof * idx);
    char *dst       = src + (vec->elem_sizeof);
    int byte_copied = vec->elem_sizeof * (vec->count - idx - 1);
    memmove(dst, src, byte_copied);

    uint64_t a           = 0xffffffffffffffff >> (64 - vec->elem_sizeof * 8);
    uint64_t packed_data = (uint64_t)data & a;

    assign_idx(vec, idx, packed_data);
}

static void check_shrink(struct cwc_vec *vec)
{
    if (((double)vec->count / vec->alloc) > 0.4)
        return;

    if (vec->count / 2 < 4)
        return;

    size_t alloc      = vec->alloc / 2;
    void *reallocated = realloc(vec->data, alloc * vec->elem_sizeof);
    if (!reallocated)
        return;

    vec->alloc = alloc;
    vec->data  = reallocated;
}

void cwc_vec_pop(struct cwc_vec *vec)
{
    if (vec->count == 0)
        return;

    --vec->count;

    check_shrink(vec);
}

void cwc_vec_pop_at(struct cwc_vec *vec, size_t idx)
{
#ifndef NDEBUG
    if (idx >= vec->count) {
        fprintf(stderr,
                "index out of bounds: trying to access index %ld with vector "
                "size %ld\n",
                idx, vec->count);
        abort();
    }
#endif

    if (vec->count == 0)
        return;

    --vec->count;

    char *cdata     = vec->data;
    char *src       = cdata + (vec->elem_sizeof * (idx + 1));
    char *dst       = src - (vec->elem_sizeof);
    int byte_copied = vec->elem_sizeof * (vec->count - idx);
    memmove(dst, src, byte_copied);

    check_shrink(vec);
}

static void *access_idx(struct cwc_vec *vec, size_t idx)
{
    switch (vec->elem_sizeof) {
    case sizeof(int8_t):
        return (void *)(intptr_t)(((int8_t *)(vec->data))[idx]);
    case sizeof(int16_t):
        return (void *)(intptr_t)(((int16_t *)(vec->data))[idx]);
    case sizeof(int32_t):
        return (void *)(intptr_t)(((int32_t *)(vec->data))[idx]);
    case sizeof(int64_t):
        return (void *)(intptr_t)(((int64_t *)(vec->data))[idx]);
    default:
        return NULL;
    }
}

void *cwc_vec_at(struct cwc_vec *vec, size_t idx)
{
#ifndef NDEBUG
    if (idx >= vec->count) {
        fprintf(stderr,
                "index out of bounds: trying to access index %ld with vector "
                "size %ld\n",
                idx, vec->count);
        abort();
    }
#endif

    return access_idx(vec, idx);
}

int cwc_vec_find(struct cwc_vec *vec, void *value)
{
    for (size_t i = 0; i < vec->count; i++) {
        switch (vec->elem_sizeof) {
        case sizeof(int8_t):
            if (((int8_t *)(vec->data))[i] == (int8_t)(intptr_t)value)
                return i;
            break;
        case sizeof(int16_t):
            if (((int16_t *)(vec->data))[i] == (int16_t)(intptr_t)value)
                return i;
            break;
        case sizeof(int32_t):
            if (((int32_t *)(vec->data))[i] == (int32_t)(intptr_t)value)
                return i;
            break;
        case sizeof(int64_t):
            if (((int64_t *)(vec->data))[i] == (int64_t)(intptr_t)value)
                return i;
            break;
        default:
            break;
        }
    }

    return -1;
}
