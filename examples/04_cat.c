/**
 * Example: cat
 * Reads a file and dumps it to stdout, along with line numbers.
 * Uses a variety of extlib's io functions to implement its functionality.
 */

#include <stdio.h>

#define EXTLIB_IMPL
#include "../extlib.h"

int main(int argc, char **argv) {
    if(argc <= 1) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    StringBuffer sb = {0};
    if(!read_file(filename, &sb)) return 1;

    int lineno = 1;
    StringSlice ss = sb_to_ss(sb);
    while(ss.size) {
        StringSlice line = ss_split_once(&ss, '\n');
        printf("%02d: " SS_Fmt "\n", lineno, SS_Arg(line));
        lineno++;
    }

    sb_free(&sb);
}
