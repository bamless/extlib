# extlib: c extended library

[![Build](https://github.com/bamless/extlib/actions/workflows/build.yml/badge.svg)](https://github.com/bamless/extlib/actions/workflows/build.yml)

extlib is a header-only library that implements common data structures and algorithms that are not
provided by the c standard library.

Features:
- Context abstraction to easily modify the application's allocation and logging behaviour
- Custom Allocators that easily integrate with the context and datastructures
- Generic, type-safe dynamic arrays and hash maps
- StringBuffer for easily working with dynamic buffers of bytes
- StringSlice for easy manipulation of immutable 'views' over byte buffers
- Cross-platform IO functions for working with processes, files and more generally
  interacting with the operating system
- Configurable logging
- Misc utility macros
- No-std and wasm support

## Quickstart

 To use the library do this in *exactly one* c file:
 ```c
 #define EXTLIB_IMPL
 // Optional configuration options
 #include "extlib.h"
 ```

> NOTE: The library by default exports all symbols without any prefix. If you find any conflicts
> when using in your project, you can define `EXTLIB_NO_SHORTHANDS` before including any of the
> library headers to disable names without prefixes.

 Configuration options:
 ```c
#define EXTLIB_NO_SHORTHANDS // Disable shorthands names, only prefixed one will be defined
#define EXTLIB_NO_STD        // Do not use libc functions
#define EXTLIB_WASM          // Enable when compiling for wasm target. Implies EXTLIB_NO_STD
#define EXTLIB_THREADSAFE    // Thread safe Context
#define NDEBUG               // Strips runtime assertions and replaces unreachables with compiler
                             // intrinsics
 ```

## Examples

You can find a bunch of examples showcasing library features in `examples/`

### Minimal example

```c
#include <stddef.h>
#include <stdio.h>
#define EXTLIB_IMPL
#include "extlib.h"

typedef struct {
    StringSlice key;
    int value;
} WordFreq;

// Could also create this typedef with `HashMap` utility macro:
//     typedef HashMap(StringSlice, int) WordMap;
typedef struct {
    WordFreq* entries;
    size_t* hashes;
    size_t size, capacity;
    Allocator* allocator;
} WordMap;

// Could also create this typedef with `Array` utility macro:
//     typedef Array(StringSlice) Words;
typedef struct {
    StringSlice* items;
    size_t size, capacity;
    Allocator* allocator;
} Words;

int main(int argc, char** argv) {
    // Push context with temp allocator so that we don't need to free
    Context ctx = *ext_context;
    ctx.alloc = &temp_allocator.base;
    push_context(&ctx);

    Words words = {0};
    // Could also specify allocators on a per-datastructure level
    // words.allocator = &default_allocator.base
    
    int i;
    for(i = 1; i < argc; i++) {
        StringSlice line = ss_from_cstr(argv[i]);
        while(line.size) {
            StringSlice word = ss_split_once_ws(&line);
            array_push(&words, word);
        }
    }
    ext_log(INFO, "Processed %d lines", i-1);

    printf("Words:\n");
    array_foreach(StringSlice, it, &words) {
        printf("  " SS_Fmt "\n", SS_Arg(*it));
    }

    WordMap word_freqs = {0};
    array_foreach(StringSlice, it, &words) {
        WordFreq* e = hmap_get_default_ss(&word_freqs, *it, 0);
        e->value++;
    }

    printf("Frequency:\n");
    hmap_foreach(WordFreq, it, &word_freqs) {
        printf("  " SS_Fmt ": %d\n", SS_Arg(it->key), it->value);
    }

    pop_context();
}
```

## Tests
You can find tests in `test/test.c`. To run them:
```sh
make test
```
