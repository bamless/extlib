/**
 * Example: ls
 * lits the content of a directorie(s) to stodout, similarly to the ls command.
 * Uses extlib's directory io functions to implement its functionalities.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define EXTLIB_IMPL
#include "../extlib.h"

#define shift(argc, argv) ((argc)--, *(argv)++)
#define unshift(argc, argv, elem)                            \
    do {                                                     \
        argc++, argv--;                                      \
        memmove(argv, argv + 1, (argc - 1) * sizeof(*argv)); \
        argv[argc - 1] = (elem);                             \
    } while(0)

static bool list_dir(const char* dir, bool all, bool recursive) {
    Paths paths = {0};
    if(!read_dir(dir, &paths)) return false;

    bool all_ok = true;
    array_foreach(char*, it, &paths) {
        if(!all && **it == '.') continue;

        void* checkpoint = temp_checkpoint();
        char* abs;
        if(dir[strlen(dir) - 1] == '/') {
            abs = temp_sprintf("%s%s", dir, *it);
        } else {
            abs = temp_sprintf("%s/%s", dir, *it);
        }

        if(recursive && get_file_type(abs) == FILE_DIR) {
            all_ok &= list_dir(abs, all, recursive);
        }

        printf("%s\n", abs);
        temp_rewind(checkpoint);
    }

    free_paths(&paths);
    return all_ok;
}

static void usage(const char* prog) {
    fprintf(stderr, "USAGE: %s [OPTIONS] path...\n", prog);
    fprintf(stderr, "OPTIONS\n");
    fprintf(stderr, "  -R recurse into directories\n");
    fprintf(stderr, "  -a shows hidden 'dot' files\n");
}

int main(int argc, char** argv) {
    char* prog = shift(argc, argv);
    bool all = false, recursive = false;

    int optn = 0;
    int nargs = argc;
    while(optn++ < nargs) {
        char* arg = shift(argc, argv);
        if(strcmp("--", arg) == 0) break;

        size_t arglen = strlen(arg);
        if(arglen <= 1 || *arg != '-') {
            unshift(argc, argv, arg);
            continue;
        }

        shift(arglen, arg);
        while(arglen) {
            char opt = shift(arglen, arg);
            switch(opt) {
            case 'R':
                recursive = true;
                break;
            case 'a':
                all = true;
                break;
            default:
                fprintf(stderr, "Unknown option '%c'\n", opt);
                usage(prog);
                return 1;
            }
        }
    }

    if(argc != 0) {
        int res = 0;
        bool print_header = argc > 1;
        while(argc) {
            const char* path = shift(argc, argv);
            if(print_header) printf("%s:\n", path);
            if(!list_dir(path, all, recursive)) res = 1;
            if(argc > 0) printf("\n");
        }
        return res;
    } else {
        if(!list_dir(".", all, recursive)) return 1;
    }
}
