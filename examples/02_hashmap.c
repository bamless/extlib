/**
 * Example: hashmap
 * Counts the frequency of words in a given file or stdin and prints the result sorted in descending
 * order.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTLIB_IMPL
#include "../extlib.h"

#define HIST_DEF 80

#define shift(argc, argv) ((argc)--, *(argv)++)

typedef struct {
    StringSlice key;
    size_t value;
} WordFreq;

typedef struct {
    WordFreq *entries;
    size_t *hashes;
    size_t size, capacity;
    Allocator *allocator;
} WordMap;

typedef struct {
    WordFreq *items;
    size_t size, capacity;
    Allocator *allocator;
} WordArray;

static const char *symbols = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
static bool reverse;

static int qsort_cmp(const void *a, const void *b) {
    const WordFreq *e1 = a, *e2 = b;
    if(e1->value < e2->value) return reverse ? -1 : 1;
    if(e1->value > e2->value) return reverse ? 1 : -1;
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "USAGE: %s [OPTIONS] [path]\n", prog);
    fprintf(stderr, "OPTIONS\n");
    fprintf(stderr,
            "  -h <number> print word frequency as a histogram. <number> is the maximum column "
            "width, and must be > 0\n");
    fprintf(stderr, "  -r          print the statistics in reverse order (ascending)\n");
}

int main(int argc, char **argv) {
    char *prog = shift(argc, argv);
    int hist = 0;

    int npos = 0;
    for(int i = 0; i < argc; i++) {
        char *arg = argv[i];
        if(strcmp("--", arg) == 0) {
            for(int j = i + 1; j < argc; j++) argv[npos++] = argv[j];
            break;
        }

        size_t arglen = strlen(arg);
        if(arglen <= 1 || *arg != '-') {
            argv[npos++] = arg;
            continue;
        }

        char opt = arg[1];
        switch(opt) {
        case 'h':
            hist = HIST_DEF;
            if(i + 1 >= argc) break;  // last argument

            char *endptr;
            long ncols = strtol(argv[i + 1], &endptr, 10);
            if(*endptr != '\0') break;  // next argument is not an integer; stick with default cols
            i++;

            if(ncols <= 0) {
                fprintf(stderr, "'-h' must be > 0\n");
                usage(prog);
                return 1;
            }

            hist = (int)ncols;
            break;
        case 'r':
            reverse = true;
            break;
        default:
            fprintf(stderr, "Unknown option '%c'\n", opt);
            usage(prog);
            return 1;
        }
    }
    argc = npos;

    StringBuffer file = {0};
    if(argc > 0) {
        if(!read_file(argv[0], &file)) {
            usage(prog);
            return 1;
        }
    } else {
        int res;
        while((res = read_line(stdin, &file)) > 0);
        if(res < 0) return 1;
    }
    sb_replace(&file, 0, symbols, ' ');

    WordMap words_freq = {0};
    StringSlice file_slice = sb_to_ss(file);
    while(file_slice.size) {
        StringSlice word = ss_split_once_ws(&file_slice);
        if(word.size == 0) continue;

        WordFreq *e = hmap_get_default_ss(&words_freq, word, 0);
        ASSERT(e != NULL, "default entry shouldn't be NULL");

        e->value++;
    }

    size_t max_freq = 0;
    size_t max_word_len = 0;
    WordArray sorted_freqs = {0};
    hmap_foreach(WordFreq, e, &words_freq) {
        if(e->value > max_freq) max_freq = e->value;
        if(e->key.size > max_word_len) max_word_len = e->key.size;
        array_push(&sorted_freqs, *e);
    }
    qsort(sorted_freqs.items, sorted_freqs.size, sizeof(*sorted_freqs.items), qsort_cmp);

    if(hist) {
        array_foreach(WordFreq, e, &sorted_freqs) {
            int ncols = hist * e->value / max_freq;
            if(!ncols) ncols = 1;
            for(int i = 0; i < ncols; i++) {
                printf("#");
            }
            printf("%*.*s %*zu", (int)(hist - ncols + e->key.size + 1), SS_Arg(e->key),
                   (int)(max_word_len - e->key.size + 1), e->value);
            printf("\n");
        }
    } else {
        array_foreach(WordFreq, e, &sorted_freqs) {
            printf("%zu " SS_Fmt "\n", e->value, SS_Arg(e->key));
        }
    }

    sb_free(&file);
    array_free(&sorted_freqs);
    hmap_free(&words_freq);
    return 0;
}
