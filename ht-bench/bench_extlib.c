// bench_extlib.c — Benchmarks for the extlib.h hashmap.
//
// Usage: ./bench_extlib <benchmark> [N]
//
// Available benchmarks (identical workloads to bench_ht for fair comparison):
//   insert-int        Insert N unique uint64 keys
//   lookup-hit-int    Pre-insert N keys, look up all (100% hit rate)
//   lookup-miss-int   Pre-insert N keys [0,N), look up [N,2N) (0% hit rate)
//   delete-int        Pre-insert N keys, delete all
//   mixed-int         Interleaved insert / lookup / delete under churn
//   iterate-int       Pre-insert N keys, iterate the full table
//   word-count-int    Frequency-count: N ops over a [0, N/4) key space
//   delete-heavy-int  Sliding-window: insert+delete to keep table at N/2 entries
//   insert-str        Insert N unique string keys
//   lookup-hit-str    Pre-insert N string keys, look up all (100% hit rate)
//   lookup-miss-str   Pre-insert N string keys [0,N), look up [N,2N) (0% hit)
//   delete-heavy-str  Sliding-window string variant
//   insert-large      Insert N keys with 32-byte struct values
//   lookup-hit-large  Pre-insert N large-value keys, look up all
//   delete-heavy-large Sliding-window with 32-byte struct values
//
// The fingerprint formula is identical to bench_ht.c, so both programs must
// print the same value for the same benchmark and N.

#define EXTLIB_IMPL
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extlib.h"

static volatile uintptr_t g_sink = 0;

#define DEFAULT_N 500000
// "key_XXXXXXXXXXXXXXXXXX\0" fits in 24 bytes
#define KEY_SIZE 24

// ==============================================================================
// Hashmap type definitions
// ==============================================================================

// 64-byte value type — must stay in sync with bench_ht.c.
typedef struct {
    uint64_t timestamp;
    uint32_t id;
    uint32_t flags;
    uint8_t payload[48];  // 64 bytes total
} Record;

static Record make_record(uint64_t key) {
    Record r;
    r.timestamp = key * UINT64_C(1000003);
    r.id = (uint32_t)key;
    r.flags = (uint32_t)(key >> 16) | 0x80000000u;
    uint64_t pat = key ^ UINT64_C(0xdeadbeefcafe1234);
    for(size_t i = 0; i < sizeof(r.payload); i += 8) {
        __builtin_memcpy(r.payload + i, &pat, 8);
        pat = pat * UINT64_C(6364136223846793005) + 1;
    }
    return r;
}

// 64-byte key type — must stay in sync with bench_ht.c.
typedef struct {
    uint64_t id;
    uint8_t data[56];  // 64 bytes total
} BigKey;

static BigKey make_big_key(uint64_t idx) {
    BigKey k;
    k.id = idx;
    uint64_t pat = idx ^ UINT64_C(0xcafebabedeadbeef);
    for(size_t i = 0; i < sizeof(k.data); i += 8) {
        __builtin_memcpy(k.data + i, &pat, 8);
        pat = pat * UINT64_C(6364136223846793005) + 1;
    }
    return k;
}

// extlib requires a specific struct layout: entries pointer to a struct with
// `key` as first field and `value` as second, then hashes, size, capacity,
// and allocator.  We define concrete types rather than using the anonymous
// HashMap() macro so that we can reference them by name in function signatures.

typedef struct {
    uint64_t key;
    int value;
} U64Entry;

typedef struct {
    U64Entry *entries;
    size_t *hashes;
    size_t size, capacity;
    Ext_Allocator *allocator;
} U64Map;

typedef struct {
    const char *key;
    int value;
} StrEntry;

typedef struct {
    StrEntry *entries;
    size_t *hashes;
    size_t size, capacity;
    Ext_Allocator *allocator;
} StrMap;

typedef struct {
    uint64_t key;
    Record value;
} LargeEntry;

typedef struct {
    LargeEntry *entries;
    size_t *hashes;
    size_t size, capacity;
    Ext_Allocator *allocator;
} LargeMap;

typedef struct {
    BigKey key;
    int value;
} BigKeyEntry;

typedef struct {
    BigKeyEntry *entries;
    size_t *hashes;
    size_t size, capacity;
    Ext_Allocator *allocator;
} BigKeyMap;

// ==============================================================================
// Fingerprint helpers
// ==============================================================================

