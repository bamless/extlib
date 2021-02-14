# extlib: c extended library

extlib is a collection of libraries that implement common data structures and algorithms that are
not provided by the c standard library.  
More specifically, it's is a collection of various code that I've written throughout the years to 
make writing code in c faster and easier, by providing tools that are in my eyes essential to any
programming language.  
Every algorithm implemented in the library strives to be as simple as possible, but not any simpler.
Common optimizations and patterns are used where deemed necessary, whilst mantaining the cleanest
and simplest implementation possible, making the library easly comprehensible and modifiable.  
In the following I'll give a brief introdution of the various components of the library, for more
examples check out the [examples/example.c](https://github.com/bamless/extlib/blob/master/examples/example.c)
file.

## ext_vector

**ext_vector** is an implementation of a dynamic, growable and type-safe array, that is simple to
use and completely compatible with plain c arrays.

### Basic usage

```c
    // Let's declare an ext_vector:
    ext_vector(int) vec = NULL;

    // Or, equivalently:
    int* vec = NULL;

    // The ext_vector(T) macro simply expands to T* and is only useful to visually distinguish an
    // ext_vector from a normal c array visually.
    // The initialization to NULL is necessary, as a NULL vector is a valid vector, the empty one.

    // Append elements:
    ext_vec_push_back(vec, 5);  // This'll copy `5` at the end of the vector, resizing it if necessary
    ext_vec_push_back(vec, 10); // Same thing for `10`

    printf("%d\n", vec[0]); // We can index directly in the vector!

    // Easy iteration using helper macro:
    ext_vec_foreach(int* it, vec) {
        printf("%d\n", *it);
    }

    // Iterating using iterator functions, actually equivalent to the above:
    for(int* it = ext_vec_begin(vec); it != ext_vec_end(vec); it++) {
        printf("%d\n", *it);
    }

    // Finally free the vector
    ext_vec_free(vec);
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
    ext_vector(int) vec = NULL;
    ext_vec_push_back(vec, "not an int") // assignment to ‘int’ from ‘char *’ makes integer from pointer without a cast
```

## ext_string
TODO

## ext_map
TODO

## extlib/assert.h
TODO