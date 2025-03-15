#include "ext_map.h"

#include <assert.h>
#include <string.h>

#define MAX_LOAD_FACTOR  0.75
#define INITIAL_CAPACITY 8

#define EMPTY_MARK 0
#define TOMB_MARK  1

#define IS_TOMB(bucket)  ((bucket)->hash == TOMB_MARK)
#define IS_EMPTY(bucket) ((bucket)->hash == EMPTY_MARK)
#define IS_VALID(bucket) (!IS_EMPTY(bucket) && !IS_TOMB(bucket))

static void* entry_at(void* entries, size_t entry_sz, size_t idx) {
    return ((char*)entries) + idx * entry_sz;
}

static void map_grow(ext_map* map) {
    size_t new_cap = map->capacity_mask ? (map->capacity_mask + 1) * 2 : INITIAL_CAPACITY;
    void* new_entries = malloc(map->entry_sz * new_cap);
    ext_map_bucket* new_buckets = calloc(new_cap, sizeof(*new_buckets));
    assert(new_entries && new_buckets && "Out of memory");

    if(map->capacity_mask > 0) {
        map->num_entries = 0;
        for(size_t i = 0; i <= map->capacity_mask; i++) {
            ext_map_bucket* buck = &map->buckets[i];
            if(IS_VALID(buck)) {
                size_t new_idx = buck->hash & (new_cap - 1); // Read as: buck->hash % new_cap
                new_buckets[new_idx] = *buck;
                memcpy(entry_at(new_entries, map->entry_sz, new_idx),
                       entry_at(map->entries, map->entry_sz, i), map->entry_sz);
                map->num_entries++;
            }
        }
    }

    free(map->entries);
    free(map->buckets);

    map->entries = new_entries;
    map->buckets = new_buckets;
    map->capacity_mask = new_cap - 1;
}

static uint32_t hash_entry(const ext_map* map, const void* entry) {
    uint32_t hash = map->hash(entry);
    return hash < 2 ? hash + 2 : hash;  // reserve hash values 0 and 1
}

static size_t find_index(const ext_map* map, const void* entry, uint32_t hash) {
    size_t idx = hash & map->capacity_mask;

    bool tomb_found = false;
    size_t tomb_idx = 0;

    for(;;) {
        ext_map_bucket* buck = &map->buckets[idx];
        if(!IS_VALID(buck)) {
            if(IS_EMPTY(buck)) {
                return tomb_found ? tomb_idx : idx;
            } else if(!tomb_found) {
                tomb_found = true;
                tomb_idx = idx;
            }
        } else if(buck->hash == hash &&
                  map->compare(entry_at(map->entries, map->entry_sz, idx), entry)) {
            return idx;
        }
        idx = (idx + 1) & map->capacity_mask;  // Read as: (idx + 1) % (map->capacity_mask + 1)
    }
}

void ext_map_init(ext_map* map, size_t entry_sz, hash_fn hash, compare_fn compare) {
    *map = (ext_map){hash, compare, entry_sz, 0, 0, 0, NULL, NULL};
}

void ext_map_free(ext_map* map) {
    free(map->entries);
    free(map->buckets);
#ifndef NDEBUG
    *map = (ext_map){0};
#endif
    free(map);
}

const void* ext_map_get(const ext_map* map, const void* entry) {
    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);

    ext_map_bucket* buck = &map->buckets[idx];
    if(!IS_VALID(buck)) {
        return NULL;
    }

    return entry_at(map->entries, map->entry_sz, idx);
}

bool ext_map_put(ext_map* map, const void* entry) {
    if(map->num_entries + 1 > (map->capacity_mask + 1) * MAX_LOAD_FACTOR) {
        map_grow(map);
    }

    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);
    ext_map_bucket* buck = &map->buckets[idx];

    bool is_new = !IS_VALID(buck);
    if(is_new) {
        map->size++;
        if(IS_EMPTY(buck)) map->num_entries++;
    }

    buck->hash = hash;
    memcpy(entry_at(map->entries, map->entry_sz, idx), entry, map->entry_sz);

    return is_new;
}

bool ext_map_erase(ext_map* map, const void* entry) {
    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);

    ext_map_bucket* buck = &map->buckets[idx];
    if(IS_VALID(buck)) {
        buck->hash = TOMB_MARK;
        map->size--;
        return true;
    }

    return false;
}

void ext_map_clear(ext_map* map) {
    if(map->capacity_mask > 0) {
        for(size_t i = 0; i <= map->capacity_mask; i++) {
            map->buckets[i].hash = EMPTY_MARK;
        }
        map->num_entries = 0;
        map->size = 0;
    }
}

size_t ext_map_size(const ext_map* map) {
    return map->size;
}

size_t ext_map_capacity(const ext_map* map) {
    return map->entries ? map->capacity_mask + 1 : 0;
}

bool ext_map_empty(const ext_map* map) {
    return map->size == 0;
}

const void* ext_map_begin(const ext_map* map) {
    if(!map->entries) return NULL;

    for(size_t i = 0; i <= map->capacity_mask; i++) {
        if(IS_VALID(&map->buckets[i])) {
            return map->entries + i * map->entry_sz;
        }
    }

    return ext_map_end(map);
}

const void* ext_map_end(const ext_map* map) {
    return map->entries ? map->entries + ext_map_capacity(map) * map->entry_sz : NULL;
}

static size_t iterator_index(const ext_map* map, const void* it) {
    return (it - map->entries) / map->entry_sz;
}

const void* ext_map_incr(const ext_map* map, const void* it) {
    for(size_t i = iterator_index(map, it) + 1; i <= map->capacity_mask; i++) {
        if(IS_VALID(&map->buckets[i])) {
            return map->entries + i * map->entry_sz;
        }
    }
    return ext_map_end(map);
}

uint32_t ext_map_hash_bytes(const void* bytes, size_t size) {
    const unsigned char* data = bytes;
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 16777619;
    }
    return hash;
}
