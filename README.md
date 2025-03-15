# extlib: c extended library

extlib is a collection of libraries that implement common data structures and algorithms that are
not provided by the c standard library.
More specifically, it's a collection of various code that I've written throughout the years to make
writing code in c faster and easier, by providing tools that are in my eyes essential to any
programming language.

Every algorithm implemented in the library strives to be as simple as possible, but not any simpler.
Common optimizations and patterns are used where deemed necessary while mantaining the cleanest
and simplest implementation possible, making the library easly comprehensible and modifiable.
In the following I'll give a brief introdution of the various components of the library, for more
examples check out the [examples/example.c](https://github.com/bamless/extlib/blob/master/examples/example.c)
file.

> NOTE: The library by default exports all symbols without any prefix. If you want find any conflicts
> when using in your project, you can define `EXT_LIB_NO_SHORTHANDS` before including any of the
> library headers to disable names without prefixes.

## ext_vector

**ext_vector** is an implementation of a dynamic, growable and type-safe array, that is simple to
use and completely compatible with plain c arrays.

### Basic usage

```c
    #include "extlib/vector.h"

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
#include "extlib/string.h"

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
#include "extlib/map.h"

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
    // Let's create a new map
    ext_map* map = ext_map_new(sizeof(Entry), hash, compare);

    // Let's add some entries to it:
    ext_map_put(map, &(Entry){.name = "Entry 1", .data = 10});
    ext_map_put(map, &(Entry){.name = "Entry 2", .data = 20});
    ext_map_put(map, &(Entry){.name = "Entry 3", .data = 30});
    ext_map_put(map, &(Entry){.name = "Entry 4", .data = 40});

    // ext_map_put will copy the data at the address pointed to in the second argument

    // Let's retrieve an entry back:
    const Entry* e = ext_map_get(map, &(Entry){.name = "Entry 2"});
    if(e) { // Did we find it?
        printf("%s: %d", e->name, e->data);
    }

    // Note how in the call to ext_map_get the provided Entry has only its name field initialized.
    // This is because our chosen key in Entry is the `name` field (as expressed in the hash and
    // compare function), and the other fields will not be accessed during the ext_map_get call.
    // Thus, it's safe for them to be default-initialized.

    // Iterate through the map using iterators:
    for(const Entry* it = ext_map_begin(map); it != ext_map_get(map); it = ext_map_incr(map, it)) {
        printf("%s: %d", it->name, it->data);
    }

    // Finally free the map
    ext_map_free(map);
}
```

### Implementation details

**ext_map** is the data structure implemented in the most classical way of the bunch.
`ext_map_new` will return an opaque pointer to a map struct that will have space for storing items
of size `entry_sz` (the first argument you pass to it). All the other functions take in `void*`
pointers to the entries, and for this reason its functions are not type-safe like the ones of
**ext_vector**.
As for the flavour of hashtable implemented, it is an open-adressing based one. The implementation
will keep track of two parallel arrays, one for entries and one for buckets. The first one is used
to store the data provided on put, and the second to cache its hash and to keep track of tombstones.
The map will grow when its fill ratio (including tombstones) reaches a certain percentage  (75% is
the default).
This makes the map really efficient to iterate over, as all the data is kept in a contiguous array
in memory.

## extlib/assert.h

the `assert.h` headers contains macros for better debug assertions and unreachable code.
`ASSERT` will print the file, line, function and a custom message if the condition is not satisfied,
and then `abort`s the program.

`UNREACHABLE` will do the same thing when executed, useful to check that a logically unreachable
piece of code is actually never reached during execution.

An additional macro called `UNUSED` is provided to hint the compiler that a given varible is not
actually used in the program. This is useful when a variable is only used in assertions which, when
elided on release builds, leave the variable unused prompting the compiler to issue a warning.
