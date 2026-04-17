#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "cwc/util.h"

static void functional_test()
{
    struct cwc_vec *vec = cwc_vec_create(sizeof(int), 10);

    cwc_vec_push(vec, (void *)INT_MAX);
    assert((int)(intptr_t)cwc_vec_at(vec, 0) == INT_MAX);
    cwc_vec_pop(vec);

    cwc_vec_push(vec, (void *)0x20);
    assert((int)(intptr_t)cwc_vec_at(vec, 0) == 0x20);

    cwc_vec_push(vec, (void *)INT_MIN);
    assert((int)(intptr_t)cwc_vec_at(vec, 1) == INT_MIN);

    cwc_vec_push(vec, (void *)0xffffffffffffffff);
    assert((int)(intptr_t)cwc_vec_at(vec, 2) == -1);

    cwc_vec_push_at(vec, 1, (void *)300);
    printf("%d\n", (int)(intptr_t)cwc_vec_at(vec, 1));
    assert((int)(intptr_t)cwc_vec_at(vec, 0) == 0x20);
    assert((int)(intptr_t)cwc_vec_at(vec, 1) == 300);
    assert((int)(intptr_t)cwc_vec_at(vec, 2) == INT_MIN);
    assert((int)(intptr_t)cwc_vec_at(vec, 3) == -1);
    assert(cwc_vec_find(vec, (void *)INT_MIN) == 2);
    assert(vec->count == 4);

    cwc_vec_pop_at(vec, 1);
    assert((int)(intptr_t)cwc_vec_at(vec, 0) == 0x20);
    assert((int)(intptr_t)cwc_vec_at(vec, 1) == INT_MIN);
    assert((int)(intptr_t)cwc_vec_at(vec, 2) == -1);
    assert(cwc_vec_find(vec, (void *)-1) == 2);
    assert(vec->count == 3);

    cwc_vec_destroy(vec);
    puts("Functional test passed");
}

static void fill_shrink_test()
{
    struct cwc_vec *vec = cwc_vec_create(sizeof(int), 1e6 + 1);

    printf("filling vector to %d element\n", INT_MAX);
    for (size_t i = 0; i < INT_MAX; i++) {
        if (!cwc_vec_push(vec, (void *)(intptr_t)i))
            cwc_assert(false, "failed to push");
    }

    puts("checking the data");
    for (size_t i = 0; i < INT_MAX; i++) {
        assert((size_t)(uintptr_t)cwc_vec_at(vec, i) == i);
    }

    puts("popping the data");
    for (size_t i = 0; i < INT_MAX; i++) {
        cwc_vec_pop(vec);
        // cwc_vec_pop_at(vec, 0);
    }

    assert(vec->count == 0);

    cwc_vec_destroy(vec);
}

int main()
{

    functional_test();
    fill_shrink_test();

    puts("OK");
    return 0;
}
