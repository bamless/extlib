#include "ext_string.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOGARITHMIC_GROWTH_TRESH (1024 * 1024)
#define ext_str_header(s)        ((str_header_t*)(s - sizeof(str_header_t)))

typedef struct {
    size_t capacity, size;
    char data[];
} str_header_t;

static void ext_str_set_size(ext_string* str, size_t size) {
    ext_str_header(*str)->size = size;
}

static void ext_str_maybe_grow(ext_string* str, size_t amount) {
    str_header_t* header = ext_str_header(*str);
    if(header->size + amount >= header->capacity) {
        size_t new_capacity = header->capacity;

        if(new_capacity > LOGARITHMIC_GROWTH_TRESH) {
            new_capacity += amount;
        } else {
            while(new_capacity <= header->size + amount) {
                new_capacity *= 2;
            }
        }

        header = realloc(header, sizeof(*header) + new_capacity);
        assert(header && "Out of memory");
        header->capacity = new_capacity;

        *str = header->data;
    }
}

ext_string ext_str_new_cap(size_t capacity) {
    str_header_t* header = malloc(sizeof(*header) + capacity + 1);
    header->size = 0;
    header->capacity = capacity + 1;
    return header->data;
}

ext_string ext_str_new_len(const void* data, size_t len) {
    str_header_t* header = malloc(sizeof(*header) + len + 1);
    assert(header && "Out of memory");
    memcpy(header->data, data, len);
    header->data[len] = '\0';
    header->capacity = len + 1;
    header->size = len;
    return header->data;
}

ext_string ext_str_dup(const ext_string str) {
    return ext_str_new_len(str, ext_str_size(str));
}

ext_string ext_str_new(const char* cstring) {
    return ext_str_new_len(cstring, strlen(cstring));
}

ext_string ext_str_vfmt(const char* fmt, va_list ap) {
    ext_string str = ext_str_new_cap(strlen(fmt) * 2);
    size_t capacity = ext_str_capacity(str);

    va_list args;
    va_copy(args, ap);
    size_t written = vsnprintf(str, capacity, fmt, args);
    va_end(args);

    if(written >= capacity) {
        ext_str_maybe_grow(&str, written);
        capacity = ext_str_capacity(str);

        va_copy(args, ap);
        written = vsnprintf(str, capacity, fmt, args);
        va_end(args);

        assert(written < capacity && "Buffer still too small");
    }

    ext_str_set_size(&str, written);
    return str;
}

ext_string ext_str_fmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ext_string ret = ext_str_vfmt(fmt, args);
    va_end(args);
    return ret;
}

void ext_str_free(ext_string str) {
    if(str) free(ext_str_header(str));
}

ext_string ext_str_join(const char* sep, char** strings, int count) {
    if(count == 0) return ext_str_new_len("", 0);

    ext_string joined = ext_str_new_cap(strlen(strings[0]) * 2);
    for(int i = 0; i < count; i++) {
        ext_str_append(&joined, strings[i]);
        if(i != count - 1) ext_str_append(&joined, sep);
    }

    return joined;
}

ext_string ext_str_join_str(const char* sep, ext_string* strings, int count) {
    if(count == 0) return ext_str_new_len("", 0);

    ext_string joined = ext_str_new_cap(ext_str_size(strings[0]) * 2);
    for(int i = 0; i < count; i++) {
        ext_str_append_str(&joined, strings[i]);
        if(i != count - 1) ext_str_append(&joined, sep);
    }

    return joined;
}

ext_string ext_str_substr(const ext_string str, size_t start, size_t end) {
    assert(start <= end && "start must be less than or equal to end");
    assert(end <= ext_str_size(str) && "Buffer overflow");
    return ext_str_new_len(str + start, end - start);
}

void ext_str_append_len(ext_string* str, const void* data, size_t len) {
    ext_str_maybe_grow(str, len);
    size_t size = ext_str_size(*str);
    memcpy(*str + size, data, len);
    (*str)[size + len] = '\0';
    ext_str_set_size(str, size + len);
}

void ext_str_append_str(ext_string* str, const ext_string other) {
    ext_str_append_len(str, other, ext_str_size(other));
}

void ext_str_append(ext_string* str, const char* cstring) {
    ext_str_append_len(str, cstring, strlen(cstring));
}

void ext_str_append_vfmt(ext_string* str, const char* fmt, va_list ap) {
    size_t capacity = ext_str_capacity(*str);
    size_t size = ext_str_size(*str);
    size_t available = capacity - size;

    va_list args;
    va_copy(args, ap);
    size_t written = vsnprintf(*str + size, available, fmt, args);
    va_end(args);

    if(written >= available) {
        ext_str_maybe_grow(str, written);
        capacity = ext_str_capacity(*str);
        available = capacity - size;

        va_copy(args, ap);
        size_t written = vsnprintf(*str + size, available, fmt, args);
        va_end(args);

        assert(written < available && "Buffer still too small");
        (void)written;
    }

    ext_str_set_size(str, size + written);
}