// Same formula as bench_ht.c.  XOR-fold all (key, value) pairs; the result
// depends only on the multiset of entries, not on iteration order.
static uintptr_t fingerprint_u64map(U64Map *map) {
    uintptr_t fp = 0;
    hmap_foreach(U64Entry, it, map) {
        fp ^= (uintptr_t)(it->key * UINT64_C(2654435761)) ^ (uintptr_t)(unsigned int)it->value;
    }
    return fp;
}

static uintptr_t fingerprint_large_map(LargeMap *map) {
    uintptr_t fp = 0;
    hmap_foreach(LargeEntry, it, map) {
        fp ^= (uintptr_t)(it->key * UINT64_C(2654435761)) ^ (uintptr_t)it->value.timestamp ^
              (uintptr_t)it->value.id ^ (uintptr_t)it->value.flags;
    }
    return fp;
}

static uintptr_t fingerprint_bigkey_map(BigKeyMap *map) {
    uintptr_t fp = 0;
    hmap_foreach(BigKeyEntry, it, map) {
        fp ^= (uintptr_t)(it->key.id * UINT64_C(2654435761)) ^ (uintptr_t)(unsigned int)it->value;
    }
    return fp;
}

static uintptr_t fingerprint_strmap(StrMap *map) {
    uintptr_t fp = 0;
    hmap_foreach(StrEntry, it, map) {
        const char *k = it->key;
        uintptr_t h = 2166136261U;  // FNV-1a offset basis
        for(const char *p = k; *p; p++) {
            h ^= (unsigned char)*p;
            h *= 16777619U;  // FNV prime
        }
        fp ^= h ^ (uintptr_t)(unsigned int)it->value;
    }
    return fp;
}

// ==============================================================================
// String key helpers
// ==============================================================================

// Identical allocation strategy to bench_ht.c: one contiguous buffer for all
// key strings, one pointer array — two frees total regardless of n.
static const char **alloc_str_keys(size_t n, size_t seed) {
    char *buf = (char *)malloc(n * KEY_SIZE);
    const char **keys = (const char **)malloc(n * sizeof(char *));
    for(size_t i = 0; i < n; i++) {
        snprintf(buf + i * KEY_SIZE, KEY_SIZE, "key_%016zu", seed + i);
        keys[i] = buf + i * KEY_SIZE;
    }
    return keys;
}

static void free_str_keys(const char **keys) {
    free((void *)keys[0]);
    free(keys);
}

// ==============================================================================
// Integer-key benchmarks
// ==============================================================================

static void bench_insert_int(size_t n) {
    U64Map map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    g_sink += fingerprint_u64map(&map);
    hmap_free(&map);
}

static void bench_lookup_hit_int(size_t n) {
    U64Map map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    uintptr_t fp = 0;
    for(size_t i = 0; i < n; i++) {
        U64Entry *e = hmap_get(&map, (uint64_t)i);
        if(e) {
            fp ^= (uintptr_t)((uint64_t)i * UINT64_C(2654435761)) ^
                  (uintptr_t)(unsigned int)e->value;
        }
    }
    g_sink += fp;
    hmap_free(&map);
}

// Pre-insert keys [0, n).  Look up keys [n, 2n) which are absent.
static void bench_lookup_miss_int(size_t n) {
    U64Map map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    g_sink += fingerprint_u64map(&map);
    for(size_t i = n; i < 2 * n; i++) {
        U64Entry *e = hmap_get(&map, (uint64_t)i);
        if(e) g_sink += (uintptr_t)e->value;  // never taken; prevents dead-code removal
    }
    hmap_free(&map);
}

static void bench_delete_int(size_t n) {
    U64Map map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    g_sink += fingerprint_u64map(&map);
    for(size_t i = 0; i < n; i++) {
        hmap_delete(&map, (uint64_t)i);
    }
    g_sink += map.size;
    hmap_free(&map);
}

// Interleaved: insert key i, look up key (i-1), delete key (i/2) every 4 steps.
static void bench_mixed_int(size_t n) {
    U64Map map = {0};
    uintptr_t fp = 0;
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
        if(i > 0) {
            U64Entry *e = hmap_get(&map, (uint64_t)(i - 1));
            if(e) {
                fp ^= (uintptr_t)((uint64_t)(i - 1) * UINT64_C(2654435761)) ^
                      (uintptr_t)(unsigned int)e->value;
            }
        }
        if((i & 3) == 3) {
            hmap_delete(&map, (uint64_t)(i >> 1));
        }
    }
    g_sink += fp ^ fingerprint_u64map(&map);
    hmap_free(&map);
}

