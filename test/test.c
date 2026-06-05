#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#undef __STRICT_ANSI__
#define CTEST_MAIN
#define CTEST_SEGFAULT
#define CTEST_COLOR_OK
#include "ctest.h"
#define EXTLIB_IMPL
#include "../extlib.h"

size_t allocated;

void *tracking_alloc(Allocator *a, size_t size) {
    (void)a;
    allocated += size;
    return default_allocator.base.alloc(&default_allocator.base, size);
}

void *tracking_realloc(Allocator *a, void *ptr, size_t old_sz, size_t new_sz) {
    (void)a;
    allocated += (int)new_sz - (int)old_sz;
    return default_allocator.base.realloc(&default_allocator.base, ptr, old_sz, new_sz);
}

void tracking_free(Allocator *a, void *ptr, size_t size) {
    (void)a;
    allocated -= size;
    default_allocator.base.free(&default_allocator.base, ptr, size);
}

Allocator tracking_allocator = {
    tracking_alloc,
    tracking_realloc,
    tracking_free,
};

int main(int argc, const char **argv) {
    int res;
    PUSH_ALLOCATOR(&tracking_allocator) {
        res = ctest_main(argc, argv);
    }
    if(allocated != 0) {
        fprintf(stderr, "\n%s:%d: error: got un-freed data: %zu\n", __FILE__, __LINE__, allocated);
        return 1;
    }
    return res;
}

typedef struct {
    int *items;
    size_t size, capacity;
    Allocator *allocator;
} Ints;

CTEST(alloc, alloc_free) {
    int *i = ext_alloc(sizeof(int));
    ASSERT_TRUE(allocated == sizeof(int));
    i = ext_realloc(i, sizeof(int), sizeof(int) * 20);
    ASSERT_TRUE(allocated == sizeof(int) * 20);
    ext_free(i, sizeof(int) * 20);
    ASSERT_TRUE(allocated == 0);
}

CTEST(alloc, new_delete) {
    int *i = ext_new(int);
    ASSERT_TRUE(allocated == sizeof(int));
    ext_delete(int, i);
    ASSERT_TRUE(allocated == 0);

    int *ints = ext_new_array(int, 20);
    ASSERT_TRUE(allocated == 20 * sizeof(int));
    ext_delete_array(int, 20, ints);
    ASSERT_TRUE(allocated == 0);
}

typedef struct {
    int a, b, c;
} ToClone;

CTEST(alloc, clone) {
    int i = 32;
    int *cloned = ext_clone(int, &i);
    ASSERT_TRUE(i == *cloned);

    int *inline_cloned = ext_clone(int, (int[]){42});
    ASSERT_TRUE(*inline_cloned == 42);

    ToClone *cloned_struct = ext_clone(ToClone, (&(ToClone){1, 2, 3}));
    ASSERT_TRUE(cloned_struct->a == 1 && cloned_struct->b == 2 && cloned_struct->c == 3);

    ext_delete(int, cloned);
    ext_delete(int, inline_cloned);
    ext_delete(ToClone, cloned_struct);
}

CTEST(temp, set_mem) {
    void *new_mem = malloc(1000);
    temp_set_mem(new_mem, 1000);
    ASSERT_TRUE(temp_allocator.start == new_mem);
    ASSERT_TRUE(temp_allocator.mem_size == 1000);
    short *i = temp_alloc(sizeof(int));
    *i = 49;
    ASSERT_TRUE(*i == 49);
    temp_set_mem(ext_temp_mem, sizeof(ext_temp_mem));
    ASSERT_TRUE(temp_allocator.mem == ext_temp_mem);
    ASSERT_TRUE(temp_allocator.mem_size == sizeof(ext_temp_mem));
    free(new_mem);
}

CTEST(temp, reset) {
    temp_alloc(1000);
    ASSERT_TRUE(temp_allocator.start > (char *)temp_allocator.mem);
    temp_reset();
    ASSERT_TRUE(temp_allocator.start == temp_allocator.mem);
}

CTEST(temp, alloc) {
    int *i = temp_alloc(sizeof(*i));
    ASSERT_TRUE(temp_allocator.start - (char *)temp_allocator.mem >= (intptr_t)sizeof(int));
    temp_reset();
}

CTEST(temp, realloc) {
    int *ints = temp_alloc(sizeof(int) * 100);
    for(int i = 0; i < 100; i++) {
        ints[i] = i;
    }
    int *new_ints = temp_realloc(ints, 100 * sizeof(int), 1000 * sizeof(int));
    ASSERT_TRUE(ints == new_ints);
    for(int i = 0; i < 100; i++) {
        ASSERT_TRUE(new_ints[i] == i);
    }
    for(int i = 100; i < 1000; i++) {
        ints[i] = i;
    }
    temp_alloc(sizeof(int));
    new_ints = temp_realloc(ints, 1000 * sizeof(int), 10000 * sizeof(int));
    ASSERT_TRUE(ints != new_ints);
    for(int i = 0; i < 1000; i++) {
        ASSERT_TRUE(new_ints[i] == i);
    }
    temp_reset();
}

CTEST(temp, available) {
    temp_alloc(sizeof(int));
    ASSERT_TRUE(temp_available() <= temp_allocator.mem_size - sizeof(int));
    temp_reset();
}

CTEST(temp, checkpoint) {
    temp_alloc(100 * sizeof(int));
    char *start = temp_allocator.start;
    void *checkpoint = temp_checkpoint();
    temp_alloc(1000 * sizeof(int));
    temp_rewind(checkpoint);
    ASSERT_TRUE(temp_allocator.start == start);
    temp_reset();
}

CTEST(temp, strdup) {
    const char *str = "Cantami, o Diva, del Pelide Achille";
    char *dup = temp_strdup(str);
    ASSERT_TRUE(str != dup && strcmp(str, dup) == 0);
    ASSERT_TRUE(dup[strlen(str)] == 0);
    temp_reset();
}

CTEST(temp, memdup) {
    unsigned char mem[] = "Cantami,\0o\0Diva,\0del\0Pelide\0Achille";
    unsigned char *dup = temp_memdup(mem, sizeof(mem));
    ASSERT_TRUE(mem != dup && memcmp(mem, dup, sizeof(mem)) == 0);
    temp_reset();
}

#ifndef EXTLIB_NO_STD
CTEST(temp, sprintf) {
    char *s = temp_sprintf("%s:%d", "test.c", 162);
    const char *expected = "test.c:162";
    ASSERT_TRUE(strcmp(s, expected) == 0);
    temp_reset();
}
#endif  // EXTLIB_NO_STD

typedef struct {
    Allocator base;
    bool noop;
} NoopAlloc;

void *noop_alloc(Allocator *a, size_t size) {
    (void)size;
    NoopAlloc *allocator = (NoopAlloc *)a;
    ASSERT_TRUE(allocator->noop);
    return (void *)1;
}

void *noop_realloc(Allocator *a, void *ptr, size_t old_sz, size_t new_sz) {
    (void)ptr, (void)old_sz, (void)new_sz;
    NoopAlloc *allocator = (NoopAlloc *)a;
    ASSERT_TRUE(allocator->noop);
    return (void *)2;
}

void noop_free(Allocator *a, void *ptr, size_t size) {
    (void)ptr, (void)size;
    NoopAlloc *allocator = (NoopAlloc *)a;
    ASSERT_TRUE(allocator->noop);
}

NoopAlloc noop_allocator = {{noop_alloc, noop_realloc, noop_free}, true};

CTEST(context, push_pop) {
    Context ctx = *ext_context;
    ctx.alloc = &noop_allocator.base;
    PUSH_CONTEXT(&ctx) {
        ASSERT_TRUE(ext_context == &ctx);
        ASSERT_TRUE(ext_context->alloc->alloc == noop_alloc);
        ASSERT_TRUE(ext_context->prev->alloc == &tracking_allocator);
        int *ptr = ext_alloc(sizeof(int));
        ASSERT_TRUE(ptr == (void *)1);
        ptr = ext_realloc(ptr, sizeof(int), sizeof(int) * 20);
        ASSERT_TRUE(ptr == (void *)2);
        ext_free(ptr, sizeof(int) * 20);
    }
    ASSERT_TRUE(ext_context != &ctx);
    ASSERT_TRUE(ext_context->alloc == &tracking_allocator);
}

CTEST(slist, push_pop) {
    typedef struct Node {
        struct Node *next;
        struct Node *prev;
    } Node;

    Node a = {0}, b = {0};
    Node *head = NULL;

    ASSERT_TRUE(slist_push(head, &a) == &a);
    ASSERT_TRUE(head == &a);
    ASSERT_TRUE(slist_push(head, &b) == &b);
    ASSERT_TRUE(head == &b);
    ASSERT_TRUE(b.next == &a);
    ASSERT_TRUE(slist_pop(head) == &a);
    ASSERT_TRUE(head == &a);

    head = NULL;
    ASSERT_TRUE(slist_push_ext(head, &a, prev) == &a);
    ASSERT_TRUE(slist_push_ext(head, &b, prev) == &b);
    ASSERT_TRUE(b.prev == &a);
    ASSERT_TRUE(slist_pop_ext(head, prev) == &a);
    ASSERT_TRUE(head == &a);
}

CTEST(squeue, push_pop) {
    typedef struct Node {
        struct Node *next;
        struct Node *prev;
    } Node;

    Node a = {0}, b = {0}, c = {0};
    Node *first = NULL;
    Node *last = NULL;

    ASSERT_TRUE(squeue_push(first, last, &a) == &a);
    ASSERT_TRUE(first == &a && last == &a && a.next == NULL);
    ASSERT_TRUE(squeue_push(first, last, &b) == &b);
    ASSERT_TRUE(first == &a && last == &b && a.next == &b && b.next == NULL);
    ASSERT_TRUE(squeue_push_front(first, last, &c) == &c);
    ASSERT_TRUE(first == &c && last == &b && c.next == &a);

    ASSERT_TRUE(squeue_pop(first, last) == &a);
    ASSERT_TRUE(first == &a && last == &b);
    ASSERT_TRUE(squeue_pop(first, last) == &b);
    ASSERT_TRUE(first == &b && last == &b);
    ASSERT_TRUE(squeue_pop(first, last) == NULL);
    ASSERT_TRUE(first == NULL && last == NULL);

    ASSERT_TRUE(squeue_push_ext(first, last, &a, prev) == &a);
    ASSERT_TRUE(squeue_push_ext(first, last, &b, prev) == &b);
    ASSERT_TRUE(first == &a && last == &b && a.prev == &b && b.prev == NULL);
    ASSERT_TRUE(squeue_pop_ext(first, last, prev) == &b);
    ASSERT_TRUE(first == &b && last == &b);
}

