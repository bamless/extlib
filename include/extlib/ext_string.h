#ifndef EXT_STRING_H
#define EXT_STRING_H

#include <stdarg.h>
#include <stdlib.h>

#if defined(__has_include)
    #if __has_include("ext_vector.h") && !defined(EXT_LIB_NO_VECTOR)
        #include "ext_vector.h"
    #endif
#elif defined(_MSC_VER) && !defined(EXT_LIB_NO_VECTOR)
    #include "ext_vector.h"
#endif

#define ext_str_npos ((size_t)-1)

typedef char* ext_string;

ext_string ext_str_new_cap(size_t capacity);
ext_string ext_str_new_len(const void* data, size_t len);
ext_string ext_str_dup(const ext_string str);
ext_string ext_str_new(const char* cstring);
ext_string ext_str_vfmt(const char* fmt, va_list ap);
ext_string ext_str_fmt(const char* fmt, ...);
void ext_str_free(ext_string str);

ext_string ext_str_join(const char* sep, char** strings, int count);
ext_string ext_str_join_str(const char* sep, ext_string* strings, int count);
ext_string ext_str_substr(const ext_string str, size_t start, size_t end);

void ext_str_append_len(ext_string* str, const void* data, size_t len);
void ext_str_append_str(ext_string* str, const ext_string other);
void ext_str_append(ext_string* str, const char* cstring);
void ext_str_append_vfmt(ext_string* str, const char* fmt, va_list ap);
void ext_str_append_fmt(ext_string* str, const char* fmt, ...);

size_t ext_str_find_len(const ext_string str, size_t start_pos, const void* needle, size_t len);
size_t ext_str_find_str(const ext_string str, size_t start_pos, const ext_string needle);
size_t ext_str_find(const ext_string str, size_t start_pos, const char* needle);

size_t ext_str_rfind_len(const ext_string str, size_t start_pos, const void* needle, size_t len);
size_t ext_str_rfind_str(const ext_string str, size_t start_pos, const ext_string needle);
size_t ext_str_rfind(const ext_string str, size_t start_pos, const char* needle);

size_t ext_str_find_char(const ext_string str, size_t start_pos, int c);
size_t ext_str_rfind_char(const ext_string str, size_t start_pos, int c);

void ext_str_to_lower(ext_string str);
void ext_str_to_upper(ext_string str);

#ifdef EXT_VECTOR_H
ext_vector(ext_string) ext_str_split(const ext_string str, char sep);
void ext_str_split_free(ext_vector(ext_string) split);
#endif

void ext_str_shrink_to_fit(ext_string* str);
void ext_str_reserve(ext_string* str, size_t amount);

int ext_str_compare(const ext_string s1, const ext_string s2);

size_t ext_str_size(const ext_string str);
size_t ext_str_capacity(const ext_string str);

#ifndef EXT_LIB_NO_SHORTHANDS

    #define str_npos ext_str_npos

typedef ext_string string;

static inline ext_string str_new_cap(size_t capacity) {
    return ext_str_new_cap(capacity);
}

static inline ext_string str_new_len(const void* data, size_t len) {
    return ext_str_new_len(data, len);
}

static inline ext_string str_dup(const ext_string str) {
    return ext_str_dup(str);
}

static inline ext_string str_new(const char* cstring) {
    return ext_str_new(cstring);
}

static inline ext_string str_vfmt(const char* fmt, va_list ap) {
    return ext_str_vfmt(fmt, ap);
}

static inline ext_string str_fmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ext_string result = ext_str_vfmt(fmt, args);
    va_end(args);
    return result;
}

static inline void str_free(ext_string str) {
    ext_str_free(str);
}

static inline ext_string str_join(const char* sep, char** strings, int count) {
    return ext_str_join(sep, strings, count);
}

static inline ext_string str_join_str(const char* sep, ext_string* strings, int count) {
    return ext_str_join_str(sep, strings, count);
}

static inline ext_string str_substr(const ext_string str, size_t start, size_t end) {
    return ext_str_substr(str, start, end);
}

static inline void str_append_len(ext_string* str, const void* data, size_t len) {
    ext_str_append_len(str, data, len);
}

static inline void str_append_str(ext_string* str, const ext_string other) {
    ext_str_append_str(str, other);
}

static inline void str_append(ext_string* str, const char* cstring) {
    ext_str_append(str, cstring);
}

static inline void str_append_vfmt(ext_string* str, const char* fmt, va_list ap) {
    ext_str_append_vfmt(str, fmt, ap);
}

static inline void str_append_fmt(ext_string* str, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ext_str_append_vfmt(str, fmt, args);
    va_end(args);
}

static inline size_t str_find_len(const ext_string str, size_t start_pos, const void* needle,
                                  size_t len) {
    return ext_str_find_len(str, start_pos, needle, len);
}

static inline size_t str_find_str(const ext_string str, size_t start_pos, const ext_string needle) {
    return ext_str_find_str(str, start_pos, needle);
}

static inline size_t str_find(const ext_string str, size_t start_pos, const char* needle) {
    return ext_str_find(str, start_pos, needle);
}

static inline size_t str_rfind_len(const ext_string str, size_t start_pos, const void* needle,
                                   size_t len) {
    return ext_str_rfind_len(str, start_pos, needle, len);
}

static inline size_t str_rfind_str(const ext_string str, size_t start_pos,
                                   const ext_string needle) {
    return ext_str_rfind_str(str, start_pos, needle);
}

static inline size_t str_rfind(const ext_string str, size_t start_pos, const char* needle) {
    return ext_str_rfind(str, start_pos, needle);
}

static inline size_t str_find_char(const ext_string str, size_t start_pos, int c) {
    return ext_str_find_char(str, start_pos, c);
}

static inline size_t str_rfind_char(const ext_string str, size_t start_pos, int c) {
    return ext_str_rfind_char(str, start_pos, c);
}

static inline void str_to_lower(ext_string str) {
    ext_str_to_lower(str);
}

static inline void str_to_upper(ext_string str) {
    ext_str_to_upper(str);
}

    #ifdef EXT_VECTOR_H
static inline ext_vector(ext_string) str_split(const ext_string str, char sep) {
    return ext_str_split(str, sep);
}

static inline void str_split_free(ext_vector(ext_string) split) {
    ext_str_split_free(split);
}
    #endif

static inline void str_shrink_to_fit(ext_string* str) {
    ext_str_shrink_to_fit(str);
}

static inline void str_reserve(ext_string* str, size_t amount) {
    ext_str_reserve(str, amount);
}

static inline int str_compare(const ext_string s1, const ext_string s2) {
    return ext_str_compare(s1, s2);
}

static inline size_t str_size(const ext_string str) {
    return ext_str_size(str);
}

static inline size_t str_capacity(const ext_string str) {
    return ext_str_capacity(str);
}

#endif // EXT_LIB_NO_SHORTHANDS

#endif  // STRING_H
