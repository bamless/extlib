#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t (*hash_fn)(const void* entry);
typedef bool (*compare_fn)(const void* entry1, const void* entry2);

typedef struct ext_map_bucket {
    uint32_t hash;
} ext_map_bucket;

typedef struct ext_map {
    hash_fn hash;
    compare_fn compare;
    size_t entry_sz;
    size_t capacity_mask;
    size_t num_entries;
    size_t size;
    ext_map_bucket* buckets;
    void* entries;
} ext_map;

void ext_map_init(ext_map* map, size_t entry_sz, hash_fn hash, compare_fn compare);
void ext_map_free(ext_map* map);

const void* ext_map_get(const ext_map* map, const void* entry);
bool ext_map_put(ext_map* map, const void* entry);
bool ext_map_erase(ext_map* map, const void* entry);
void ext_map_clear(ext_map* map);

size_t ext_map_size(const ext_map* map);
size_t ext_map_capacity(const ext_map* map);
bool ext_map_empty(const ext_map* map);

const void* ext_map_begin(const ext_map* map);
const void* ext_map_end(const ext_map* map);
const void* ext_map_incr(const ext_map* map, const void* it);

uint32_t ext_map_hash_bytes(const void* bytes, size_t size);

#ifndef EXT_LIB_NO_SHORTHANDS

typedef ext_map map;

static inline void map_init(ext_map* map, size_t entry_sz, hash_fn hash, compare_fn compare) {
    ext_map_init(map, entry_sz, hash, compare);
}

static inline void map_free(ext_map* map) {
    ext_map_free(map);
}

static inline const void* map_get(const ext_map* map, const void* entry) {
    return ext_map_get(map, entry);
}

static inline bool map_put(ext_map* map, const void* entry) {
    return ext_map_put(map, entry);
}

static inline bool map_erase(ext_map* map, const void* entry) {
    return ext_map_erase(map, entry);
}

static inline void map_clear(ext_map* map) {
    ext_map_clear(map);
}

static inline size_t map_size(const ext_map* map) {
    return ext_map_size(map);
}

static inline size_t map_capacity(const ext_map* map) {
    return ext_map_capacity(map);
}

static inline bool map_empty(const ext_map* map) {
    return ext_map_empty(map);
}

static inline const void* map_begin(const ext_map* map) {
    return ext_map_begin(map);
}

static inline const void* map_end(const ext_map* map) {
    return ext_map_end(map);
}

static inline const void* map_incr(const ext_map* map, const void* it) {
    return ext_map_incr(map, it);
}

static inline uint32_t map_hash_bytes(const void* bytes, size_t size) {
    return ext_map_hash_bytes(bytes, size);
}

#endif  // EXT_LIB_NO_SHORTHANDS

#endif  // MAP_H