CTEST(dlist, insert_remove) {
    typedef struct Node {
        struct Node *next;
        struct Node *prev;
        struct Node *link;
        struct Node *back;
    } Node;

    Node a = {0}, b = {0}, c = {0}, d = {0};
    Node *first = NULL;
    Node *last = NULL;

    ASSERT_TRUE(dlist_push_back(first, last, &a) == &a);
    ASSERT_TRUE(first == &a && last == &a && a.next == NULL && a.prev == NULL);
    ASSERT_TRUE(dlist_push_back(first, last, &b) == &b);
    ASSERT_TRUE(first == &a && last == &b && a.next == &b && b.prev == &a);
    ASSERT_TRUE(dlist_push_front(first, last, &c) == &c);
    ASSERT_TRUE(first == &c && last == &b && c.next == &a && a.prev == &c);
    ASSERT_TRUE(dlist_insert(first, last, &a, &d) == &d);
    ASSERT_TRUE(a.next == &d && d.prev == &a && d.next == &b && b.prev == &d);

    ASSERT_TRUE(dlist_remove(first, last, &d) == &d);
    ASSERT_TRUE(first == &c && last == &b && a.next == &b && b.prev == &a);
    ASSERT_TRUE(dlist_remove(first, last, &c) == &c);
    ASSERT_TRUE(first == &a && last == &b && a.prev == NULL);
    ASSERT_TRUE(dlist_remove(first, last, &b) == &b);
    ASSERT_TRUE(first == &a && last == &a && a.next == NULL);
    ASSERT_TRUE(dlist_remove(first, last, &a) == &a);
    ASSERT_TRUE(first == NULL && last == NULL);

    ASSERT_TRUE(dlist_push_back_ext(first, last, &a, link, back) == &a);
    ASSERT_TRUE(dlist_push_back_ext(first, last, &b, link, back) == &b);
    ASSERT_TRUE(first == &a && last == &b && a.link == &b && b.back == &a);
}

CTEST(arena, alloc_realloc_free) {
    Arena a = make_arena();
    int *i = arena_alloc(&a, sizeof(int));
    *i = 42;
    ASSERT_TRUE(arena_get_allocated(&a) >= sizeof(int));
    ASSERT_TRUE(a.current_page != NULL);
    int *new_i = arena_realloc(&a, i, sizeof(int), sizeof(int) * 20);
    ASSERT_TRUE(*new_i == 42);
    ASSERT_TRUE(i == new_i);
    ASSERT_TRUE(arena_get_allocated(&a) == sizeof(int) * 20);
    for(int i = 1; i < 20; i++) {
        new_i[i] = i;
    }
    arena_alloc(&a, sizeof(int));
    new_i = arena_realloc(&a, i, sizeof(int) * 20, sizeof(int) * 100);
    ASSERT_TRUE(new_i != i);
    ASSERT_TRUE(*new_i == 42);
    ASSERT_TRUE(arena_get_allocated(&a) >= sizeof(int) + sizeof(int) * 20 + sizeof(int) * 100);
    for(int i = 1; i < 20; i++) {
        ASSERT_TRUE(i == new_i[i]);
    }

    size_t old_pos = arena_get_allocated(&a);
    arena_free(&a, new_i, sizeof(int) * 100);
    ASSERT_TRUE(arena_get_allocated(&a) < old_pos);

    arena_reset(&a);
    ASSERT_TRUE(a.current_page == NULL);
    ASSERT_TRUE(a.free_pages != NULL);
    ASSERT_TRUE(arena_get_allocated(&a) == 0);

    arena_destroy(&a);
    ASSERT_TRUE(allocated == 0);
}

CTEST(arena, flexible_page) {
    Arena a = make_arena();
    void *mem = arena_alloc(&a, 1000);
    ASSERT_TRUE(mem != NULL);
    arena_destroy(&a);
}

CTEST(arena, alignment) {
    Arena a = make_arena(.alignment = 128);
    int *i = arena_alloc(&a, sizeof(int));
    ASSERT_TRUE((intptr_t)i % 128 == 0);
    ASSERT_TRUE(arena_get_allocated(&a) ==
                EXT_ALIGN_UP(sizeof(int), 128) + EXT_ALIGN_PAD(a.current_page->data, a.alignment));
    int *new_i = arena_realloc(&a, i, sizeof(int), sizeof(int) * 10);
    ASSERT_TRUE(i == new_i);
    arena_alloc(&a, 1);
    new_i = arena_realloc(&a, new_i, sizeof(int) * 10, sizeof(int) * 100);
    ASSERT_TRUE(i != new_i);
    ASSERT_TRUE((intptr_t)new_i % 128 == 0);
    arena_reset(&a);
    ASSERT_TRUE(a.current_page == NULL);
    ASSERT_TRUE(arena_get_allocated(&a) == 0);
    arena_destroy(&a);
    ASSERT_TRUE(allocated == 0);
}

CTEST(arena, custom_allocator) {
    Arena a = make_arena(.page_allocator = &temp_allocator.base);
    int *i = arena_alloc(&a, sizeof(int));
    ASSERT_TRUE(i != NULL);
    ASSERT_TRUE(temp_allocator.start - (char *)temp_allocator.mem >= (intptr_t)sizeof(int));
    ASSERT_TRUE(allocated == 0);
    arena_destroy(&a);
    temp_reset();
}

CTEST(arena, strdup) {
    Arena a = make_arena();
    const char *str = "Cantami, o Diva, del Pelide Achille";
    char *dup = arena_strdup(&a, str);
    ASSERT_TRUE(str != dup && strcmp(str, dup) == 0);
    ASSERT_TRUE(dup[strlen(str)] == 0);
    ext_arena_destroy(&a);
}

CTEST(arena, memdup) {
    Arena a = make_arena();
    unsigned char mem[] = "Cantami,\0o\0Diva,\0del\0Pelide\0Achille";
    unsigned char *dup = arena_memdup(&a, mem, sizeof(mem));
    ASSERT_TRUE(mem != dup && memcmp(mem, dup, sizeof(mem)) == 0);
    arena_destroy(&a);
}

#ifndef EXTLIB_NO_STD
CTEST(arena, sprintf) {
    Arena a = make_arena();
    char *s = arena_sprintf(&a, "%d %s", 3, "ciao");
    ASSERT_TRUE(strcmp(s, "3 ciao") == 0);
    arena_destroy(&a);
}
#endif  // EXTLIB_NO_STD

CTEST(arena, reset) {
    Arena a = make_arena(.page_allocator = &temp_allocator.base);

    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    ASSERT_TRUE(arena_get_allocated(&a) > 0);

    arena_reset(&a);
    ASSERT_TRUE(arena_get_allocated(&a) == 0);
    ASSERT_TRUE(a.current_page == NULL);
    ASSERT_TRUE(a.free_pages != NULL);

    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    arena_destroy(&a);
    temp_reset();
}

CTEST(arena, reset_reuses_first_usable_free_page) {
    Arena a = make_arena();

    void *large = arena_alloc(&a, a.page_size * 2);
    ASSERT_TRUE(large != NULL);
    ArenaPage *large_page = a.current_page;

    void *small = arena_alloc(&a, 64);
    ASSERT_TRUE(small != NULL);
    ArenaPage *normal_page = a.current_page;
    ASSERT_TRUE(normal_page != large_page);
    ASSERT_TRUE(normal_page->size < large_page->size);

    arena_reset(&a);
    small = arena_alloc(&a, 64);
    ASSERT_TRUE(small != NULL);
    ASSERT_TRUE(a.current_page == large_page || a.current_page == normal_page);

    arena_destroy(&a);
    ASSERT_TRUE(allocated == 0);
}

CTEST(arena, rewind) {
    Arena a = make_arena(.page_allocator = &temp_allocator.base);

    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    ASSERT_TRUE(arena_get_allocated(&a) > 0);

    ArenaPage *last_page = a.current_page;
    size_t old_pos = arena_get_allocated(&a);
    ArenaCheckpoint c = arena_checkpoint(&a);

    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    arena_rewind(&a, c);
    ASSERT_TRUE(a.current_page == last_page);
    ASSERT_TRUE(arena_get_allocated(&a) == old_pos);

    arena_destroy(&a);
    temp_reset();
}

CTEST(arena, rewind_reuses_freed_pages) {
    Arena a = make_arena();

    arena_alloc(&a, 32);
    ArenaCheckpoint c = arena_checkpoint(&a);
    ArenaPage *checkpoint_page = a.current_page;
    size_t old_pos = arena_get_allocated(&a);

    void *large = arena_alloc(&a, a.page_size * 2);
    ASSERT_TRUE(large != NULL);
    ArenaPage *freed_page = a.current_page;
    ASSERT_TRUE(freed_page != checkpoint_page);

    arena_rewind(&a, c);
    ASSERT_TRUE(a.current_page == checkpoint_page);
    ASSERT_TRUE(a.free_pages == freed_page);
    ASSERT_TRUE(arena_get_allocated(&a) == old_pos);

    large = arena_alloc(&a, a.page_size * 2);
    ASSERT_TRUE(large != NULL);
    ASSERT_TRUE(a.current_page == freed_page);

    arena_rewind(&a, c);
    ASSERT_TRUE(a.current_page == checkpoint_page);
    ASSERT_TRUE(arena_get_allocated(&a) == old_pos);

    arena_destroy(&a);
    ASSERT_TRUE(allocated == 0);
}

CTEST(arena, checkpoint) {
    Arena a = make_arena(.page_allocator = &temp_allocator.base);

    size_t old_pos = arena_get_allocated(&a);
    ArenaCheckpoint c = arena_checkpoint(&a);

    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    ASSERT_TRUE(arena_get_allocated(&a) > 0);

    arena_rewind(&a, c);
    ASSERT_TRUE(arena_get_allocated(&a) == old_pos);

    for(int i = 0; i < 500; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }

    old_pos = arena_get_allocated(&a);
    Ext_ArenaPage *last_page = a.current_page;
    c = arena_checkpoint(&a);
    for(int i = 0; i < 5000; i++) {
        arena_sprintf(&a, "Mem %d", i);
    }
    arena_rewind(&a, c);
    ASSERT_TRUE(a.current_page == last_page);
    ASSERT_TRUE(arena_get_allocated(&a) == old_pos);

    arena_destroy(&a);
    temp_reset();
}

