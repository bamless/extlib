#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t (*hash_fn)(const void* entry);
typedef bool (*compare_fn)(const void* entry1, const void* entry2);

typedef struct ext_map ext_map;

ext_map* ext_map_new(size_t entry_sz, hash_fn hash, compare_fn compare);
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

#endif  // MAP_H
