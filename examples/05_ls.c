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

static bool list_dir(const char* dir, bool all, bool recursive) {
    Paths paths = {0};
    if(!read_dir(dir, &paths)) return false;

    bool ok = true;
    array_foreach(char*, it, &paths) {
        if(!all && **it == '.') continue;
        void* temp = temp_checkpoint();

        char* full;
        if(dir[strlen(dir) - 1] == '/') {
            full = temp_sprintf("%s%s", dir, *it);
        } else {
            full = temp_sprintf("%s/%s", dir, *it);
        }

        if(recursive && get_file_type(full) == FILE_DIR) {
            ok &= list_dir(full, all, recursive);
        }

        printf("%s\n", full);
        temp_rewind(temp);
    }

    free_paths(&paths);
    return ok;
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

    int npos = 0;
    for(int i = 0; i < argc; i++) {
        char* arg = argv[i];
        if(strcmp("--", arg) == 0) {
            for(int j = i + 1; j < argc; j++) argv[npos++] = argv[j];
            break;
        }

        size_t arglen = strlen(arg);
        if(arglen <= 1 || *arg != '-') {
            argv[npos++] = arg;
            continue;
        }

        char* flags = arg + 1;
        while(*flags) {
            switch(*flags++) {
            case 'R':
                recursive = true;
                break;
            case 'a':
                all = true;
                break;
            default:
                fprintf(stderr, "Unknown option '%c'\n", flags[-1]);
                usage(prog);
                return 1;
            }
        }
    }
    argc = npos;

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
