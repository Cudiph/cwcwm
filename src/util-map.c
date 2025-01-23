/* util-map.c - hashmap data structure
 *
 * Copyright (C) 2024 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
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
#include <xxhash.h>

#include "cwc/util.h"

static inline void __recompute_limit(struct cwc_hhmap *map)
{
    map->expand_at_size_exceed = map->alloc * 3 / 4;
    map->shrink_at_size_behind = map->alloc / 5;
#ifdef HHMAP_AUTOMATIC_REHASH
    map->rehash_at_remove_counter_exceed = map->alloc;
    map->remove_counter                  = 0;
#endif /* ifdef HHMAP_AUTOMATIC_REHASH */
    if (map->shrink_at_size_behind < 8)
        map->shrink_at_size_behind = 0;
}

struct cwc_hhmap *cwc_hhmap_create(int prealloc_size)
{
    struct cwc_hhmap *m = malloc(sizeof(*m));
    if (!m)
        return NULL;

    uint64_t init_alloc = 8;
    while (init_alloc < prealloc_size)
        init_alloc <<= 1;

    m->size  = 0;
    m->alloc = init_alloc;
    m->table = calloc(init_alloc, sizeof(*m->table));
    __recompute_limit(m);
    // disable shrink limit to respect initial_size
    m->shrink_at_size_behind = 0;

    return m;
}

void cwc_hhmap_destroy(struct cwc_hhmap *map)
{
    free(map->table);
    free(map);
}

/* return pointer to hash_entry that found, return NULL otherwise */
struct hhash_entry *cwc_hhmap_lookup(struct cwc_hhmap *map,
                                     uint64_t hash,
                                     struct hhash_entry **prev)
{
    uint64_t alloc            = map->alloc;
    struct hhash_entry *table = map->table;
    uint64_t idx              = hash % alloc;
    struct hhash_entry *root  = &table[idx];

    // loop until the end of linked list
    *prev                       = root;
    struct hhash_entry *current = root;
    do {
        if (hash == current->hash)
            break;

        *prev   = current;
        current = current->next;

    } while (current != NULL);

    return current;
}

static inline void __cwc_hhmap_rehash_expand(struct cwc_hhmap *map);

static inline void
__cwc_hhmap_insert(struct cwc_hhmap *map, uint64_t hash, void *data)
{
    struct hhash_entry *prev;
    struct hhash_entry *current = cwc_hhmap_lookup(map, hash, &prev);

    if (current) {
        current->data = data;
        return;
    }

    // find empty block using linear probing
    struct hhash_entry *table = map->table;
    uint64_t alloc            = map->alloc;
    uint64_t idx              = prev - table;
    current                   = &table[idx];
    while (current->hash != 0) {
        idx += 1; // jump interval, must be prime number
        if (idx >= alloc)
            idx -= alloc;

        current = &table[idx];
    }

    current->hash = hash;
    current->data = data;
    map->size++;

    if (prev != current)
        prev->next = current;

    __cwc_hhmap_rehash_expand(map);
}

void __cwc_hhmap_rehash_to_size(struct cwc_hhmap *map, uint64_t new_size)
{
    uint64_t alloc                = map->alloc;
    struct hhash_entry *old_table = map->table;
    struct hhash_entry *new_table = calloc(new_size, sizeof(*new_table));
    if (new_table == NULL)
        return;

    map->table = new_table;
    map->size  = 0;
    map->alloc = new_size;
    __recompute_limit(map);

    for (uint64_t i = 0; i < alloc; i++) {
        struct hhash_entry *elem = &old_table[i];
        if (!elem->hash)
            continue;
        __cwc_hhmap_insert(map, elem->hash, elem->data);
    }

    free(old_table);
}

/* expand and rehash to half filled bucket (75% load to about 40%) */
static inline void __cwc_hhmap_rehash_expand(struct cwc_hhmap *map)
{
    if (map->expand_at_size_exceed >= map->size)
        return;

    __cwc_hhmap_rehash_to_size(map, map->alloc * 2);
}