CTEST(arena, fixed_page_size) {
    Arena a = make_arena(.flags = EXT_ARENA_FIXED_PAGE_SIZE);
    int *i = arena_alloc(&a, sizeof(int));
    ASSERT_TRUE(i != NULL);
    *i = 42;
    ASSERT_TRUE(*i == 42);
    arena_destroy(&a);
}

CTEST(arena, no_chain) {
    Arena a = make_arena(.flags = EXT_ARENA_NO_CHAIN);
    int *i = arena_alloc(&a, sizeof(int));
    ASSERT_TRUE(i != NULL);
    *i = 42;
    ASSERT_TRUE(*i == 42);
    ASSERT_TRUE(a.current_page != NULL);
    arena_destroy(&a);
}

CTEST(arena, reset_empty) {
    Arena a = make_arena();
    arena_reset(&a);
    ASSERT_TRUE(a.current_page == NULL);
}

CTEST(array, reserve) {
    Ints ints = {0};
    array_reserve(&ints, 100);
    ASSERT_TRUE(ints.capacity >= 100);
    size_t cur_capacity = ints.capacity;
    array_reserve(&ints, 5);
    ASSERT_TRUE(ints.capacity == cur_capacity);
    array_free(&ints);
    ASSERT_TRUE(allocated == 0);
}

CTEST(array, reserve_exact) {
    Ints ints = {0};
    array_reserve_exact(&ints, 100);
    ASSERT_TRUE(ints.capacity == 100);
    array_reserve_exact(&ints, 5);
    ASSERT_TRUE(ints.capacity == 100);
    array_free(&ints);
}

CTEST(array, push) {
    Ints ints = {0};
    for(int i = 0; i < 100; i++) {
        array_push(&ints, i);
    }
    for(int i = 0; i < (int)ints.size; i++) {
        ASSERT_TRUE(ints.items[i] == i);
    }
    array_free(&ints);
}

CTEST(array, foreach) {
    Ints ints = {0};
    for(int i = 0; i < 100; i++) {
        array_push(&ints, i);
    }
    int i = 0;
    array_foreach(int, it, &ints) {
        ASSERT_TRUE(*it == i);
        i++;
    }
    array_free(&ints);
}

CTEST(array, push_all) {
    Ints ints = {0};
    int items[] = {0, 1, 2, 3, 4, 5};
    array_push_all(&ints, items, EXT_ARR_SIZE(items));
    ASSERT_TRUE(ints.size == EXT_ARR_SIZE(items));
    for(int i = 0; i < (int)ints.size; i++) {
        ASSERT_TRUE(ints.items[i] == i);
    }
    array_free(&ints);
}

CTEST(array, pop) {
    Ints ints = {0};
    array_push(&ints, 1);
    array_push(&ints, 2);
    ASSERT_TRUE(ints.size == 2);
    ASSERT_TRUE(ints.items[ints.size - 1] == 2);
    int elem = array_pop(&ints);
    ASSERT_TRUE(elem == 2);
    ASSERT_TRUE(ints.size == 1);
    elem = array_pop(&ints);
    ASSERT_TRUE(elem == 1);
    ASSERT_TRUE(ints.size == 0);
    array_free(&ints);
}

CTEST(array, remove) {
    Ints ints = {0};
    int items[] = {1, 2, 3, 4, 5};
    array_push_all(&ints, items, EXT_ARR_SIZE(items));
    array_remove(&ints, 2);
    ASSERT_TRUE(ints.items[2] == 4);
    array_remove(&ints, 0);
    ASSERT_TRUE(ints.items[0] == 2);
    array_remove(&ints, ints.size - 1);
    ASSERT_TRUE(ints.items[ints.size - 1] == 4);
    ASSERT_TRUE(ints.size == 2 && ints.items[0] == 2 && ints.items[1] == 4);
    array_free(&ints);
}

CTEST(array, swap_remove) {
    Ints ints = {0};
    int items[] = {1, 2, 3, 4, 5};
    array_push_all(&ints, items, EXT_ARR_SIZE(items));
    array_swap_remove(&ints, 2);
    ASSERT_TRUE(ints.items[2] == 5);
    array_swap_remove(&ints, 0);
    ASSERT_TRUE(ints.items[0] == 4);
    array_swap_remove(&ints, ints.size - 1);
    ASSERT_TRUE(ints.items[ints.size - 1] == 2);
    ASSERT_TRUE(ints.size == 2 && ints.items[0] == 4 && ints.items[1] == 2);
    array_free(&ints);
}

CTEST(array, clear) {
    Ints ints = {0};
    array_push(&ints, 1);
    array_push(&ints, 2);
    ASSERT_TRUE(ints.size == 2);
    array_clear(&ints);
    ASSERT_TRUE(ints.size == 0 && ints.capacity > 0);
    array_free(&ints);
}

CTEST(array, resize) {
    Ints ints = {0};
    array_resize(&ints, 100);
    ASSERT_TRUE(ints.size == 100);
    array_resize(&ints, 10);
    ASSERT_TRUE(ints.size == 10);
    array_free(&ints);
}

CTEST(array, shrink_to_fit) {
    Ints ints = {0};
    array_push(&ints, 1);
    array_push(&ints, 2);
    ASSERT_TRUE(ints.capacity > 2);
    array_shrink_to_fit(&ints);
    ASSERT_TRUE(ints.capacity == 2);
    (void)array_pop(&ints);
    (void)array_pop(&ints);
    array_shrink_to_fit(&ints);
    ASSERT_TRUE(ints.size == 0 && ints.capacity == 0 && ints.items == NULL);
    array_free(&ints);
}

CTEST(array, allocator) {
    Ints ints = {0};
    size_t temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail == 0);
    ints.allocator = &temp_allocator.base;
    for(int i = 0; i < 100; i++) {
        array_push(&ints, i);
    }
    temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail >= 100 * sizeof(int));
    ASSERT_TRUE(allocated == 0);
    temp_reset();
}

CTEST(array, ctx_allocator) {
    PUSH_ALLOCATOR(&temp_allocator.base) {
        Ints ints = {0};
        size_t temp_avail = temp_allocator.end - temp_allocator.start;
        ASSERT_TRUE(temp_allocator.mem_size - temp_avail == 0);
        for(int i = 0; i < 100; i++) {
            array_push(&ints, i);
        }
        temp_avail = temp_allocator.end - temp_allocator.start;
        ASSERT_TRUE(temp_allocator.mem_size - temp_avail >= 100 * sizeof(int));
        ASSERT_TRUE(allocated == 0);
    }
    temp_reset();
}

CTEST(sb, append) {
    const char s[] = "Cantami,\0o\0Diva,\0del\0Pelide\0Achille";
    StringBuffer sb = {0};
    sb_append(&sb, s, sizeof(s) - 1);
    ASSERT_TRUE(sb.size == sizeof(s) - 1);
    ASSERT_TRUE(memcmp(s, sb.items, sizeof(s) - 1) == 0);
    sb_free(&sb);
}

CTEST(sb, append_cstr) {
    const char s[] = "Cantami, o Diva, del Pelide Achille";
    StringBuffer sb = {0};
    sb_append_cstr(&sb, s);
    ASSERT_TRUE(sb.size == sizeof(s) - 1);
    ASSERT_TRUE(memcmp(s, sb.items, sizeof(s) - 1) == 0);
    sb_free(&sb);
}

CTEST(sb, prepend) {
    const char prefix[] = "Cantami,\0o\0Diva,\0del\0Pelide";
    const char suffix[] = "\0Achille";
    const char expected[] = "Cantami,\0o\0Diva,\0del\0Pelide\0Achille";
    StringBuffer sb = {0};
    sb_append(&sb, suffix, sizeof(suffix) - 1);
    sb_prepend(&sb, prefix, sizeof(prefix) - 1);
    ASSERT_TRUE(memcmp(sb.items, expected, sb.size) == 0);
    sb_free(&sb);
}

CTEST(sb, prepend_cstr) {
    const char prefix[] = "Cantami, o Diva, del Pelide";
    const char suffix[] = " Achille";
    const char expected[] = "Cantami, o Diva, del Pelide Achille";
    StringBuffer sb = {0};
    sb_append_cstr(&sb, suffix);
    sb_prepend_cstr(&sb, prefix);
    ASSERT_TRUE(memcmp(sb.items, expected, sb.size) == 0);
    sb_free(&sb);
}

CTEST(sb, prepend_char) {
    const char suffix[] = "antami, o Diva, del Pelide Achille";
    const char expected[] = "Cantami, o Diva, del Pelide Achille";
    StringBuffer sb = {0};
    sb_append_cstr(&sb, suffix);
    sb_prepend_char(&sb, 'C');
    ASSERT_TRUE(memcmp(sb.items, expected, sb.size) == 0);
    sb_free(&sb);
}

CTEST(sb, replace) {
    const char s[] = "Cantami,\bo\vDiva,\ndel\rPelide\tAchille";
    const char expected[] = "Cantami, o Diva, del Pelide Achille";
    StringBuffer sb = {0};
    sb_append_cstr(&sb, s);
    sb_replace(&sb, 0, "\b\v\n\r\t", ' ');
    ASSERT_TRUE(memcmp(sb.items, expected, sb.size) == 0);
    sb_replace(&sb, 0, "@", '$');
    ASSERT_TRUE(memcmp(sb.items, expected, sb.size) == 0);
    sb_free(&sb);
}

CTEST(sb, to_upper) {
    StringBuffer sb = {0};
    sb_append_cstr(&sb, "Hello, World! 123");
    sb_to_upper(&sb);
    ASSERT_TRUE(memcmp(sb.items, "HELLO, WORLD! 123", sb.size) == 0);

    // Already uppercase
    sb_to_upper(&sb);
    ASSERT_TRUE(memcmp(sb.items, "HELLO, WORLD! 123", sb.size) == 0);

    sb_free(&sb);
}

CTEST(sb, to_lower) {
    StringBuffer sb = {0};
    sb_append_cstr(&sb, "Hello, World! 123");
    sb_to_lower(&sb);
    ASSERT_TRUE(memcmp(sb.items, "hello, world! 123", sb.size) == 0);

    // Already lowercase
    sb_to_lower(&sb);
    ASSERT_TRUE(memcmp(sb.items, "hello, world! 123", sb.size) == 0);

    sb_free(&sb);
}