static void bench_iterate_int(size_t n) {
    U64Map map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    g_sink += fingerprint_u64map(&map);
    hmap_free(&map);
}

// Frequency-count: N get_default (find-or-insert) operations over a key space
// of size N/4.  hmap_get_default returns a pointer to the found-or-inserted entry.
static void bench_word_count_int(size_t n) {
    U64Map map = {0};
    size_t range = n / 4 + 1;
    for(size_t i = 0; i < n; i++) {
        hmap_get_default(&map, (uint64_t)(i % range), 0)->value += 1;
    }
    g_sink += fingerprint_u64map(&map);
    hmap_free(&map);
}

static void bench_delete_heavy_int(size_t n) {
    size_t w = n / 2 + 1;
    U64Map map = {0};

    for(size_t i = 0; i < w; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
    }
    for(size_t i = w; i < w + n; i++) {
        hmap_put(&map, (uint64_t)i, (int)i);
        hmap_delete(&map, (uint64_t)(i - w));
    }

    g_sink += fingerprint_u64map(&map);
    hmap_free(&map);
}

// ==============================================================================
// Large-value benchmarks (32-byte Record)
// ==============================================================================

static void bench_insert_large(size_t n) {
    LargeMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, make_record((uint64_t)i));
    }
    g_sink += fingerprint_large_map(&map);
    hmap_free(&map);
}

static void bench_lookup_hit_large(size_t n) {
    LargeMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, (uint64_t)i, make_record((uint64_t)i));
    }
    uintptr_t fp = 0;
    for(size_t i = 0; i < n; i++) {
        LargeEntry *e = hmap_get(&map, (uint64_t)i);
        if(e) {
            fp ^= (uintptr_t)((uint64_t)i * UINT64_C(2654435761)) ^ (uintptr_t)e->value.timestamp ^
                  (uintptr_t)e->value.id ^ (uintptr_t)e->value.flags;
        }
    }
    g_sink += fp;
    hmap_free(&map);
}

static void bench_delete_heavy_large(size_t n) {
    size_t w = n / 2 + 1;
    LargeMap map = {0};
    for(size_t i = 0; i < w; i++) {
        hmap_put(&map, (uint64_t)i, make_record((uint64_t)i));
    }
    for(size_t i = w; i < w + n; i++) {
        hmap_put(&map, (uint64_t)i, make_record((uint64_t)i));
        hmap_delete(&map, (uint64_t)(i - w));
    }
    g_sink += fingerprint_large_map(&map);
    hmap_free(&map);
}

// ==============================================================================
// Large-key benchmarks (64-byte BigKey, int value)
// ==============================================================================

static void bench_insert_bigkey(size_t n) {
    BigKeyMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, make_big_key((uint64_t)i), (int)i);
    }
    g_sink += fingerprint_bigkey_map(&map);
    hmap_free(&map);
}

static void bench_lookup_hit_bigkey(size_t n) {
    BigKeyMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put(&map, make_big_key((uint64_t)i), (int)i);
    }
    uintptr_t fp = 0;
    for(size_t i = 0; i < n; i++) {
        BigKey k = make_big_key((uint64_t)i);
        BigKeyEntry *e = hmap_get(&map, k);
        if(e) {
            fp ^= (uintptr_t)(k.id * UINT64_C(2654435761)) ^ (uintptr_t)(unsigned int)e->value;
        }
    }
    g_sink += fp;
    hmap_free(&map);
}

static void bench_delete_heavy_bigkey(size_t n) {
    size_t w = n / 2 + 1;
    BigKeyMap map = {0};
    for(size_t i = 0; i < w; i++) {
        hmap_put(&map, make_big_key((uint64_t)i), (int)i);
    }
    for(size_t i = w; i < w + n; i++) {
        hmap_put(&map, make_big_key((uint64_t)i), (int)i);
        hmap_delete(&map, make_big_key((uint64_t)(i - w)));
    }
    g_sink += fingerprint_bigkey_map(&map);
    hmap_free(&map);
}

// ==============================================================================
// String-key benchmarks
// ==============================================================================

static void bench_delete_heavy_str(size_t n) {
    size_t w = n / 2 + 1;
    const char **keys = alloc_str_keys(w + n, 0);
    StrMap map = {0};

    for(size_t i = 0; i < w; i++) {
        hmap_put_cstr(&map, keys[i], (int)i);
    }
    for(size_t i = w; i < w + n; i++) {
        hmap_put_cstr(&map, keys[i], (int)i);
        hmap_delete_cstr(&map, keys[i - w]);
    }

    g_sink += fingerprint_strmap(&map);
    hmap_free(&map);
    free_str_keys(keys);
}