/* Rehash and shrink to half (20% load to 50% load)
 *
 * Want to use realloc by moving upper address to lower address originally but
 * the singly linked list point to random place makes it hard.
 */
static inline void __cwc_hhmap_rehash_shrink(struct cwc_hhmap *map)
{
    if (map->size >= map->shrink_at_size_behind)
        return;

    __cwc_hhmap_rehash_to_size(map, map->alloc / 2);
}

void cwc_hhmap_insert(struct cwc_hhmap *map, const char *key, void *data)
{
    uint64_t xxhash = XXH3_64bits(key, strlen(key));

    __cwc_hhmap_insert(map, xxhash, data);
}

void cwc_hhmap_ninsert(struct cwc_hhmap *map,
                       void *key,
                       int key_len,
                       void *data)
{
    uint64_t xxhash = XXH3_64bits(key, key_len);
    __cwc_hhmap_insert(map, xxhash, data);
}

static inline void *__cwc_hhmap_get(struct cwc_hhmap *map, uint64_t hash)
{
    struct hhash_entry *prev;
    struct hhash_entry *result = cwc_hhmap_lookup(map, hash, &prev);

    if (result)
        return result->data;

    return NULL;
}

void *cwc_hhmap_get(struct cwc_hhmap *map, const char *key)
{
    uint64_t xxhash = XXH3_64bits(key, strlen(key));
    return __cwc_hhmap_get(map, xxhash);
}

void *cwc_hhmap_nget(struct cwc_hhmap *map, const void *key, int key_len)
{
    uint64_t xxhash = XXH3_64bits(key, key_len);
    return __cwc_hhmap_get(map, xxhash);
}

struct hhash_entry *__cwc_hhmap_get_entry(struct cwc_hhmap *map, uint64_t hash)
{
    struct hhash_entry *prev;
    struct hhash_entry *result = cwc_hhmap_lookup(map, hash, &prev);

    return result;
}

struct hhash_entry *cwc_hhmap_get_entry(struct cwc_hhmap *map, const char *key)
{
    uint64_t xxhash = XXH3_64bits(key, strlen(key));
    return __cwc_hhmap_get_entry(map, xxhash);
}

struct hhash_entry *
cwc_hhmap_nget_entry(struct cwc_hhmap *map, const void *key, int key_len)
{
    uint64_t xxhash = XXH3_64bits(key, key_len);
    return __cwc_hhmap_get_entry(map, xxhash);
}

void __cwc_hhmap_remove(struct cwc_hhmap *map, uint64_t hash)
{
    struct hhash_entry *prev;
    struct hhash_entry *result = cwc_hhmap_lookup(map, hash, &prev);

    if (result == NULL)
        return;

    // if its the last node then clear
    if (result->next == NULL) {
        if (prev)
            prev->next = NULL;
        *result = (struct hhash_entry){0};
    } else { // keep the link for other to use
        result->hash = 0;
        result->data = NULL;
    }

    map->size--;

#ifdef HHMAP_AUTOMATIC_REHASH
    map->remove_counter++;
    if (map->remove_counter >= map->rehash_at_remove_counter_exceed)
        __cwc_hhmap_rehash_to_size(map, map->alloc);
#else
    __cwc_hhmap_rehash_shrink(map);
#endif /* ifdef HHMAP_AUTOMATIC_REHASH */
}

void cwc_hhmap_remove(struct cwc_hhmap *map, const char *key)
{
    uint64_t xxhash = XXH3_64bits(key, strlen(key));
    __cwc_hhmap_remove(map, xxhash);
}

void cwc_hhmap_nremove(struct cwc_hhmap *map, const void *key, int key_len)
{

    uint64_t xxhash = XXH3_64bits(key, key_len);
    __cwc_hhmap_remove(map, xxhash);
}