CTEST(sb, reverse) {
    StringBuffer sb = {0};

    // Odd-length string
    sb_append_cstr(&sb, "abcde");
    sb_reverse(&sb);
    ASSERT_TRUE(memcmp(sb.items, "edcba", sb.size) == 0);
    sb.size = 0;

    // Even-length string
    sb_append_cstr(&sb, "abcd");
    sb_reverse(&sb);
    ASSERT_TRUE(memcmp(sb.items, "dcba", sb.size) == 0);
    sb.size = 0;

    // Single char
    sb_append_cstr(&sb, "x");
    sb_reverse(&sb);
    ASSERT_TRUE(memcmp(sb.items, "x", sb.size) == 0);
    sb.size = 0;

    // Empty
    sb_reverse(&sb);
    ASSERT_TRUE(sb.size == 0);

    sb_free(&sb);
}

CTEST(sb, to_cstr) {
    StringBuffer sb = {0};
    const char s[] = "Cantami, o Diva, del Pelide Achille";
    sb_append(&sb, s, sizeof(s) - 1);
    char *res = sb_to_cstr(&sb);
    ASSERT_TRUE(strcmp(s, res) == 0);
    ext_free(res, strlen(res) + 1);
}

#ifndef EXTLIB_NO_STD
CTEST(sb, appendf) {
    StringBuffer sb = {0};
    int res = sb_appendf(&sb, "%s:%d", "test.c", 494);
    const char *expected = "test.c:494";
    ASSERT_TRUE(res == (int)strlen(expected));
    ASSERT_TRUE(memcmp(expected, sb.items, strlen(expected)) == 0);
    sb_free(&sb);
}
#endif

CTEST(slice, slice) {
    char s1[] = "Hello, world!";
    StringSlice ss = ss_from_cstr(s1);
    ASSERT_TRUE(ss.size == sizeof(s1) - 1);
    ASSERT_TRUE(memcmp(s1, ss.data, sizeof(s1) - 1) == 0);

    char s2[] = "Hello,\0world!";
    ss = ss_from(s2, sizeof(s2) - 1);
    ASSERT_TRUE(ss.size == sizeof(s2) - 1);
    ASSERT_TRUE(memcmp(s2, ss.data, sizeof(s2) - 1) == 0);

    StringBuffer sb = {0};
    sb_append_cstr(&sb, "Hello");
    sb_append_cstr(&sb, ", ");
    sb_append_cstr(&sb, "World!");
    ss = sb_to_ss(sb);
    ASSERT_TRUE(ss.size == sb.size);
    ASSERT_TRUE(memcmp(sb.items, ss.data, sb.size) == 0);

    sb_free(&sb);
}

CTEST(slice, split_once) {
    const char *expected[] = {
        "Cantami,", "o", "Diva,", "del", "Pelide", "Achille",
    };
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_split_once(&ss, ' ');
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, rsplit_once) {
    const char *expected[] = {
        "Achille", "Pelide", "del", "Diva,", "o", "Cantami,",
    };
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_rsplit_once(&ss, ' ');
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, split_once_cstr) {
    const char *expected[] = {
        "Cantami",
        "o Diva",
        "del Pelide Achille",
    };
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    size_t ss_len = ss.size;

    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, ", ");
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, ". ");
        ASSERT_TRUE(word.size == ss_len && ss.size == 0);
    }

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, ".....................................");
        ASSERT_TRUE(word.size == ss_len && ss.size == 0);
    }

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, "Cantami, o Diva, del Pelide Achille");
        ASSERT_TRUE(word.size == 0 && ss.size == 0);
    }
}

CTEST(slice, rsplit_once_cstr) {
    const char *expected[] = {
        "del Pelide Achille",
        "o Diva",
        "Cantami",
    };
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    size_t ss_len = ss.size;

    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_rsplit_once_cstr(&ss, ", ");
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, ". ");
        ASSERT_TRUE(word.size == ss_len && ss.size == 0);
    }

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, ".....................................");
        ASSERT_TRUE(word.size == ss_len && ss.size == 0);
    }

    ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    while(ss.size) {
        StringSlice word = ss_split_once_cstr(&ss, "Cantami, o Diva, del Pelide Achille");
        ASSERT_TRUE(word.size == 0 && ss.size == 0);
    }
}

CTEST(slice, split_once_any) {
    const char *expected[] = {"one", "two", "three", "four", "five", "six"};
    StringSlice ss = ss_from_cstr("one,two three,four.five six");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_split_once_any(&ss, ",. ");
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, rsplit_once_any) {
    const char *expected[] = {"six", "five", "four", "three", "two", "one"};
    StringSlice ss = ss_from_cstr("one,two three,four.five six");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_rsplit_once_any(&ss, ",. ");
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, split_once_ws) {
    const char *expected[] = {
        "Cantami,", "o", "Diva,", "del", "Pelide", "Achille",
    };
    StringSlice ss = ss_from_cstr("Cantami, o\f\tDiva,\ndel\v\r\nPelide \v Achille");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_split_once_ws(&ss);
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, rsplit_once_ws) {
    const char *expected[] = {"Achille", "Pelide", "del", "Diva,", "o", "Cantami,"};
    StringSlice ss = ss_from_cstr("Cantami, o\f\tDiva,\ndel\v\r\nPelide \v Achille");
    size_t i = 0;
    while(ss.size) {
        StringSlice word = ss_rsplit_once_ws(&ss);
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i++], word.size) == 0);
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));
}

CTEST(slice, trim_start) {
    const char expected[] = "Cantami, o Diva, del Pelide Achille\n";
    StringSlice s = ss_from_cstr("\t\n  \v \r Cantami, o Diva, del Pelide Achille\n");
    StringSlice trimmed = ss_trim_start(s);
    ASSERT_TRUE(memcmp(expected, trimmed.data, trimmed.size) == 0);
    StringSlice trimmed2 = ss_trim_start(s);
    ASSERT_TRUE(trimmed.size == trimmed2.size &&
                memcmp(trimmed2.data, trimmed.data, trimmed.size) == 0);

    s = ss_from_cstr("");
    trimmed = ss_trim_start(s);
    ASSERT_TRUE(trimmed.size == 0);
}

CTEST(slice, trim_end) {
    const char expected[] = "\nCantami, o Diva, del Pelide Achille";
    StringSlice s = ss_from_cstr("\nCantami, o Diva, del Pelide Achille\t\n  \v \r ");
    StringSlice trimmed = ss_trim_end(s);
    ASSERT_TRUE(memcmp(expected, trimmed.data, trimmed.size) == 0);
    StringSlice trimmed2 = ss_trim_end(s);
    ASSERT_TRUE(trimmed.size == trimmed2.size &&
                memcmp(trimmed2.data, trimmed.data, trimmed.size) == 0);

    s = ss_from_cstr("");
    trimmed = ss_trim_end(s);
    ASSERT_TRUE(trimmed.size == 0);
}

CTEST(slice, trim) {
    const char expected[] = "Cantami, o Diva, del Pelide Achille";
    StringSlice s = ss_from_cstr("\t\n  \v \r Cantami, o Diva, del Pelide Achille\t\n  \v \r ");
    StringSlice trimmed = ss_trim(s);
    ASSERT_TRUE(memcmp(expected, trimmed.data, trimmed.size) == 0);
    StringSlice trimmed2 = ss_trim(s);
    ASSERT_TRUE(trimmed.size == trimmed2.size &&
                memcmp(trimmed2.data, trimmed.data, trimmed.size) == 0);

    s = ss_from_cstr("");
    trimmed = ss_trim(s);
    ASSERT_TRUE(trimmed.size == 0);
}

CTEST(slice, cut) {
    const char expected[] = "o Diva, del Pelide Achille";
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    StringSlice trunc = ss_cut(ss, 0);
    ASSERT_TRUE(ss_eq(trunc, ss));
    trunc = ss_cut(ss, 9);
    ASSERT_TRUE(memcmp(trunc.data, expected, trunc.size) == 0);
    trunc = ss_cut(ss, 350);
    ASSERT_TRUE(trunc.size == 0);
}

CTEST(slice, trunc_end) {
    const char expected[] = "Cantami, o Diva, del Pelide";
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    StringSlice trunc = ss_trunc(ss, 0);
    ASSERT_TRUE(trunc.size == 0);
    trunc = ss_trunc(ss, 27);
    ASSERT_TRUE(memcmp(trunc.data, expected, trunc.size) == 0);
    trunc = ss_trunc(ss, 350);
    ASSERT_TRUE(trunc.size == ss.size && ss_eq(trunc, ss));
}

CTEST(slice, substr) {
    StringSlice ss = ss_from_cstr("Hello, World!");

    // Middle substring
    ASSERT_TRUE(ss_eq(ss_substr(ss, 7, 5), SS("World")));

    // Start at 0
    ASSERT_TRUE(ss_eq(ss_substr(ss, 0, 5), SS("Hello")));

    // len exceeds remaining
    ASSERT_TRUE(ss_eq(ss_substr(ss, 7, 100), SS("World!")));

    // start past end
    ASSERT_TRUE(ss_substr(ss, 100, 5).size == 0);
}

CTEST(slice, starts_with) {
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    ASSERT_TRUE(ss_starts_with(ss, ss_from_cstr("Cantami,")));
    ASSERT_TRUE(ss_starts_with(ss, ss_from_cstr("")));
    ASSERT_TRUE(ss_starts_with(ss, ss_from_cstr("Cantami, o Diva, del Pelide Achille")));
    ASSERT_TRUE(!ss_starts_with(ss, ss_from_cstr("Cantami, o Diva, del Pelide Achille, l'ira "
                                                 "funesta che infiniti addusse lutti agli Achei")));
    ASSERT_TRUE(!ss_starts_with(ss, ss_from_cstr("Narrami, o Musa,")));
}

CTEST(slice, ends_with) {
    StringSlice ss = ss_from_cstr("Cantami, o Diva, del Pelide Achille");
    ASSERT_TRUE(ss_ends_with(ss, ss_from_cstr("Pelide Achille")));
    ASSERT_TRUE(ss_ends_with(ss, ss_from_cstr("")));
    ASSERT_TRUE(ss_ends_with(ss, ss_from_cstr("Cantami, o Diva, del Pelide Achille")));
    ASSERT_TRUE(!ss_ends_with(ss, ss_from_cstr("Cantami, o Diva, del Pelide Achille, l'ira funesta "
                                               "che infiniti addusse lutti agli Achei")));
    ASSERT_TRUE(
        !ss_ends_with(ss, ss_from_cstr("per acquistare a sé la vita e il ritorno ai compagni.")));
}

