/**
 * Example: Dynamic arrays
 * Collects all lines from stdin into a dynamic array, sorts them and prints the result to stdout.
 * Similar to the `sort` command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTLIB_IMPL
#include "../extlib.h"

typedef struct {
    char** items;
    size_t size, capacity;
    Allocator* allocator;
} Lines;

static int qsort_strcmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int main(void) {
    Lines lines = {0};
    StringBuffer line = {0};

    int res;
    while((res = read_line(stdin, &line)) > 0) {
        array_push(&lines, sb_to_cstr(&line));
    }
    if(res < 0) return 1;

    qsort(lines.items, lines.size, sizeof(*lines.items), qsort_strcmp);
    ext_array_foreach(char*, it, &lines) {
        printf("%s", *it);
    }

    sb_free(&line);
    ext_array_foreach(char*, it, &lines) {
        ext_free(*it, strlen(*it) + 1);
    }
    array_free(&lines);
    return 0;
}
