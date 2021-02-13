#ifndef STRING_H
#define STRING_H

#include <stdarg.h>
#include <stdlib.h>

#include "extlib/vector.h"

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
ext_string ext_str_join_str(const char *sep, ext_string* strings, int count);
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

ext_vector(ext_string) ext_str_split(const ext_string str, char sep);
void ext_str_split_free(ext_vector(ext_string) split);

void ext_str_shrink_to_fit(ext_string* str);
void ext_str_reserve(ext_string* str, size_t amount);

int ext_str_compare(const ext_string s1, const ext_string s2);

size_t ext_str_size(const ext_string str);
size_t ext_str_capacity(const ext_string str);

#endif  // STRING_H