CTEST(slice, strip_prefix) {
    StringSlice ss = ss_from_cstr("Hello, World!");

    // Prefix present
    StringSlice stripped = ss_strip_prefix(ss, SS("Hello, "));
    ASSERT_TRUE(ss_eq(stripped, SS("World!")));

    // Prefix absent — returns original
    stripped = ss_strip_prefix(ss, SS("Goodbye"));
    ASSERT_TRUE(ss_eq(stripped, ss));

    // Empty prefix — returns original
    stripped = ss_strip_prefix(ss, SS(""));
    ASSERT_TRUE(ss_eq(stripped, ss));

    // Prefix == entire string
    stripped = ss_strip_prefix(ss, ss);
    ASSERT_TRUE(stripped.size == 0);

    // cstr variant
    stripped = ss_strip_prefix_cstr(ss, "Hello, ");
    ASSERT_TRUE(ss_eq(stripped, SS("World!")));
    stripped = ss_strip_prefix_cstr(ss, "Goodbye");
    ASSERT_TRUE(ss_eq(stripped, ss));
}

CTEST(slice, strip_suffix) {
    StringSlice ss = ss_from_cstr("Hello, World!");

    // Suffix present
    StringSlice stripped = ss_strip_suffix(ss, SS(", World!"));
    ASSERT_TRUE(ss_eq(stripped, SS("Hello")));

    // Suffix absent — returns original
    stripped = ss_strip_suffix(ss, SS("Goodbye"));
    ASSERT_TRUE(ss_eq(stripped, ss));

    // Empty suffix — returns original
    stripped = ss_strip_suffix(ss, SS(""));
    ASSERT_TRUE(ss_eq(stripped, ss));

    // Suffix == entire string
    stripped = ss_strip_suffix(ss, ss);
    ASSERT_TRUE(stripped.size == 0);

    // cstr variant
    stripped = ss_strip_suffix_cstr(ss, ", World!");
    ASSERT_TRUE(ss_eq(stripped, SS("Hello")));
    stripped = ss_strip_suffix_cstr(ss, "Goodbye");
    ASSERT_TRUE(ss_eq(stripped, ss));
}

CTEST(slice, eq) {
    ASSERT_TRUE(ss_eq(ss_from_cstr("Hello"), ss_from_cstr("Hello")));
    ASSERT_TRUE(!ss_eq(ss_from_cstr("Hello"), ss_from_cstr("Olleh")));
    ASSERT_TRUE(!ss_eq(ss_from_cstr("Hello"), ss_from_cstr("")));
}

CTEST(slice, eq_ignore_case) {
    ASSERT_TRUE(ss_eq_ignore_case(SS("Hello"), SS("hello")));
    ASSERT_TRUE(ss_eq_ignore_case(SS("HELLO"), SS("hello")));
    ASSERT_TRUE(ss_eq_ignore_case(SS(""), SS("")));
    ASSERT_TRUE(!ss_eq_ignore_case(SS("Hello"), SS("World")));
    ASSERT_TRUE(!ss_eq_ignore_case(SS("Hello"), SS("Hell")));

    // Digits/punctuation unchanged
    ASSERT_TRUE(ss_eq_ignore_case(SS("test-123"), SS("TEST-123")));
}

CTEST(slice, cmp_ignore_case) {
    ASSERT_TRUE(ss_cmp_ignore_case(SS("abc"), SS("ABC")) == 0);
    ASSERT_TRUE(ss_cmp_ignore_case(SS("abc"), SS("abd")) < 0);
    ASSERT_TRUE(ss_cmp_ignore_case(SS("abd"), SS("ABC")) > 0);
    ASSERT_TRUE(ss_cmp_ignore_case(SS("ab"), SS("ABC")) < 0);
    ASSERT_TRUE(ss_cmp_ignore_case(SS("abc"), SS("AB")) > 0);
    ASSERT_TRUE(ss_cmp_ignore_case(SS(""), SS("")) == 0);
}

CTEST(slice, starts_with_ignore_case) {
    StringSlice ss = SS("Hello, World!");
    ASSERT_TRUE(ss_starts_with_ignore_case(ss, SS("hello")));
    ASSERT_TRUE(ss_starts_with_ignore_case(ss, SS("HELLO, WORLD!")));
    ASSERT_TRUE(ss_starts_with_ignore_case(ss, SS("")));
    ASSERT_TRUE(!ss_starts_with_ignore_case(ss, SS("world")));
    ASSERT_TRUE(!ss_starts_with_ignore_case(ss, SS("Hello, World! Extra")));

    // cstr variant
    ASSERT_TRUE(ss_starts_with_ignore_case_cstr(ss, "hello"));
    ASSERT_TRUE(!ss_starts_with_ignore_case_cstr(ss, "world"));
}

CTEST(slice, ends_with_ignore_case) {
    StringSlice ss = SS("Hello, World!");
    ASSERT_TRUE(ss_ends_with_ignore_case(ss, SS("world!")));
    ASSERT_TRUE(ss_ends_with_ignore_case(ss, SS("HELLO, WORLD!")));
    ASSERT_TRUE(ss_ends_with_ignore_case(ss, SS("")));
    ASSERT_TRUE(!ss_ends_with_ignore_case(ss, SS("hello")));
    ASSERT_TRUE(!ss_ends_with_ignore_case(ss, SS("Extra Hello, World!")));

    // cstr variant
    ASSERT_TRUE(ss_ends_with_ignore_case_cstr(ss, "world!"));
    ASSERT_TRUE(!ss_ends_with_ignore_case_cstr(ss, "hello"));
}

CTEST(slice, to_cstr) {
    StringSlice ss = ss_from_cstr("Cantami o diva del pelide Achille");
    char *copy = ss_to_cstr_alloc(ss, &temp_allocator.base);
    ASSERT_TRUE(copy[ss.size] == '\0');
    ASSERT_TRUE(memcmp(ss.data, copy, ss.size) == 0);
    temp_reset();
}

CTEST(slice, foreach_split) {
    const char *expected[] = {
        "Cantami,", "o", "Diva,", "del", "Pelide", "Achille",
    };
    size_t i = 0;
    ss_foreach_split(SS("Cantami, o Diva, del Pelide Achille"), ' ', word) {
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i], word.size) == 0);
        i++;
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    // Single element (no delimiter found)
    i = 0;
    ss_foreach_split(SS("hello"), ' ', word) {
        ASSERT_TRUE(ss_eq(word, SS("hello")));
        i++;
    }
    ASSERT_TRUE(i == 1);
}

CTEST(slice, foreach_rsplit) {
    const char *expected[] = {
        "Achille", "Pelide", "del", "Diva,", "o", "Cantami,",
    };
    size_t i = 0;
    ss_foreach_rsplit(SS("Cantami, o Diva, del Pelide Achille"), ' ', word) {
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i], word.size) == 0);
        i++;
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    // Single element (no delimiter found)
    i = 0;
    ss_foreach_rsplit(SS("hello"), ' ', word) {
        ASSERT_TRUE(ss_eq(word, SS("hello")));
        i++;
    }
    ASSERT_TRUE(i == 1);
}

CTEST(slice, foreach_split_cstr) {
    const char *expected[] = {
        "Cantami",
        "o Diva",
        "del Pelide Achille",
    };
    size_t i = 0;
    ss_foreach_split_cstr(SS("Cantami, o Diva, del Pelide Achille"), ", ", word) {
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i], word.size) == 0);
        i++;
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    // Delimiter not found — yields entire string
    i = 0;
    ss_foreach_split_cstr(SS("hello"), ", ", word) {
        ASSERT_TRUE(ss_eq(word, SS("hello")));
        i++;
    }
    ASSERT_TRUE(i == 1);
}

CTEST(slice, foreach_rsplit_cstr) {
    const char *expected[] = {
        "del Pelide Achille",
        "o Diva",
        "Cantami",
    };
    size_t i = 0;
    ss_foreach_rsplit_cstr(SS("Cantami, o Diva, del Pelide Achille"), ", ", word) {
        ASSERT_TRUE(i < EXT_ARR_SIZE(expected));
        ASSERT_TRUE(memcmp(word.data, expected[i], word.size) == 0);
        i++;
    }
    ASSERT_TRUE(i == EXT_ARR_SIZE(expected));

    // Delimiter not found — yields entire string
    i = 0;
    ss_foreach_rsplit_cstr(SS("hello"), ", ", word) {
        ASSERT_TRUE(ss_eq(word, SS("hello")));
        i++;
    }
    ASSERT_TRUE(i == 1);
}

CTEST(slice, find_char) {
    StringSlice ss = ss_from_cstr("hello world");

    ASSERT_TRUE(ss_find_char(ss, 'h', 0) == 0);
    ASSERT_TRUE(ss_find_char(ss, 'o', 0) == 4);
    ASSERT_TRUE(ss_find_char(ss, 'o', 5) == 7);
    ASSERT_TRUE(ss_find_char(ss, 'z', 0) == -1);

    // Offset past end
    ASSERT_TRUE(ss_find_char(ss, 'h', ss.size) == -1);
}

CTEST(slice, rfind_char) {
    StringSlice ss = ss_from_cstr("hello world");

    ASSERT_TRUE(ss_rfind_char(ss, 'd', ss.size) == 10);
    ASSERT_TRUE(ss_rfind_char(ss, 'o', ss.size) == 7);
    ASSERT_TRUE(ss_rfind_char(ss, 'o', 5) == 4);
    ASSERT_TRUE(ss_rfind_char(ss, 'z', ss.size) == -1);

    // Offset at 0 — nothing to search
    ASSERT_TRUE(ss_rfind_char(ss, 'h', 0) == -1);
}

CTEST(slice, find) {
    StringSlice ss = ss_from_cstr("hello world hello");

    // Basic forward find
    ASSERT_TRUE(ss_find(ss, SS("hello"), 0) == 0);
    ASSERT_TRUE(ss_find(ss, SS("world"), 0) == 6);

    // With offset
    ASSERT_TRUE(ss_find(ss, SS("hello"), 1) == 12);
    ASSERT_TRUE(ss_find(ss, SS("hello"), 12) == 12);

    // Not found
    ASSERT_TRUE(ss_find(ss, SS("xyz"), 0) == -1);

    // Offset past end
    ASSERT_TRUE(ss_find(ss, SS("hello"), 13) == -1);

    // Empty needle
    ASSERT_TRUE(ss_find(ss, SS(""), 0) == 0);
    ASSERT_TRUE(ss_find(ss, SS(""), 5) == 5);

    // Needle larger than haystack
    ASSERT_TRUE(ss_find(SS("hi"), SS("hello"), 0) == -1);
}

