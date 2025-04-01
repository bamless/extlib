# extlib: c extended library

extlib is a library that implement common data structures and algorithms that are not provided by
the c standard library.
More specifically, it's a collection of various code that I've written throughout the years to make
writing code in c faster and easier, by providing tools that are in my eyes essential to any
programming language.

Every algorithm implemented in the library strives to be as simple as possible, but not any simpler.
Common optimizations and patterns are used where deemed necessary while mantaining the cleanest
and simplest implementation possible, making the library easly comprehensible and modifiable.
In the following I'll give a brief introdution of the various components of the library, for more
examples check out the [examples/example.c](https://github.com/bamless/extlib/blob/master/examples/example.c)
file.

> NOTE: The library by default exports all symbols without any prefix. If you find any conflicts
> when using in your project, you can define `EXTLIB_NO_SHORTHANDS` before including any of the
> library headers to disable names without prefixes.

# How to use in your project

This is an header-only library, so you can simply include the header in your project and use it.  
You will also need to define `EXTLIB_IMPLEMENTATION` in exactly one `c` file to include function
implementations.

# Libraries

## ext_vector

**ext_vector** is an implementation of a dynamic, growable and type-safe array, that is simple to
use and completely compatible with plain c arrays.

### Basic usage

```c
    #include "extlib.h"

    // Let's declare an vector:
    vector(int) vec = NULL;

    // Or, equivalently:
    int* vec = NULL;

    // The vector(T) macro simply expands to T* and is only useful to visually distinguish an
    // vector from a normal c array.
    // The initialization to NULL is necessary, as a NULL vector is a valid vector, the empty one.

    // Append elements:
    vec_push_back(vec, 5);  // This'll copy `5` at the end of the vector, resizing it if necessary
    vec_push_back(vec, 10); // Same thing for `10`

    printf("%d\n", vec[0]); // We can index directly in the vector!

    // Easy iteration using helper macro:
    vec_foreach(int* it, vec) {
        printf("%d\n", *it);
    }

    // Iterating using iterator functions, actually equivalent to the above:
    for(int* it = vec_begin(vec); it != vec_end(vec); it++) {
        printf("%d\n", *it);
    }

    // Finally free the vector
    vec_free(vec);
```

### Impementation details

**ext_vector** uses the common trick of storing extra information before the pointer provided to the
user. This allows keeping track of both the capacity and the size of the vector without resorting
on using a struct. This is what allows full compatibility with plain c arrays and the possibility of
indexing the vector with the `[]` operator.
Another intresting point in the implementation of **ext_vector** is that it is fully implemented in
preprocessor macros. The macros are basically used as poor man's templates and are what allow its
type safety.
For example doing something like this will result in a compiler error:
```c
int* vec = NULL;
ext_vec_push_back(vec, "not an int") // assignment to ‘int’ from ‘char *’ makes integer from pointer without a cast
```

## ext_string
**ext_string** is an implementation of a dynamic and growable string that is compatible with normal
c-like char* strings.

> NOTE: **ext_string** depends on **ext_vector** for some optional functions (`ext_str_split` and
> `ext_str_split_free`). If you haven't included **ext_vector** in your project, you can define
> `EXT_LIB_NO_VECTOR` before including **ext_string** to disable these functions. This is done
> automatically in GCC/Clang.

### Basic usage

```c
#define EXTLIB_IMPLEMENTATION
#include "extlib.h"

// Let's create a new string:
string str = str_new("New string!");

// Or, equivalently:
char* str = str_new("New string!");

// string is a typedef to char*, the only reason in using it is to visually distinguish
// strings from plain c char* strings.

printf("The first character is %c\n", str[0]); // Note how we can index directly into the string!

// As a plus, strings are always NUL terminated - even thought they keep track of an explicit
// length - in order to make them compatible with c-strings

puts(str); // This will work as expected!

// strings are mutable, and new content can be appended to them
string_append(&string, " Another string");

// Note how the above function takes in a pointer to the string (or, expanding the typedef,
// a char**). This will be the case for every function that needs to possibly grow the string, and
// the reason for it is that, in case of a reallocation, we need to update the pointer of the caller
// or otherwise we'll create a dangling reference.

// strings are heap-allocated, and need to be freed
str_free(str);
```

### Implementation details

**ext_string** uses the same trick of storing extra data before the pointer as **ext_vector** does.
This allows full compatibility with c-strings and the ability to directly index in the string with
`[]`.
Unlike **ext_vector** though, **ext_string** is not fully implemented in macros, as we do not need
to handle data of different types.

## ext_map

**ext_map** is an implementation of a hashmap data structure that can store arbitrary data.

### Basic usage

```c
#define EXTLIB_IMPLEMENTATION
#include "extlib.h"

typedef struct Entry {
    const char* name;
    int data;
}

static void hash(const void* entry) {
    const Entry* e = entry;
    // Generic hashing function, but you can provde your own
    return ext_map_hash_bytes(e->name, strlen(e->name));
}

static void compare(const void* entry1, const void* entry2) {
    const Entry *e1 = entry, *e2 = entry2;
    // `== 0` is important as strcmp will return 0 on equality, which is falsy in c!
    return strcmp(e1->name, e2->name) == 0;
}

// Note how in the above functions we return the hash and we compare only the `name` field of the
// Entry struct. This is how we express that the `name` field should be used as the key, whilst the
// others contain the associated data

int main(void) {
    printf("\n---- map examples ----\n");

    // Create a new map that will contain items of the `Entry` struct
    map map;
    map_init(&map, sizeof(Entry), hash, compare);

    // Set items in the map
    // The items are copied in the map on `put`
    map_put(&map, &(Entry){.name = "Entry 1", .value = 10});
    map_put(&map, &(Entry){.name = "Entry 2", .value = 20});
    map_put(&map, &(Entry){.name = "Entry 3", .value = 30});
    map_put(&map, &(Entry){.name = "Entry 4", .value = 40});
    map_put(&map, &(Entry){.name = "Entry 5", .value = 50});
    map_put(&map, &(Entry){.name = "Entry 6", .value = 60});
    map_put(&map, &(Entry){.name = "Entry 7", .value = 70});
    map_put(&map, &(Entry){.name = "Entry 8", .value = 80});


    // Retrieve an item from the map
    // Note that we initialize only the `name` field as it is our chosen key in the struct
    // On `get` the other fields are not accessed and thus it's fine for them to be default-initialized
    const Entry* e2 = map_get(&map, &(Entry){.name = "Entry 2"});
    if(e2) { // If the key isn't found `get` will return NULL
        printf("'%s' %d\n", e2->name, e2->value);
    }

    printf("map size: %zu\n", map_size(&map));

    // Iterate over all entries using iteator functions
    for(const Entry* it = map_begin(&map); it != map_end(&map); it = map_incr(&map, it)) {
        printf("'%s' %d\n", it->name, it->value);
    }

    map_free(&map);
}
```

### Implementation details

**ext_map** is the data structure implemented in the most classical way of the bunch.
`ext_map_init` will initialize a map struct that will store entries of size `entry_sz`. All other
functions will take in `void*` pointers to the entries, and for this reason its functions are not
type-safe like the ones of **ext_vector**.

`ext_map_init` also takes in two function pointers, `hash` and `compare`, that will be used to
compute the hash of the keys and to compare them respectively. See the example above for a simple
example of how to implement these functions.

As for the flavour of hashtable implemented, it is an open-adressing hash map with linear probing.
The implementation will keep track of two parallel arrays, one for entries and one for buckets. The
first one is used to store the data provided on put, and the second to cache its hash and to keep
track of tombstones. The map will grow when its fill ratio (including tombstones) reaches a certain
percentage  (75%). This makes the map really efficient to iterate over, as all the data is kept in a
contiguous array in memory (even though there might be holes caused by tombstones and hashing).

## Asserts and unreachable code

the `ext_assert.h` headers contains macros for better debug assertions and unreachable code.
`ASSERT` will print the file, line, function and a custom message if the condition is not satisfied,
and then `abort`s the program.

`UNREACHABLE` will do the same thing when executed, useful to check that a logically unreachable
piece of code is actually never reached during execution.

When compiling in release mode (i.e. with `-DNDEBUG`), the `ASSERT` macro will be elided, and the
`UNREACHABLE` macro will be replaced with a `__builtin_unreachable()` call, which will hint the
compiler that the code is unreachable.

The library also provides a `STATIC_ASSERT` macro that will be checked at compile time. This is
implemented using `static_assert` in C11, while on older versions of C it will use a trick
that will cause a compilation error if the condition is not satisfied.