void ext_str_append_fmt(ext_string* str, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ext_str_append_vfmt(str, fmt, args);
    va_end(args);
}

size_t ext_str_find_len(ext_string str, size_t start_pos, const void* needle, size_t len) {
    size_t size = ext_str_size(str);
    if(size < len) {
        return ext_str_npos;
    }

    for(size_t i = start_pos; i <= size - len; i++) {
        if(memcmp(str + i, needle, len) == 0) {
            return i;
        }
    }

    return ext_str_npos;
}

size_t ext_str_find_str(const ext_string str, size_t start_pos, const ext_string needle) {
    return ext_str_find_len(str, start_pos, needle, ext_str_size(needle));
}

size_t ext_str_find(const ext_string str, size_t start_pos, const char* needle) {
    return ext_str_find_len(str, start_pos, needle, strlen(needle));
}

size_t ext_str_rfind_len(const ext_string str, size_t start_pos, const void* needle, size_t len) {
    size_t size = ext_str_size(str);
    if(size < len) {
        return ext_str_npos;
    }

    if(start_pos >= size) {
        return ext_str_npos;
    }

    if(start_pos <= len) {
        start_pos = len - 1;
    }

    for(size_t i = size - start_pos - 1; i != ext_str_npos; i--) {
        if(memcmp(str + i, needle, len) == 0) {
            return i;
        }
    }

    return ext_str_npos;
}

size_t ext_str_rfind_str(const ext_string str, size_t start_pos, const ext_string needle) {
    return ext_str_rfind_len(str, start_pos, needle, ext_str_size(needle));
}

size_t ext_str_rfind(const ext_string str, size_t start_pos, const char* needle) {
    return ext_str_rfind_len(str, start_pos, needle, strlen(needle));
}

size_t ext_str_find_char(const ext_string str, size_t start_pos, int c) {
    for(size_t i = start_pos; i < ext_str_size(str); i++) {
        if(str[i] == c) return i;
    }
    return ext_str_npos;
}

size_t ext_str_rfind_char(const ext_string str, size_t start_pos, int c) {
    if(start_pos >= ext_str_size(str)) return ext_str_npos;

    for(size_t i = ext_str_size(str) - start_pos - 1; i != ext_str_npos; i--) {
        if(str[i] == c) return i;
    }
    return ext_str_npos;
}

void ext_str_to_lower(ext_string str) {
    for(size_t i = 0; i < ext_str_size(str); i++) {
        str[i] = tolower(str[i]);
    }
}

void ext_str_to_upper(ext_string str) {
    for(size_t i = 0; i < ext_str_size(str); i++) {
        str[i] = toupper(str[i]);
    }
}

#ifdef EXT_VECTOR_H
ext_vector(ext_string) ext_str_split(const ext_string str, char sep) {
    ext_vector(ext_string) tokens = NULL;

    char* last = str;
    for(size_t i = 0; i < ext_str_size(str); i++) {
        if(str[i] == sep) {
            ext_string tok = ext_str_new_len(last, str + i - last);
            ext_vec_push_back(tokens, tok);
            last = str + i + 1;
        }
    }

    ext_string tok = ext_str_new_len(last, str + ext_str_size(str) - last);
    ext_vec_push_back(tokens, tok);
    return tokens;
}

void ext_str_split_free(ext_vector(ext_string) split) {
    for(ext_string* it = ext_vec_begin(split); it != ext_vec_end(split); it++) {
        ext_str_free(*it);
    }
    ext_vec_free(split);
}
#endif

void ext_str_shrink_to_fit(ext_string* str) {
    size_t capacity = ext_str_capacity(*str);
    size_t size = ext_str_size(*str);

    if(size + 1 < capacity) {
        str_header_t* header = ext_str_header(*str);
        header = realloc(header, sizeof(*header) + size + 1);
        assert(header && "Out of memory");
        header->capacity = size + 1;

        *str = header->data;
    }
}

void ext_str_reserve(ext_string* str, size_t amount) {
    size_t capacity = ext_str_capacity(*str);
    if(amount + 1 > capacity) {
        str_header_t* header = ext_str_header(*str);
        header = realloc(header, sizeof(*header) + amount + 1);
        assert(header && "Out of memory");
        header->capacity = amount + 1;
        *str = header->data;
    }
}

int ext_str_compare(const ext_string s1, const ext_string s2) {
    size_t len1 = ext_str_size(s1);
    size_t len2 = ext_str_size(s2);
    size_t min_len = len1 < len2 ? len1 : len2;

    int res = memcmp(s1, s2, min_len);
    if(res == 0) {
        return len1 > len2 ? 1 : (len1 < len2 ? -1 : 0);
    }

    return res;
}

size_t ext_str_size(const ext_string str) {
    return ext_str_header(str)->size;
}

size_t ext_str_capacity(const ext_string str) {
    return ext_str_header(str)->capacity;
}