CTEST(slice, rfind) {
    StringSlice ss = ss_from_cstr("hello world hello");

    // Basic reverse find
    ASSERT_TRUE(ss_rfind(ss, SS("hello"), ss.size) == 12);
    ASSERT_TRUE(ss_rfind(ss, SS("world"), ss.size) == 6);

    // With offset limiting search
    ASSERT_TRUE(ss_rfind(ss, SS("hello"), 11) == 0);
    ASSERT_TRUE(ss_rfind(ss, SS("hello"), 0) == 0);

    // Not found
    ASSERT_TRUE(ss_rfind(ss, SS("xyz"), ss.size) == -1);

    // Empty needle
    ASSERT_TRUE(ss_rfind(ss, SS(""), 5) == 5);

    // Needle larger than haystack
    ASSERT_TRUE(ss_rfind(SS("hi"), SS("hello"), 10) == -1);
}

CTEST(slice, find_cstr) {
    StringSlice ss = ss_from_cstr("foo bar baz foo");

    ASSERT_TRUE(ss_find_cstr(ss, "foo", 0) == 0);
    ASSERT_TRUE(ss_find_cstr(ss, "foo", 1) == 12);
    ASSERT_TRUE(ss_find_cstr(ss, "bar", 0) == 4);
    ASSERT_TRUE(ss_find_cstr(ss, "quux", 0) == -1);
}

CTEST(slice, rfind_cstr) {
    StringSlice ss = ss_from_cstr("foo bar baz foo");

    ASSERT_TRUE(ss_rfind_cstr(ss, "foo", ss.size) == 12);
    ASSERT_TRUE(ss_rfind_cstr(ss, "foo", 11) == 0);
    ASSERT_TRUE(ss_rfind_cstr(ss, "bar", ss.size) == 4);
    ASSERT_TRUE(ss_rfind_cstr(ss, "quux", ss.size) == -1);
}

CTEST(slice, basename) {
    // Simple absolute path
    ASSERT_TRUE(ss_eq(ss_basename(SS("/usr/local/bin/test")), SS("test")));
    // Relative path
    ASSERT_TRUE(ss_eq(ss_basename(SS("src/main.c")), SS("main.c")));
    // Trailing slash
    ASSERT_TRUE(ss_eq(ss_basename(SS("/usr/local/")), SS("local")));
    // No separator — returns whole string
    ASSERT_TRUE(ss_eq(ss_basename(SS("hello.txt")), SS("hello.txt")));
    // Root
    ASSERT_TRUE(ss_basename(SS("/")).size == 0);
    // Multiple trailing slashes
    ASSERT_TRUE(ss_eq(ss_basename(SS("/usr/local///")), SS("local")));

#ifdef EXT_WINDOWS
    // Windows-style backslash separators
    ASSERT_TRUE(ss_eq(ss_basename(SS("C:\\Users\\test\\file.txt")), SS("file.txt")));
    ASSERT_TRUE(ss_eq(ss_basename(SS("C:\\Users\\test\\")), SS("test")));
    // Mixed separators
    ASSERT_TRUE(ss_eq(ss_basename(SS("C:\\Users/test/file.txt")), SS("file.txt")));
#endif
}

CTEST(slice, dirname) {
    // Simple absolute path
    ASSERT_TRUE(ss_eq(ss_dirname(SS("/usr/local/bin/test")), SS("/usr/local/bin")));
    // Single level
    ASSERT_TRUE(ss_eq(ss_dirname(SS("/test")), SS("/")));
    // Trailing slash
    ASSERT_TRUE(ss_eq(ss_dirname(SS("/usr/local/")), SS("/usr")));
    // No separator — returns empty
    ASSERT_TRUE(ss_dirname(SS("hello.txt")).size == 0);
    // Root
    ASSERT_TRUE(ss_eq(ss_dirname(SS("/")), SS("/")));
    // Relative path
    ASSERT_TRUE(ss_eq(ss_dirname(SS("src/main.c")), SS("src")));

#ifdef EXT_WINDOWS
    // Windows-style backslash separators
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users\\test\\file.txt")), SS("C:\\Users\\test")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\file.txt")), SS("C:")));
    // Mixed separators
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users/test/file.txt")), SS("C:\\Users/test")));
#endif
}

CTEST(slice, extension) {
    // Normal file extension
    ASSERT_TRUE(ss_eq(ss_extension(SS("/path/to/file.txt")), SS(".txt")));
    // Multiple dots
    ASSERT_TRUE(ss_eq(ss_extension(SS("archive.tar.gz")), SS(".gz")));
    // No extension
    ASSERT_TRUE(ss_extension(SS("/path/to/Makefile")).size == 0);
    // Dotfile (no extension)
    ASSERT_TRUE(ss_extension(SS("/home/user/.gitignore")).size == 0);
    // Dotfile with extension
    ASSERT_TRUE(ss_eq(ss_extension(SS(".config.json")), SS(".json")));
    // Trailing slash, no ext
    ASSERT_TRUE(ss_extension(SS("/path/to/dir/")).size == 0);

#ifdef EXT_WINDOWS
    // Windows-style paths
    ASSERT_TRUE(ss_eq(ss_extension(SS("C:\\Users\\test\\file.txt")), SS(".txt")));
    ASSERT_TRUE(ss_extension(SS("C:\\Users\\test\\Makefile")).size == 0);
#endif
}

CTEST(sb, append_path) {
    StringBuffer sb = {0};

    // Append to empty buffer
    sb_append_path_cstr(&sb, "usr");
    ASSERT_TRUE(memcmp(sb.items, "usr", sb.size) == 0);

    // Append with separator insertion
    sb_append_path_cstr(&sb, "local");
    ASSERT_TRUE(memcmp(sb.items, "usr/local", sb.size) == 0);

    // Append when buffer already ends with separator
    sb_append_char(&sb, '/');
    sb_append_path_cstr(&sb, "bin");
    ASSERT_TRUE(memcmp(sb.items, "usr/local/bin", sb.size) == 0);

    sb_free(&sb);

    // StringSlice variant
    StringBuffer sb2 = {0};
    sb_append_path(&sb2, SS("home"));
    sb_append_path(&sb2, SS("user"));
    ASSERT_TRUE(memcmp(sb2.items, "home/user", sb2.size) == 0);
    sb_free(&sb2);

#ifdef EXT_WINDOWS
    // Windows-style: backslash at end should not get an extra separator
    {
        StringBuffer sb3 = {0};
        sb_append_cstr(&sb3, "C:\\Users\\");
        sb_append_path_cstr(&sb3, "test");
        ASSERT_TRUE(memcmp(sb3.items, "C:\\Users\\test", sb3.size) == 0);
        sb_free(&sb3);
    }
#endif
}

#ifdef EXT_WINDOWS
CTEST(slice, windows_drive_letters) {
    // Basic drive letter paths
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\file.txt")), SS("C:")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("D:\\dir\\file.txt")), SS("D:\\dir")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:/file.txt")), SS("C:")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("E:/dir/file.txt")), SS("E:/dir")));

    // Drive letter only
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:")), SS("C:")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("D:")), SS("D:")));

    // Drive letter with trailing separator
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\")), SS("C:")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:/")), SS("C:")));

    // Multiple levels
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users\\test\\file.txt")), SS("C:\\Users\\test")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users\\test")), SS("C:\\Users")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users")), SS("C:")));
}

CTEST(slice, windows_unc_paths) {
    // Standard UNC paths
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\server\\share\\file.txt")), SS("\\\\server\\share")));
    ASSERT_TRUE(
        ss_eq(ss_dirname(SS("\\\\server\\share\\dir\\file.txt")), SS("\\\\server\\share\\dir")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\server\\share")), SS("\\\\server\\share")));

    // UNC path with trailing separator
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\server\\share\\")), SS("\\\\server\\share")));

    // Extended-length paths with drive letters
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\?\\C:\\file.txt")), SS("\\\\?\\C:")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\?\\C:\\dir\\file.txt")), SS("\\\\?\\C:\\dir")));

    // Device paths
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\.\\device\\file.txt")), SS("\\\\.\\device")));
    ASSERT_TRUE(ss_eq(ss_dirname(SS("\\\\.\\device")), SS("\\\\.\\device")));

    // Basename should work correctly with UNC paths
    ASSERT_TRUE(ss_eq(ss_basename(SS("\\\\server\\share\\file.txt")), SS("file.txt")));
    ASSERT_TRUE(ss_eq(ss_basename(SS("\\\\server\\share")), SS("share")));
}

CTEST(slice, windows_mixed_separators) {
    // Mixed forward and backslashes
    ASSERT_TRUE(ss_eq(ss_dirname(SS("C:\\Users/test\\file.txt")), SS("C:\\Users/test")));
    ASSERT_TRUE(ss_eq(ss_basename(SS("C:\\Users/test\\file.txt")), SS("file.txt")));

    // Extension should work with mixed separators
    ASSERT_TRUE(ss_eq(ss_extension(SS("C:\\Users/test\\file.txt")), SS(".txt")));
}

CTEST(sb, windows_path_separator_consistency) {
    // Test that sb_append_path uses consistent separators
    StringBuffer sb = {0};

    // Start with backslash style
    sb_append_cstr(&sb, "C:\\Windows");
    sb_append_path_cstr(&sb, "System32");
    ASSERT_TRUE(memcmp(sb.items, "C:\\Windows\\System32", sb.size) == 0);
    sb_free(&sb);

    // Start with forward slash style
    StringBuffer sb2 = {0};
    sb_append_cstr(&sb2, "C:/Windows");
    sb_append_path_cstr(&sb2, "System32");
    ASSERT_TRUE(memcmp(sb2.items, "C:/Windows/System32", sb2.size) == 0);
    sb_free(&sb2);

    // Multiple appends should maintain style
    StringBuffer sb3 = {0};
    sb_append_cstr(&sb3, "C:\\Users");
    sb_append_path_cstr(&sb3, "test");
    sb_append_path_cstr(&sb3, "Documents");
    ASSERT_TRUE(memcmp(sb3.items, "C:\\Users\\test\\Documents", sb3.size) == 0);
    sb_free(&sb3);
}
#endif

typedef struct {
    int key;
    int value;
} IntEntry;