static void bench_insert_str(size_t n) {
    const char **keys = alloc_str_keys(n, 0);
    StrMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put_cstr(&map, keys[i], (int)i);
    }
    g_sink += fingerprint_strmap(&map);
    hmap_free(&map);
    free_str_keys(keys);
}

static void bench_lookup_hit_str(size_t n) {
    const char **keys = alloc_str_keys(n, 0);
    StrMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put_cstr(&map, keys[i], (int)i);
    }
    uintptr_t fp = 0;
    for(size_t i = 0; i < n; i++) {
        StrEntry *e = hmap_get_cstr(&map, keys[i]);
        if(e) {
            uintptr_t h = 2166136261U;
            for(const char *p = keys[i]; *p; p++) {
                h ^= (unsigned char)*p;
                h *= 16777619U;
            }
            fp ^= h ^ (uintptr_t)(unsigned int)e->value;
        }
    }
    g_sink += fp;
    hmap_free(&map);
    free_str_keys(keys);
}

static void bench_lookup_miss_str(size_t n) {
    const char **keys = alloc_str_keys(n, 0);
    const char **miss_keys = alloc_str_keys(n, n);
    StrMap map = {0};
    for(size_t i = 0; i < n; i++) {
        hmap_put_cstr(&map, keys[i], (int)i);
    }
    g_sink += fingerprint_strmap(&map);
    for(size_t i = 0; i < n; i++) {
        StrEntry *e = hmap_get_cstr(&map, miss_keys[i]);
        if(e) g_sink += (uintptr_t)e->value;  // never taken
    }
    hmap_free(&map);
    free_str_keys(keys);
    free_str_keys(miss_keys);
}

// ==============================================================================
// Dispatch
// ==============================================================================

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr,
                "Usage: %s <benchmark> [N]\n"
                "\n"
                "Benchmarks: insert-int lookup-hit-int lookup-miss-int delete-int\n"
                "            mixed-int iterate-int word-count-int\n"
                "            insert-str lookup-hit-str lookup-miss-str\n",
                argv[0]);
        return 1;
    }

    size_t n = (argc >= 3) ? (size_t)atoll(argv[2]) : DEFAULT_N;
    const char *bm = argv[1];

    if(strcmp(bm, "insert-int") == 0) bench_insert_int(n);
    else if(strcmp(bm, "lookup-hit-int") == 0) bench_lookup_hit_int(n);
    else if(strcmp(bm, "lookup-miss-int") == 0) bench_lookup_miss_int(n);
    else if(strcmp(bm, "delete-int") == 0) bench_delete_int(n);
    else if(strcmp(bm, "mixed-int") == 0) bench_mixed_int(n);
    else if(strcmp(bm, "iterate-int") == 0) bench_iterate_int(n);
    else if(strcmp(bm, "word-count-int") == 0) bench_word_count_int(n);
    else if(strcmp(bm, "delete-heavy-int") == 0) bench_delete_heavy_int(n);
    else if(strcmp(bm, "insert-str") == 0) bench_insert_str(n);
    else if(strcmp(bm, "lookup-hit-str") == 0) bench_lookup_hit_str(n);
    else if(strcmp(bm, "lookup-miss-str") == 0) bench_lookup_miss_str(n);
    else if(strcmp(bm, "delete-heavy-str") == 0) bench_delete_heavy_str(n);
    else if(strcmp(bm, "insert-bigkey") == 0) bench_insert_bigkey(n);
    else if(strcmp(bm, "lookup-hit-bigkey") == 0) bench_lookup_hit_bigkey(n);
    else if(strcmp(bm, "delete-heavy-bigkey") == 0) bench_delete_heavy_bigkey(n);
    else if(strcmp(bm, "insert-large") == 0) bench_insert_large(n);
    else if(strcmp(bm, "lookup-hit-large") == 0) bench_lookup_hit_large(n);
    else if(strcmp(bm, "delete-heavy-large") == 0) bench_delete_heavy_large(n);
    else {
        fprintf(stderr, "Unknown benchmark: %s\n", bm);
        return 1;
    }

    printf("%" PRIuPTR "\n", (uintptr_t)g_sink);
    return 0;
}