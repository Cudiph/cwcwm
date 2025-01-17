#ifndef _CWC_UTIL_H
#define _CWC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <wayland-util.h>
#include <wlr/util/log.h>

//====================== HASHMAP ======================

struct hhash_entry {
    /* next colliding element in singly linked list */
    struct hhash_entry *next;
    /* hash result, 0 if entry is unoccupied */
    uint64_t hash;
    /* user data */
    void *data;
};

/* hhmap or hybrid hash map designed for read operation at big table although
 * the data integrity is questionable since the hash collision is not handled
 * which mean it contradict the purpose. Currently the hash
 * collision that produced from xxhash is not yet found with 20 million hash
 * entry with number as a string for the key.
 *
 * Frequent insert+remove will cause chaos
 * in the linked list logic and may cause infinite loop if it not rehashed. The
 * automatic rehash is available to overcome this with reference count overhead.
 *
 * Linear probing is used for finding empty block, and chaining is used for
 * lookup operation using singly linked list that point to other element in
 * the table, so in theory it got better cache locality and faster lookup but
 * hey that's just theory, a ~~game~~ programming theory!
 *
 * The performance is twice as slow in small table but become competitive the
 * bigger the table is.
 */
struct cwc_hhmap {
    /* filled entry count */
    uint64_t size;

    /* allocated length (in size, not bytes) */
    uint64_t alloc;

    /* expensive div/mul cache, must changed after rehashing */
    uint64_t expand_at_size_exceed; // alloc * 3/4
    uint64_t shrink_at_size_behind; // alloc / 5

#ifdef HHMAP_AUTOMATIC_REHASH
    uint32_t rehash_at_remove_counter_exceed;
    uint32_t remove_counter;
#endif // HHMAP_AUTOMATIC_REHASH

    /* pointer to the array of hash entry */
    struct hhash_entry *table;
};

/* if prealloc_size < 8 it will still 8 entries allocated */
struct cwc_hhmap *cwc_hhmap_create(int prealloc_size);

void cwc_hhmap_destroy(struct cwc_hhmap *map);

/* insert element */
void cwc_hhmap_insert(struct cwc_hhmap *map, const char *key, void *data);
void cwc_hhmap_ninsert(struct cwc_hhmap *map,
                       void *key,
                       int key_len,
                       void *data);

/* return the saved inserted data */
void *cwc_hhmap_get(struct cwc_hhmap *map, const char *key);
void *cwc_hhmap_nget(struct cwc_hhmap *map, const void *key, int key_len);

/* return the hash entry instead of data */
struct hhash_entry *cwc_hhmap_get_entry(struct cwc_hhmap *map, const char *key);
struct hhash_entry *
cwc_hhmap_nget_entry(struct cwc_hhmap *map, const void *key, int key_len);

/* remove */
void cwc_hhmap_remove(struct cwc_hhmap *map, const char *key);
void cwc_hhmap_nremove(struct cwc_hhmap *map, const void *key, int key_len);

/* rehash(map->alloc) will reset the linked list */
void __cwc_hhmap_rehash_to_size(struct cwc_hhmap *map, uint64_t new_size);

bool wl_list_length_at_least(struct wl_list *list, int more_than_or_equal_to);

void wl_list_swap(struct wl_list *x, struct wl_list *y);

void wl_list_reattach(struct wl_list *older_sibling, struct wl_list *elm);

struct wlr_box;
void normalized_region_at(
    struct wlr_box *region, double x, double y, double *nx, double *ny);

enum cwc_log_importance {
    CWC_SILENT              = WLR_SILENT,
    CWC_ERROR               = WLR_ERROR,
    CWC_INFO                = WLR_INFO,
    CWC_DEBUG               = WLR_DEBUG,
    CWC_LOG_IMPORTANCE_LAST = WLR_LOG_IMPORTANCE_LAST,
};

#define cwc_log(verb, fmt, ...)                                   \
    _wlr_log((enum wlr_log_importance)verb, "[cwc] [%s:%d] " fmt, \
             _WLR_FILENAME, __LINE__, ##__VA_ARGS__)

bool _cwc_assert(bool condition, const char *format, ...);

#define cwc_assert(cond, fmt, ...)                                          \
    _cwc_assert(cond, "[%s:%d] [cwc_assertion_failed] " fmt, _WLR_FILENAME, \
                __LINE__, ##__VA_ARGS__)

// rust macros
#define unreachable_()   cwc_assert(0, "unreachable code reached!")
#define todo_(desc, ...) cwc_assert(0, desc, ##__VA_ARGS__)

// common macros
#define MIN(A, B)            ((A) < (B) ? (A) : (B))
#define MAX(A, B)            ((A) > (B) ? (A) : (B))
#define CLAMP(val, min, max) MIN(MAX(val, min), max)
#define LENGTH(X)            (sizeof X / sizeof X[0])
#define LISTEN_CREATE(E, H)                           \
    do {                                              \
        struct wl_listener *_l = malloc(sizeof(*_l)); \
        _l->notify             = (H);                 \
        wl_signal_add((E), _l);                       \
    } while (0)

#define LISTEN_DESTROY(L)           \
    do {                            \
        wl_list_remove(&(L)->link); \
        free(L);                    \
    } while (0)

static inline uint64_t timespec_to_msec(struct timespec *t)
{
    return t->tv_sec * 1000 + t->tv_nsec / 1e6;
}

#endif // !_CWC_UTIL_H