typedef struct {
    IntEntry *entries;
    size_t *hashes;
    size_t size, capacity;
    Allocator *allocator;
} IntMap;

CTEST(hmap, get_put) {
    IntMap map = {0};
    for(int i = 0; i < 22; i++) {
        hmap_put(&map, i, i * 10);
    }

    ASSERT_TRUE(map.size == 22);
    hmap_put(&map, 2, 100);
    ASSERT_TRUE(map.size == 22);

    IntEntry *entry = hmap_get(&map, 2);
    ASSERT_TRUE(entry != NULL);
    ASSERT_TRUE(entry->value == 100);
    entry->value += 50;

    IntEntry *entry2 = hmap_get(&map, 2);
    ASSERT_TRUE(entry == entry2);
    ASSERT_TRUE(entry2->value == 150);

    hmap_free(&map);
}

CTEST(hmap, delete) {
    IntEntry *e;
    IntMap map = {0};

    for(int i = 1; i < 50; i++) {
        hmap_put(&map, i, i * 10);
    }

    e = hmap_get(&map, 1);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->value == 10);

    ASSERT_TRUE(map.size == 49);
    hmap_delete(&map, 1);
    ASSERT_TRUE(map.size == 48);
    e = hmap_get(&map, 1);
    ASSERT_TRUE(e == NULL);

    e = hmap_get(&map, 4);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->value == 40);

    for(int i = 2; i < 50; i++) {
        hmap_delete(&map, i);
    }

    for(int i = 2; i < 50; i++) {
        IntEntry *e = hmap_get(&map, i);
        ASSERT_TRUE(e == NULL);
    }
    ASSERT_TRUE(map.size == 0);

    hmap_free(&map);
}

CTEST(hmap, clear) {
    IntMap map = {0};
    for(int i = 0; i < 10; i++) {
        hmap_put(&map, i, i * 10);
    }
    ASSERT_TRUE(map.size == 10);
    hmap_clear(&map);
    ASSERT_TRUE(map.size == 0);
    for(int i = 0; i < 10; i++) {
        IntEntry *e = hmap_get(&map, i);
        ASSERT_TRUE(e == NULL);
    }

    hmap_put(&map, 3, 100);
    ASSERT_TRUE(map.size == 1);

    IntEntry *e = hmap_get(&map, 3);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->key == 3 && e->value == 100);

    hmap_clear(&map);
    ASSERT_TRUE(map.size == 0);

    hmap_free(&map);
}

CTEST(hmap, iter) {
    IntMap map = {0};
    for(int i = 0; i < 50; i++) {
        hmap_put(&map, i, i * 10);
    }
    int i = 0;
    for(IntEntry *it = hmap_begin(&map); it != hmap_end(&map); it = hmap_next(&map, it)) {
        i++;
    }
    ASSERT_TRUE(i == 50);

    i = 0;
    hmap_foreach(IntEntry, it, &map) {
        i++;
    }
    ASSERT_TRUE(i == 50);

    hmap_free(&map);
}

CTEST(hmap, allocator) {
    IntMap ints = {0};
    size_t temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail == 0);
    ints.allocator = &temp_allocator.base;
    for(int i = 0; i < 100; i++) {
        hmap_put(&ints, i, i * 10);
    }
    temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail >= 100 * sizeof(IntEntry));
    ASSERT_TRUE(allocated == 0);
    temp_reset();
}

CTEST(hmap, ctx_allocator) {
    Context ctx = *ext_context;
    ctx.alloc = &temp_allocator.base;
    push_context(&ctx);

    IntMap ints = {0};
    size_t temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail == 0);
    ints.allocator = &temp_allocator.base;
    for(int i = 0; i < 100; i++) {
        hmap_put(&ints, i, i * 10);
    }
    temp_avail = temp_allocator.end - temp_allocator.start;
    ASSERT_TRUE(temp_allocator.mem_size - temp_avail >= 100 * sizeof(IntEntry));
    ASSERT_TRUE(allocated == 0);

    pop_context();
    temp_reset();
}

typedef struct {
    const char *key;
    int value;
} StrEntry;

typedef struct {
    StrEntry *entries;
    size_t *hashes;
    size_t capacity, size;
    Allocator *allocator;
} StrMap;

CTEST(hmap, get_put_cstr) {
    StrMap map = {0};
    for(int i = 0; i < 22; i++) {
        hmap_put_cstr(&map, temp_sprintf("key %d", i), i * 10);
    }

    ASSERT_TRUE(map.size == 22);
    hmap_put_cstr(&map, "key 2", 100);
    ASSERT_TRUE(map.size == 22);

    StrEntry *entry = hmap_get_cstr(&map, "key 2");
    ASSERT_TRUE(entry != NULL);
    ASSERT_TRUE(entry->value == 100);
    entry->value += 50;

    StrEntry *entry2 = hmap_get_cstr(&map, "key 2");
    ASSERT_TRUE(entry == entry2);
    ASSERT_TRUE(entry2->value == 150);

    StrEntry *entry3 = hmap_get_cstr(&map, "key 30");
    ASSERT_TRUE(entry3 == NULL);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, delete_cstr) {
    StrEntry *e;
    StrMap map = {0};

    for(int i = 1; i < 50; i++) {
        hmap_put_cstr(&map, temp_sprintf("key %d", i), i * 10);
    }

    e = hmap_get_cstr(&map, "key 1");
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->value == 10);

    ASSERT_TRUE(map.size == 49);
    hmap_delete_cstr(&map, "key 1");
    ASSERT_TRUE(map.size == 48);
    e = hmap_get_cstr(&map, "key 1");
    ASSERT_TRUE(e == NULL);

    for(int i = 2; i < 50; i++) {
        hmap_delete_cstr(&map, temp_sprintf("key %d", i));
    }
    for(int i = 2; i < 50; i++) {
        const char *key = temp_sprintf("key %d", i);
        StrEntry *e = hmap_get_cstr(&map, key);
        ASSERT_TRUE(e == NULL);
    }
    ASSERT_TRUE(map.size == 0);

    hmap_free(&map);
    temp_reset();
}

typedef struct {
    StringSlice key;
    int value;
} SliceEntry;

typedef struct {
    SliceEntry *entries;
    size_t *hashes;
    size_t capacity, size;
    Allocator *allocator;
} SliceMap;

CTEST(hmap, get_put_ss) {
    SliceMap map = {0};
    for(int i = 0; i < 22; i++) {
        hmap_put_ss(&map, ss_from_cstr(temp_sprintf("key %d", i)), i * 10);
    }

    ASSERT_TRUE(map.size == 22);
    hmap_put_ss(&map, ss_from_cstr("key 2"), 100);
    ASSERT_TRUE(map.size == 22);

    SliceEntry *entry = hmap_get_ss(&map, ss_from_cstr("key 2"));
    ASSERT_TRUE(entry != NULL);
    ASSERT_TRUE(entry->value == 100);
    entry->value += 50;

    SliceEntry *entry2 = hmap_get_ss(&map, ss_from_cstr("key 2"));
    ASSERT_TRUE(entry == entry2);
    ASSERT_TRUE(entry2->value == 150);

    SliceEntry *entry3 = hmap_get_ss(&map, ss_from_cstr("key 30"));
    ASSERT_TRUE(entry3 == NULL);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, delete_ss) {
    SliceEntry *e;
    SliceMap map = {0};

    for(int i = 1; i < 50; i++) {
        hmap_put_ss(&map, ss_from_cstr(temp_sprintf("key %d", i)), i * 10);
    }

    e = hmap_get_ss(&map, ss_from_cstr("key 1"));
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->value == 10);

    ASSERT_TRUE(map.size == 49);
    hmap_delete_ss(&map, ss_from_cstr("key 1"));
    ASSERT_TRUE(map.size == 48);
    e = hmap_get_ss(&map, ss_from_cstr("key 1"));
    ASSERT_FALSE(e != NULL);

    for(int i = 2; i < 50; i++) {
        hmap_delete_ss(&map, ss_from_cstr(temp_sprintf("key %d", i)));
    }
    for(int i = 2; i < 50; i++) {
        SliceEntry *e = hmap_get_ss(&map, ss_from_cstr(temp_sprintf("key %d", i)));
        ASSERT_TRUE(e == NULL);
    }
    ASSERT_TRUE(map.size == 0);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, get_bytes) {
    HashMap(int, int) map = {0};

    // Empty map must return NULL without touching a NULL entries pointer.
    ASSERT_TRUE(hmap_get(&map, 1) == NULL);

    for(int i = 1; i <= 22; i++) {
        hmap_put(&map, i, i * 10);
    }

    // Hit: correct key and value, inline mutation visible on next lookup.
    ASSERT_TRUE(hmap_get(&map, 5) != NULL);
    ASSERT_TRUE(hmap_get(&map, 5)->key == 5);
    ASSERT_TRUE(hmap_get(&map, 5)->value == 50);
    hmap_get(&map, 5)->value = 99;
    ASSERT_TRUE(hmap_get(&map, 5)->value == 99);

    // Same key returns the same pointer (stable storage, no realloc triggered here).
    // Note: two get calls must not share an expression — both write to the tmp slot.
    void *p3a = hmap_get(&map, 3);
    void *p3b = hmap_get(&map, 3);
    ASSERT_TRUE(p3a == p3b);

    // Miss on absent key.
    ASSERT_TRUE(hmap_get(&map, 100) == NULL);

    hmap_free(&map);
}

CTEST(hmap, get_bytes_tombstones) {
    HashMap(int, int) map = {0};

    for(int i = 1; i <= 50; i++) {
        hmap_put(&map, i, i * 10);
    }

    // Create tombstones in the lower half.
    for(int i = 1; i <= 25; i++) {
        hmap_delete(&map, i);
    }

    // Deleted entries are not found (probe must continue past tombstones).
    for(int i = 1; i <= 25; i++) {
        ASSERT_TRUE(hmap_get(&map, i) == NULL);
    }

    // Entries whose probe chain passes through tombstones are still found.
    for(int i = 26; i <= 50; i++) {
        ASSERT_TRUE(hmap_get(&map, i) != NULL);
        ASSERT_TRUE(hmap_get(&map, i)->value == i * 10);
    }

    hmap_free(&map);
}

