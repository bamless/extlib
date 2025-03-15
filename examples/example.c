#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ext_map.h"
#include "ext_string.h"
#include "ext_vector.h"

typedef struct Entity {
    const char* name;
    int x, y;
} Entity;

static void vector_example(void) {
    printf("\n---- vector examples ----\n");
    int integers[] = {1, 2, 3, 4, 5};

    // Vector of primitive type (ints)
    // the `vector(T)` macro simply subsitutes the given type with a pointer version of it,
    // so we could have used int* as well, but it's useful to distinguish between vectors
    // and other kind of buffers visually
    vector(int) vec = NULL; // must be initialized to NULL! Remember, a NULL vector is a valid vector, the empty one
    vec_push_back_all(vec, integers, sizeof(integers) / sizeof(int));

    // Can directly index int the vector!
    vec_insert(vec, 3, 100);
    assert(vec[3] == 100);

    vec_erase(vec, 3);
    assert(vec[3] == 4);

    vec_push_back(vec, 500);
    assert(vec_back(vec) == 500);

    // Iterate over the elements using iterator functions
    for(const int* it = vec_begin(vec); it != vec_end(vec); it++) {
        printf("%d\n", *it);
    }

    vec_free(vec);

    // Vector of structs (Entity)
    vector(Entity) entities = NULL;

    // The extra paranthesis around the Entity compound literal are needed for the macro to 
    // correctly parse the literal as a single element
    // The Entity struct will be copyed in the vector on `push_back`
    vec_push_back(entities, ((Entity){.name = "Entity 1", .x = 10,  .y = 20}));
    vec_push_back(entities, ((Entity){.name = "Entity 2", .x = 0,   .y = 100}));
    vec_push_back(entities, ((Entity){.name = "Entity 3", .x = 73,  .y = 11}));
    vec_push_back(entities, ((Entity){.name = "Entity 4", .x = 103, .y = 20}));

    vec_pop_back(entities);
    assert(vec_size(entities) == 3);

    // Can iterate with helper macro too, equivalent to iterator version above
    vec_foreach(const Entity* it, entities) {
        printf("%s: {%d, %d}\n", it->name, it->x, it->y);
    }

    vec_free(entities);
}

static void string_example(void) {
    printf("\n---- string examples ----\n");

    // Create a new string
    // char* would work too, but it's useful to use the `string` typedef to distinguish
    // between regular c-strings and strings
    string str = str_new("abc foo bar foobar");

    // Can index directly into the string!
    printf("the first character is: %c\n", str[0]);

    size_t pos = str_find(str, 0, "foo");
    if(pos != str_npos) {
        printf("Found 'foo' at position %zu\n", pos);
    }

    // Split returns a dynamic arrays of tokens
    printf("Tokens:\n");
    vector(string) tokens = str_split(str, ' ');
    vec_foreach(const string* it, tokens) {
        printf("%s\n", *it);
    }
    str_split_free(tokens);

    printf("Logarithmic growth on append for constant constant amortized time\n");
    printf("before append: size %zu\tcapacity %zu\n", str_size(str), str_capacity(str));

    // Note that functions that modify the string take a pointer to it, as they need to reassign 
    // the string in case of allocations that change the underlying pointer to the data
    str_append_fmt(&str, " number %d", 20);
    printf("%s\n", str);

    printf("after append: size %zu\tcapacity %zu\n", str_size(str), str_capacity(str));

    // Lexicographic compare
    string str2 = str_new("another string");
    int res = str_compare(str, str2);
    if(res == 0) {
        printf("`str` and `str2` are equal\n");
    } else if(res < 0) {
        printf("`str` is less than `str2`\n");
    } else if(res > 0) {
        printf("`str` is greater than `str2`\n");
    }

    str_free(str2);
    str_free(str);

    // string can hold arbitrary binary data
    string binary = str_new_len("foo\0test", 8);
    
    // But if they contain embedded NUL terminators you cannot use them in c standard library 
    // functions that don't take in an explicit length
    printf("incorrect length from standard library: %zu\n", strlen(binary));
    printf("correct length from str functions: %zu\n", str_size(binary));

    str_free(binary);
}

typedef struct Entry {
    const char* name;
    int value;
} Entry;

// We hash and return only the `name` field of the Entry struct, as it is the one we want to use
// as key, while the other one contains the associated data. 
// Note that you can use multiple fields as keys by combining their hash and multiple fields as
// data by ignoring them
static uint32_t hash(const void* entry) {
    const Entry* e = entry;
    // Here we use the provided hashing function as we don't need particular requirements,
    // but you can provide your own and substitute it here
    return map_hash_bytes(e->name, strlen(e->name));
}

// Check the `name` fields of two Entry structs for equality
// We need to match up the field chosen as keys in both the `hash` and `compare` functions,
// otherwise we'll incur in some problems (such as the impossibilty to retrieve an item back)
static bool compare(const void* entry1, const void* entry2) {
    const Entry *e1 = entry1, *e2 = entry2;
    // the `== 0` part is important as strcmp returns 0 on equality which is falsy in c!
    return strcmp(e1->name, e2->name) == 0;
}

static void map_example(void) {
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

int main(void) {
    vector_example();
    string_example();
    map_example();
}
