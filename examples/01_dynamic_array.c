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

// Could also create this typedef with `Array` utility macro:
//     typedef Array(StringSlice) Lines;
typedef struct {
    StringSlice *items;
    size_t size, capacity;
    Allocator *allocator;
} Lines;

static int qsort_cmp(const void *a, const void *b) {
    return ss_cmp(ss_trim(*(StringSlice *)a), ss_trim(*(StringSlice *)b));
}

int main(void) {
    // Could also define this dynamic array inline with:
    //     Array(StringSlice) lines = {0};
    Lines lines = {0};
    StringBuffer file = {0};

    int res;
    while((res = read_line(stdin, &file)) > 0);
    if(res < 0) return 1;

    StringSlice ss = sb_to_ss(file);
    while(ss.size) {
        StringSlice line = ss_split_once(&ss, '\n');
        if(ss_trim(line).size == 0) continue;
        ext_array_push(&lines, line);
    }

    qsort(lines.items, lines.size, sizeof(*lines.items), qsort_cmp);
    array_foreach(StringSlice, line, &lines) {
        printf(SS_Fmt "\n", SS_Arg(*line));
    }

    array_free(&lines);
    sb_free(&file);
    return 0;
}