CTEST(hmap, get_default_bytes) {
    HashMap(int, int) map = {0};

    // First access on an empty map: triggers grow then inserts with default.
    hmap_get_default(&map, 1, 100)->value += 5;
    ASSERT_TRUE(map.size == 1);

    // Second access: key already present, default is ignored, value unchanged.
    ASSERT_TRUE(hmap_get_default(&map, 1, 0)->value == 105);
    ASSERT_TRUE(map.size == 1);

    // Accessing a key with a large default does not overwrite the stored value.
    (void)hmap_get_default(&map, 1, 999);
    ASSERT_TRUE(hmap_get(&map, 1)->value == 105);

    // Insert enough keys to trigger at least one grow.
    for(int i = 2; i <= 22; i++) {
        (void)hmap_get_default(&map, i, i * 10);
    }
    ASSERT_TRUE(map.size == 22);

    // After a grow the previously inserted key is accessible under its new address.
    ASSERT_TRUE(hmap_get(&map, 1)->value == 105);
    for(int i = 2; i <= 22; i++) {
        ASSERT_TRUE(hmap_get(&map, i) != NULL);
        ASSERT_TRUE(hmap_get(&map, i)->value == i * 10);
    }

    hmap_free(&map);
}

CTEST(hmap, get_default_bytes_tombstone_reuse) {
    HashMap(int, int) map = {0};

    for(int i = 1; i <= 20; i++) {
        hmap_put(&map, i, i);
    }

    for(int i = 1; i <= 10; i++) {
        hmap_delete(&map, i);
    }
    ASSERT_TRUE(map.size == 10);

    // Re-inserting deleted keys via get_default should reuse tombstone slots.
    for(int i = 1; i <= 10; i++) {
        (void)hmap_get_default(&map, i, i * 100);
    }
    ASSERT_TRUE(map.size == 20);

    for(int i = 1; i <= 10; i++) {
        ASSERT_TRUE(hmap_get(&map, i) != NULL);
        ASSERT_TRUE(hmap_get(&map, i)->value == i * 100);
    }

    hmap_free(&map);
}

CTEST(hmap, get_cstr) {
    HashMap(const char *, int) map = {0};

    ASSERT_TRUE(hmap_get_cstr(&map, "missing") == NULL);

    for(int i = 0; i < 22; i++) {
        hmap_put_cstr(&map, temp_sprintf("key %d", i), i * 10);
    }

    ASSERT_TRUE(hmap_get_cstr(&map, "key 3") != NULL);
    ASSERT_TRUE(hmap_get_cstr(&map, "key 3")->value == 30);
    hmap_get_cstr(&map, "key 3")->value = 77;
    ASSERT_TRUE(hmap_get_cstr(&map, "key 3")->value == 77);

    ASSERT_TRUE(hmap_get_cstr(&map, "key 99") == NULL);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, get_default_cstr) {
    HashMap(const char *, int) map = {0};

    // Word-count pattern.
    const char *words[] = {"foo", "bar", "foo", "baz", "foo", "bar"};
    for(size_t i = 0; i < 6; i++) {
        hmap_get_default_cstr(&map, words[i], 0)->value++;
    }

    ASSERT_TRUE(map.size == 3);
    ASSERT_TRUE(hmap_get_cstr(&map, "foo")->value == 3);
    ASSERT_TRUE(hmap_get_cstr(&map, "bar")->value == 2);
    ASSERT_TRUE(hmap_get_cstr(&map, "baz")->value == 1);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, get_ss) {
    HashMap(StringSlice, int) map = {0};

    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("missing")) == NULL);

    for(int i = 0; i < 22; i++) {
        hmap_put_ss(&map, ss_from_cstr(temp_sprintf("key %d", i)), i * 10);
    }

    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("key 7")) != NULL);
    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("key 7"))->value == 70);
    hmap_get_ss(&map, ss_from_cstr("key 7"))->value = 55;
    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("key 7"))->value == 55);

    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("key 99")) == NULL);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, get_default_ss) {
    HashMap(StringSlice, int) map = {0};

    // Word-count pattern — primary use case for get_default.
    const char *words[] = {"foo", "bar", "foo", "baz", "foo", "bar"};
    for(size_t i = 0; i < 6; i++) {
        hmap_get_default_ss(&map, ss_from_cstr(words[i]), 0)->value++;
    }

    ASSERT_TRUE(map.size == 3);
    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("foo"))->value == 3);
    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("bar"))->value == 2);
    ASSERT_TRUE(hmap_get_ss(&map, ss_from_cstr("baz"))->value == 1);

    hmap_free(&map);
    temp_reset();
}

CTEST(hmap, entry_macro_foreach) {
    HashMap(int, int) map = {0};

    for(int i = 0; i < 20; i++) {
        hmap_put(&map, i, i * 2);
    }

    // Entry(K, V) used as the type argument; valid because hmap_begin returns void*.
    int count = 0;
    int sum = 0;
    hmap_foreach(Entry(int, int), it, &map) {
        count++;
        sum += it->value;
    }
    ASSERT_TRUE(count == 20);
    ASSERT_TRUE(sum == 380);  // 2 * (0+1+...+19) = 2 * 190

    hmap_free(&map);
}

CTEST(defer, loop) {
    char *str = ext_strdup("hello, world");
    defer_loop((void)0, ext_free(str, 13)) {
        str[0] = 'H';
        str[7] = 'W';
        ASSERT_TRUE(strcmp(str, "Hello, World") == 0);
    }
}

static void sb_log(Ext_LogLevel lvl, void *data, const char *fmt, va_list ap) {
    StringBuffer *sb = (StringBuffer *)data;
    switch(lvl) {
    case EXT_DEBUG:
        sb_append_cstr(sb, "[DEBUG] ");
        break;
    case EXT_INFO:
        sb_append_cstr(sb, "[INFO] ");
        break;
    case EXT_WARNING:
        sb_append_cstr(sb, "[WARNING] ");
        break;
    case EXT_ERROR:
        sb_append_cstr(sb, "[ERROR] ");
        break;
    case EXT_NO_LOGGING:
        EXT_UNREACHABLE();
    }
    sb_appendvf(sb, fmt, ap);
}

CTEST(logging, log_level) {
    StringBuffer sb = {0};
    Context ctx = *ext_context;
    ctx.log_data = (void *)&sb;
    ctx.log_fn = &sb_log;
    push_context(&ctx);

    ext_log(INFO, "info");
    ASSERT_TRUE(sb.size > 0 && memcmp("[INFO] info", sb.items, sb.size) == 0);
    sb.size = 0;

    ext_log(WARNING, "warning");
    ASSERT_TRUE(sb.size > 0 && memcmp("[WARNING] warning", sb.items, sb.size) == 0);
    sb.size = 0;

    ext_log(ERROR, "error");
    ASSERT_TRUE(sb.size > 0 && memcmp("[ERROR] error", sb.items, sb.size) == 0);
    sb.size = 0;

    ctx.log_level = WARNING;

    ext_log(INFO, "info");
    ASSERT_TRUE(sb.size == 0);

    ext_log(WARNING, "warning");
    ASSERT_TRUE(sb.size > 0 && memcmp("[WARNING] warning", sb.items, sb.size) == 0);
    sb.size = 0;

    ext_log(ERROR, "error");
    ASSERT_TRUE(sb.size > 0 && memcmp("[ERROR] error", sb.items, sb.size) == 0);
    sb.size = 0;

    ctx.log_level = ERROR;

    ext_log(INFO, "info");
    ASSERT_TRUE(sb.size == 0);

    ext_log(WARNING, "warning");
    ASSERT_TRUE(sb.size == 0);

    ext_log(ERROR, "error");
    ASSERT_TRUE(sb.size > 0 && memcmp("[ERROR] error", sb.items, sb.size) == 0);
    sb.size = 0;

    ctx.log_level = NO_LOGGING;

    ext_log(INFO, "info");
    ASSERT_TRUE(sb.size == 0);

    ext_log(WARNING, "warning");
    ASSERT_TRUE(sb.size == 0);

    ext_log(ERROR, "error");
    ASSERT_TRUE(sb.size == 0);

    sb_free(&sb);
    pop_context();
}

CTEST(io, read_write_file) {
    const char content[] = "Cantami, o Diva,\nDel Pelide Achille\n";
    bool res = write_file("./test/out.txt", content, sizeof(content) - 1);
    ASSERT_TRUE(res);

    StringBuffer sb = {0};
    res = read_file("./test/out.txt", &sb);
    ASSERT_TRUE(res);
    ASSERT_TRUE(sb.size == sizeof(content) - 1);
    ASSERT_TRUE(memcmp(content, sb.items, sizeof(content) - 1) == 0);

    sb_free(&sb);
}

CTEST(io, read_line) {
    const char *content[] = {
        "Cantami, o Diva,\n",
        "Del Pelide Achille\n",
    };

    FILE *f = fopen("./test/out.txt", "rb");
    ASSERT_TRUE(f != NULL);

    int line = 0;
    StringBuffer sb = {0};
    int res;
    while((res = read_line(f, &sb)) > 0) {
        ASSERT_TRUE(memcmp(content[line], sb.items, sb.size) == 0);
        sb.size = 0;
        line++;
    }
    ASSERT_FALSE(res < 0 && ferror(f) != 0);
    ASSERT_TRUE(res == 0);
    fclose(f);
    sb_free(&sb);
}

CTEST(io, get_file_type) {
    LOGGING_LEVEL(NO_LOGGING) {
        FileType t = get_file_type("doesnotexist");
        ASSERT_TRUE(t == FILE_ERR);
        t = get_file_type("extlib.h");
        ASSERT_TRUE(t == FILE_REGULAR);
        t = get_file_type("test");
        ASSERT_TRUE(t == FILE_DIR);
    }
}

CTEST(io, get_cwd) {
    char *cwd = get_cwd();
    ASSERT_TRUE(cwd != NULL);
    ext_free(cwd, strlen(cwd) + 1);
}

CTEST(io, fs_operations) {
    char cantami[] = "Cantami, o Diva, del Pelide Achille";
    LOGGING_LEVEL(NO_LOGGING) {
        ASSERT_TRUE(write_file("./test/a.txt", cantami, sizeof(cantami) - 1));
        ASSERT_TRUE(rename_file("./test/a.txt", "./test/b.txt"));
        ASSERT_TRUE(delete_file("./test/b.txt"));
        ASSERT_TRUE(create_dir("./test/test2"));
        ASSERT_TRUE(write_file("./test/test2/cose.txt", "abc", 3));
        ASSERT_TRUE(rename_file("./test/test2", "./test/test3"));
        ASSERT_TRUE(delete_dir_recursively("./test/test3"));
    }
}
