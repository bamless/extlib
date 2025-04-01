#ifndef EXTLIB_H
#define EXTLIB_H

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------------------
// Allocation functions

#ifndef EXTLIB_REALLOC
    #define EXTLIB_REALLOC(ptr, size) realloc(ptr, size)
#endif  // EXTLIB_REALLOC

#ifndef EXTLIB_FREE
    #define EXTLIB_FREE(ptr) free(ptr)
#endif  // EXTLIB_FREE

// ----------------------------------------------------------------------------
// Assertions

#ifndef NDEBUG
    // More convinient assert macro, accepts a message to be displayed.
    #define EXT_ASSERT(cond, msg)                                                               \
        ((cond) ? ((void)0)                                                                     \
                : (fprintf(stderr, "%s [line:%d] in %s(): %s failed: %s\n", __FILE__, __LINE__, \
                           __func__, #cond, msg),                                               \
                   abort()))

    // Unreachable code assert macro, prints a message and aborts. Replaced by compiler builtins in
    // release mode
    #define EXT_UNREACHABLE()                                                                    \
        (fprintf(stderr, "%s [line:%d] in %s(): Reached unreachable code\n", __FILE__, __LINE__, \
                 __func__),                                                                      \
         abort())

#else
    #define EXT_ASSERT(cond, msg) ((void)(cond))

    #if defined(__GNUC__) || defined(__clang__)
        #define EXT_UNREACHABLE() __builtin_unreachable()
    #elif defined(_MSC_VER)
        #include <stdlib.h>
        #define EXT_UNREACHABLE() __assume(0)
    #else
        #define EXT_UNREACHABLE() ((void)0)
    #endif
#endif

// Portable static assert macro. It uses the builtin if available (>= C11), otherwise it uses a
// janky trick with bitfields and macros to create a compile-time error.
#ifndef static_assert
    #define EXT_CONCAT2_(pre, post) pre##post
    #define EXT_CONCAT_(pre, post)  EXT_CONCAT2_(pre, post)
    #define EXT_STATIC_ASSERT(cond, msg)            \
        typedef struct {                            \
            int static_assertion_failed : !!(cond); \
        } EXT_CONCAT_(static_assertion_, __COUNTER__)
#else
    #define JSR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#endif

#ifndef EXTLIB_NO_SHORTHANDS
    #define ASSERT        EXT_ASSERT
    #define UNREACHABLE   EXT_UNREACHABLE
    #define STATIC_ASSERT EXT_STATIC_ASSERT
#endif  // EXT_LIB_NO_SHORTHANDS

// ----------------------------------------------------------------------------
// Vector

// Utility macro for declaring a vector. This is a pointer to a type T, useful for visually
// distinguish `ext_vector`s from normal `T*`s.
#define ext_vector(T) T*

// Utility macro for iterating in a foreach style
#define ext_vec_foreach(elem, vec)                                                              \
    for(size_t __cont = 1, __i = 0; __cont && __i < ext_vec_size(vec); __cont = !__cont, __i++) \
        for(elem = ext_vec_iterator(vec, __i); __cont; __cont = !__cont)

// Release vector resources
#define ext_vec_free(vec)                          \
    do {                                           \
        if(vec) EXTLIB_FREE(ext_vec_header_(vec)); \
    } while(0)

// Returns the number of elements in the vector
#define ext_vec_size(vec) ((vec) ? (ext_vec_header_(vec)->size) : (size_t)0)

// Returns the number of elements that can be stored in the vector before reisizing
#define ext_vec_capacity(vec) ((vec) ? (ext_vec_header_(vec)->capacity) : (size_t)0)

// Returns true if the vector is empty
#define ext_vec_empty(vec) (ext_vec_size(vec) == 0)

// Returns the first element in the vector
#define ext_vec_front(vec) ((vec)[0])

// Returns the last element in the vector
#define ext_vec_back(vec) ((vec)[ext_vec_size(vec) - 1])

// Pushes an element to the back of the vector
#define ext_vec_push_back(vec, e)         \
    do {                                  \
        ext_vec_maybe_grow_(vec, 1);      \
        size_t size = ext_vec_size(vec);  \
        (vec)[size] = (e);                \
        ext_vec_set_size_(vec, size + 1); \
    } while(0)

// Pushes an array of elements to the back of the vector
#define ext_vec_push_back_all(vec, arr, size)           \
    do {                                                \
        ext_vec_reserve(vec, ext_vec_size(vec) + size); \
        for(size_t i = 0; i < size; i++) {              \
            ext_vec_push_back(vec, arr[i]);             \
        }                                               \
    } while(0)

// Pops the last element from the vector
#define ext_vec_pop_back(vec)                                                \
    do {                                                                     \
        assert(ext_vec_size(vec) != 0 && "Cannot pop_back on empty vector"); \
        ext_vec_set_size_(vec, ext_vec_size(vec) - 1);                       \
    } while(0)

// Inserts an element at the i-th position in the vector
#define ext_vec_insert(vec, i, e)                   \
    do {                                            \
        ext_vec_maybe_grow_(vec, 1);                \
        size_t size = ext_vec_size(vec);            \
        assert(i < size + 1 && "Buffer overflow");  \
        size_t shift = (size - i) * sizeof(*(vec)); \
        memmove((vec) + i + 1, (vec) + i, shift);   \
        (vec)[i] = (e);                             \
        ext_vec_set_size_(vec, size + 1);           \
    } while(0)

// Erases the i-th element from the vector
#define ext_vec_erase(vec, i)                           \
    do {                                                \
        size_t size = ext_vec_size(vec);                \
        assert(i < size && "Buffer overflow");          \
        size_t shift = (size - i - 1) * sizeof(*(vec)); \
        memmove((vec) + i, (vec) + i + 1, shift);       \
        ext_vec_set_size_(vec, size - 1);               \
    } while(0);

// Erases the i-th element from the vector and replaces it with the last element
// More efficient than ext_vec_erase, but does not preserve order
#define ext_vec_erase_unordered(vec, i)        \
    do {                                       \
        size_t size = ext_vec_size(vec);       \
        assert(i < size && "Buffer overflow"); \
        (vec)[i] = (vec)[size - 1];            \
        ext_vec_set_size_(vec, size - 1);      \
    } while(0);

// Clears the vector, but does not free the memory (i.e. capacity remains the same)
#define ext_vec_clear(vec)                      \
    do {                                        \
        if(vec) ext_vec_header_(vec)->size = 0; \
    } while(0)

// Reserves memory for 'amount' elements in the vector. Does nothing if 'amount' is less than the
// current capacity. The size of the vector is not changed.
#define ext_vec_reserve(vec, amount)                                                      \
    do {                                                                                  \
        if(!(vec)) {                                                                      \
            ext_vec_header_t_* header =                                                   \
                EXTLIB_REALLOC(NULL, sizeof(*header) + (amount) * sizeof(*(vec)));        \
            assert(header && "Out of memory");                                            \
            header->capacity = (amount);                                                  \
            header->size = 0;                                                             \
            (vec) = (void*)(header->data);                                                \
        } else if(ext_vec_capacity(vec) < (amount)) {                                     \
            ext_vec_header_t_* header = ext_vec_header_(vec);                             \
            header = EXTLIB_REALLOC(header, sizeof(*header) + (amount) * sizeof(*(vec))); \
            assert(header && "Out of memory");                                            \
            header->capacity = (amount);                                                  \
            (vec) = (void*)(header->data);                                                \
        }                                                                                 \
    } while(0)

// Resizes the vector to 'new_size' elements. Allocates memory if the new size is larger than the
// old size. It zeros the new elements if any.
#define ext_vec_resize_zeroed(vec, new_size)                             \
    assert((new_size) >= 0 && "Negative size");                          \
    do {                                                                 \
        size_t size = ext_vec_size(vec);                                 \
        if(new_size >= size) {                                           \
            ext_vec_reserve(vec, new_size);                              \
            memset((vec) + size, 0, (new_size - size) * sizeof(*(vec))); \
            ext_vec_set_size_(vec, new_size);                            \
        }                                                                \
        ext_vec_set_size_(vec, new_size);                                \
    } while(0)

// Resizes the vector to 'new_size' elements. Allocates memory if the new size is larger than the
// old size. The new elements, if any, are not initialized.
#define ext_vec_resize(vec, new_size)           \
    assert((new_size) >= 0 && "Negative size"); \
    do {                                        \
        size_t size = ext_vec_size(vec);        \
        if(new_size >= size) {                  \
            ext_vec_reserve(vec, new_size);     \
        }                                       \
        ext_vec_set_size_(vec, new_size);       \
    } while(0)

// Shrinks the vector capacity to fit the current size.
#define ext_vec_shrink_to_fit(vec)                                                                \
    do {                                                                                          \
        if(vec) {                                                                                 \
            ext_vec_header_t_* header = ext_vec_header_(vec);                                     \
            if(header->size) {                                                                    \
                header = EXTLIB_REALLOC(header, sizeof(*header) + sizeof(*(vec)) * header->size); \
                assert(header && "Out of memory");                                                \
                header->capacity = header->size;                                                  \
                (vec) = (void*)(header->data);                                                    \
            } else {                                                                              \
                EXTLIB_FREE(header);                                                              \
                (vec) = NULL;                                                                     \
            }                                                                                     \
        }                                                                                         \
    } while(0)

// ----------------------------------------------------------------------------
// Vector iterators

// Returns a pointer to the first element in the vector
#define ext_vec_begin(vec) (vec)

// Returns a pointer to the end of the vector
#define ext_vec_end(vec) ((vec) ? (vec) + ext_vec_header_(vec)->size : NULL)

// Returns a pointer to the i-th element in the vector
#define ext_vec_iterator(vec, i) ((vec) + i)

// Returns the index of the iterator in the vector
#define ext_vec_iterator_index(vec, it) (it - (vec))

#ifndef EXTLIB_NO_SHORTHANDS
    #define vec_foreach        ext_vec_foreach
    #define vector             ext_vector
    #define vec_free           ext_vec_free
    #define vec_size           ext_vec_size
    #define vec_capacity       ext_vec_capacity
    #define vec_empty          ext_vec_empty
    #define vec_front          ext_vec_front
    #define vec_back           ext_vec_back
    #define vec_begin          ext_vec_begin
    #define vec_end            ext_vec_end
    #define vec_iterator       ext_vec_iterator
    #define vec_iterator_index ext_vec_iterator_index
    #define vec_push_back      ext_vec_push_back
    #define vec_push_back_all  ext_vec_push_back_all
    #define vec_pop_back       ext_vec_pop_back
    #define vec_insert         ext_vec_insert
    #define vec_erase          ext_vec_erase
    #define vec_clear          ext_vec_clear
    #define vec_reserve        ext_vec_reserve
    #define vec_resize         ext_vec_resize
    #define vec_shrink_to_fit  ext_vec_shrink_to_fit
#endif  // EXTLIB_NO_SHORTHANDS

// ----------------------------------------------------------------------------
// Vector private functions - do not use these directly

typedef struct {
    size_t capacity, size;  // Capacity (allocated memory) and size (slots used in the vector)
    char data[];            // The actual start of the vector memory
} ext_vec_header_t_;

#define ext_vec_maybe_grow_(vec, amount)                             \
    do {                                                             \
        size_t capacity = ext_vec_capacity(vec);                     \
        size_t size = ext_vec_size(vec);                             \
        if(size + (amount) > capacity) {                             \
            size_t new_capacity = capacity ? capacity * 2 : 1;       \
            while(size + (amount) > new_capacity) new_capacity *= 2; \
            ext_vec_reserve(vec, new_capacity);                      \
        }                                                            \
    } while(0)

#define ext_vec_header_(vec)            ((ext_vec_header_t_*)((char*)(vec) - sizeof(ext_vec_header_t_)))
#define ext_vec_set_capacity_(vec, cap) (ext_vec_header_(vec)->capacity = cap)
#define ext_vec_set_size_(vec, sz)      (ext_vec_header_(vec)->size = sz)

// ----------------------------------------------------------------------------
// String

// Signals an invalid position in the string. Same as SIZE_MAX.
#define ext_str_npos ((size_t)-1)

// Utility typedef to visually distinguish between `ext_string`s and normal `char*`s
typedef char* ext_string;

// Creates a new string with the given capacity.
ext_string ext_str_new_cap(size_t capacity);
// Creates a new string with the given data and length.
ext_string ext_str_new_len(const void* data, size_t len);
// Copies the given `ext_string`.
ext_string ext_str_dup(const ext_string str);
// Creates a new string from a C-style string.
ext_string ext_str_new(const char* cstring);
// Creates a new string using a format string and a variable argument list.
ext_string ext_str_vfmt(const char* fmt, va_list ap);
// Creates a new string using a format string.
ext_string ext_str_fmt(const char* fmt, ...);
// Frees the memory used by the string.
void ext_str_free(ext_string str);

// Joins an array of C-style strings into a single string, separated by the given separator.
ext_string ext_str_join(const char* sep, char** strings, int count);
// Joins an array of `ext_string`s into a single string, separated by the given separator.
ext_string ext_str_join_str(const char* sep, ext_string* strings, int count);
// Creates a substring from the given string, starting at the given position and ending at the
ext_string ext_str_substr(const ext_string str, size_t start, size_t end);

// Appends arbitrary data of the given length to the string.
void ext_str_append_len(ext_string* str, const void* data, size_t len);
// Appends an `ext_string` to the string.
void ext_str_append_str(ext_string* str, const ext_string other);
// Appends a C-style string to the string.
void ext_str_append(ext_string* str, const char* cstring);
// Appends a formatted string to the string using a variable argument list.
void ext_str_append_vfmt(ext_string* str, const char* fmt, va_list ap);
// Appends a formatted string to the string.
void ext_str_append_fmt(ext_string* str, const char* fmt, ...);

// Finds the first occurrence of a substring of arbitrary data of size len in the string, starting
// at the given position.
size_t ext_str_find_len(const ext_string str, size_t start_pos, const void* needle, size_t len);
// Finds the first occurrence of an `ext_string` in the string, starting at the given position.
size_t ext_str_find_str(const ext_string str, size_t start_pos, const ext_string needle);
// Finds the first occurrence of a C-style substring in the string, starting at the given position.
size_t ext_str_find(const ext_string str, size_t start_pos, const char* needle);

// Finds the last occurrence of a substring of arbitrary data of size len in the string, starting
// at the given position.
size_t ext_str_rfind_len(const ext_string str, size_t start_pos, const void* needle, size_t len);
// Finds the last occurrence of an `ext_string` in the string, starting at the given position.
size_t ext_str_rfind_str(const ext_string str, size_t start_pos, const ext_string needle);
// Finds the last occurrence of a C-style substring in the string, starting at the given position.
size_t ext_str_rfind(const ext_string str, size_t start_pos, const char* needle);

// Finds the first occurrence of a character in the string, starting at the given position.
size_t ext_str_find_char(const ext_string str, size_t start_pos, int c);
// Finds the last occurrence of a character in the string, starting at the given position.
size_t ext_str_rfind_char(const ext_string str, size_t start_pos, int c);

// Converts the string to lowercase. Assumes the string is ASCII.
void ext_str_to_lower(ext_string str);
// Converts the string to uppercase. Assumes the string is ASCII.
void ext_str_to_upper(ext_string str);

// Splits the string into an array of `ext_string`s using the given separator.
ext_vector(ext_string) ext_str_split(const ext_string str, char sep);
// Frees the memory used by the array of `ext_string`s.
void ext_str_split_free(ext_vector(ext_string) split);

// Shrinks the string capacity to fit the current size.
void ext_str_shrink_to_fit(ext_string* str);
// Reserves memory for 'amount' characters in the string. Does nothing if 'amount' is less than the
// current capacity. The size of the string is not changed.
void ext_str_reserve(ext_string* str, size_t amount);
// Resizes the string to 'new_size' characters. Allocates memory if the new size is larger than the
// old size. It zeros the new characters if any.
void ext_str_resize_zeroed(ext_string* str, size_t new_size);
// Resizes the string to 'new_size' characters. Allocates memory if the new size is larger than the
// old size. The new characters, if any, are not initialized.
void ext_str_resize(ext_string* str, size_t new_size);

// Compares two strings lexicographically. Returns 0 if equal, < 0 if s1 < s2, > 0 if s1 > s2.
int ext_str_compare(const ext_string s1, const ext_string s2);

// Returns the number of bytes used by the string.
size_t ext_str_size(const ext_string str);
// Returns the number of bytes that can be stored in the string before resizing.
size_t ext_str_capacity(const ext_string str);

#ifndef EXTLIB_NO_SHORTHANDS

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
static inline ext_vector(ext_string) str_split(const ext_string str, char sep) {
    return ext_str_split(str, sep);
}
static inline void str_split_free(ext_vector(ext_string) split) {
    ext_str_split_free(split);
}
static inline void str_shrink_to_fit(ext_string* str) {
    ext_str_shrink_to_fit(str);
}
static inline void str_reserve(ext_string* str, size_t amount) {
    ext_str_reserve(str, amount);
}
static inline void str_resize_zeroed(ext_string* str, size_t new_size) {
    ext_str_resize_zeroed(str, new_size);
}
static inline void str_resize(ext_string* str, size_t new_size) {
    ext_str_resize(str, new_size);
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
#endif  // EXTLIB_NO_SHORTHANDS

// ----------------------------------------------------------------------------
// String implementation

#ifdef EXTLIB_IMPLEMENTATION

    #define EXT_STR_LOG_GROWTH_TRESH (1024 * 1024)
    #define ext_str_header_(s)       ((str_header_t_*)(s - sizeof(str_header_t_)))

typedef struct {
    size_t capacity, size;
    char data[];
} str_header_t_;

static void ext_str_set_size_(ext_string* str, size_t size) {
    ext_str_header_(*str)->size = size;
}

static void ext_str_maybe_grow_(ext_string* str, size_t amount) {
    str_header_t_* header = ext_str_header_(*str);
    if(header->size + amount >= header->capacity) {
        size_t new_capacity = header->capacity;

        if(new_capacity > EXT_STR_LOG_GROWTH_TRESH) {
            new_capacity += amount;
        } else {
            while(new_capacity <= header->size + amount) {
                new_capacity *= 2;
            }
        }

        header = EXTLIB_REALLOC(header, sizeof(*header) + new_capacity);
        assert(header && "Out of memory");
        header->capacity = new_capacity;
        *str = header->data;
    }
}

ext_string ext_str_new_cap(size_t capacity) {
    str_header_t_* header = EXTLIB_REALLOC(NULL, sizeof(*header) + capacity + 1);
    header->size = 0;
    header->capacity = capacity + 1;
    return header->data;
}

ext_string ext_str_new_len(const void* data, size_t len) {
    assert(data && "data cannot be NULL");
    str_header_t_* header = EXTLIB_REALLOC(NULL, sizeof(*header) + len + 1);
    assert(header && "Out of memory");
    memcpy(header->data, data, len);
    header->data[len] = '\0';
    header->capacity = len + 1;
    header->size = len;
    return header->data;
}

ext_string ext_str_new(const char* cstring) {
    assert(cstring && "cstring cannot be NULL");
    return ext_str_new_len(cstring, strlen(cstring));
}

ext_string ext_str_dup(const ext_string str) {
    return ext_str_new_len(str, ext_str_size(str));
}

ext_string ext_str_vfmt(const char* fmt, va_list ap) {
    ext_string str = ext_str_new_cap(strlen(fmt) * 2);
    size_t capacity = ext_str_capacity(str);

    va_list args;
    va_copy(args, ap);
    size_t written = vsnprintf(str, capacity, fmt, args);
    va_end(args);

    if(written >= capacity) {
        ext_str_maybe_grow_(&str, written);
        capacity = ext_str_capacity(str);

        va_copy(args, ap);
        written = vsnprintf(str, capacity, fmt, args);
        va_end(args);

        assert(written < capacity && "Buffer still too small");
    }

    ext_str_set_size_(&str, written);
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
    if(str) EXTLIB_FREE(ext_str_header_(str));
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
    ext_str_maybe_grow_(str, len);
    size_t size = ext_str_size(*str);
    memcpy(*str + size, data, len);
    (*str)[size + len] = '\0';
    ext_str_set_size_(str, size + len);
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
        ext_str_maybe_grow_(str, written);
        capacity = ext_str_capacity(*str);
        available = capacity - size;

        va_copy(args, ap);
        size_t written = vsnprintf(*str + size, available, fmt, args);
        va_end(args);

        assert(written < available && "Buffer still too small");
        (void)written;
    }

    ext_str_set_size_(str, size + written);
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

void ext_str_shrink_to_fit(ext_string* str) {
    size_t capacity = ext_str_capacity(*str);
    size_t size = ext_str_size(*str);
    if(size + 1 < capacity) {
        str_header_t_* header = ext_str_header_(*str);
        header = EXTLIB_REALLOC(header, sizeof(*header) + size + 1);
        assert(header && "Out of memory");
        header->capacity = size + 1;
        *str = header->data;
    }
}

void ext_str_reserve(ext_string* str, size_t amount) {
    size_t capacity = ext_str_capacity(*str);
    if(amount + 1 > capacity) {
        str_header_t_* header = ext_str_header_(*str);
        header = EXTLIB_REALLOC(header, sizeof(*header) + amount + 1);
        assert(header && "Out of memory");
        header->capacity = amount + 1;
        *str = header->data;
    }
}

void ext_str_resize_zeroed(ext_string* str, size_t new_size) {
    size_t size = ext_str_size(*str);
    if(new_size > size) {
        ext_str_maybe_grow_(str, new_size - size);
        memset(*str + size, 0, new_size - size);
    }
    ext_str_set_size_(str, new_size);
    (*str)[new_size] = '\0';
}

void ext_str_resize(ext_string* str, size_t new_size) {
    ext_str_maybe_grow_(str, new_size);
    ext_str_set_size_(str, new_size);
    (*str)[new_size] = '\0';
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
    return ext_str_header_(str)->size;
}

size_t ext_str_capacity(const ext_string str) {
    return ext_str_header_(str)->capacity;
}
#endif  // EXT_LIB_IMPLEMENTATION

// ----------------------------------------------------------------------------
// Hashmap

// Hashmap entry hash callback function
typedef uint32_t (*hash_fn)(const void* entry);
// Hashmap entry comparison callback function
typedef bool (*compare_fn)(const void* entry1, const void* entry2);

typedef struct {
    uint32_t hash;
} ext_map_bucket_;

typedef struct ext_map {
    hash_fn hash;
    compare_fn compare;
    size_t entry_sz;
    size_t capacity_mask;
    size_t num_entries;
    size_t size;
    ext_map_bucket_* buckets;
    void* entries;
} ext_map;

// Initializes the hashmap with the given entry size, hash function, and comparison function.
void ext_map_init(ext_map* map, size_t entry_sz, hash_fn hash, compare_fn compare);
// Frees the memory used by the hashmap.
void ext_map_free(ext_map* map);

// Gets the entry in the hashmap. Returns NULL if not found.
const void* ext_map_get(const ext_map* map, const void* entry);
// Puts an entry in the hashmap. Returns true if the entry was added, false if it was replaced
// (already existed in the map).
bool ext_map_put(ext_map* map, const void* entry);
// Erases an entry from the hashmap. Returns true if the entry was erased, false if it was not
// found.
bool ext_map_erase(ext_map* map, const void* entry);
// Clears the hashmap, but does not free the memory.
void ext_map_clear(ext_map* map);

// Returns the number of entries in the hashmap.
size_t ext_map_size(const ext_map* map);
// Returns the number of entries that can be stored in the hashmap before resizing.
size_t ext_map_capacity(const ext_map* map);
// Returns true if the hashmap is empty.
bool ext_map_empty(const ext_map* map);

// Utility function to hash arbitrary data of the given size.
uint32_t ext_map_hash_bytes(const void* bytes, size_t size);

// -----------------------------------------------------------
// Hashmap iterators

// Returns a pointer to the first entry in the hashmap.
const void* ext_map_begin(const ext_map* map);
// Returns a pointer to the end of the hashmap.
const void* ext_map_end(const ext_map* map);
// Increments the iterator to the next entry in the hashmap.
const void* ext_map_incr(const ext_map* map, const void* it);

#ifndef EXTLIB_NO_SHORTHANDS

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
#endif  // EXTLIB_NO_SHORTHANDS

#ifdef EXTLIB_IMPLEMENTATION
    // Read as: size * 0.75, i.e. a load factor of 75%
    // This is basically doing:
    //   size / 2 + size / 4 = (3 * size) / 4
    #define EXT_MAP_MAX_ENTRY_LOAD(size) (((size) >> 1) + ((size) >> 2))

    #define EXT_MAP_INITIAL_CAPACITY 8
    #define EXT_MAP_EMPTY_MARK       0
    #define EXT_MAP_TOMB_MARK        1

    #define EXT_MAP_IS_TOMB(bucket)  ((bucket)->hash == EXT_MAP_TOMB_MARK)
    #define EXT_MAP_IS_EMPTY(bucket) ((bucket)->hash == EXT_MAP_EMPTY_MARK)
    #define EXT_MAP_IS_VALID(bucket) (!EXT_MAP_IS_EMPTY(bucket) && !EXT_MAP_IS_TOMB(bucket))

static void* entry_at(void* entries, size_t entry_sz, size_t idx) {
    return ((char*)entries) + idx * entry_sz;
}

static void map_grow(ext_map* map) {
    size_t new_cap = map->capacity_mask ? (map->capacity_mask + 1) * 2 : EXT_MAP_INITIAL_CAPACITY;
    void* new_entries = EXTLIB_REALLOC(NULL, map->entry_sz * new_cap);
    ext_map_bucket_* new_buckets = EXTLIB_REALLOC(NULL, sizeof(ext_map_bucket_) * new_cap);
    assert(new_entries && new_buckets && "Out of memory");
    memset(new_buckets, EXT_MAP_EMPTY_MARK, sizeof(ext_map_bucket_) * new_cap);

    if(map->capacity_mask > 0) {
        map->num_entries = 0;
        for(size_t i = 0; i <= map->capacity_mask; i++) {
            ext_map_bucket_* buck = &map->buckets[i];
            if(EXT_MAP_IS_VALID(buck)) {
                size_t new_idx = buck->hash & (new_cap - 1);  // Read as: buck->hash % new_cap
                new_buckets[new_idx] = *buck;
                memcpy(entry_at(new_entries, map->entry_sz, new_idx),
                       entry_at(map->entries, map->entry_sz, i), map->entry_sz);
                map->num_entries++;
            }
        }
    }

    EXTLIB_FREE(map->entries);
    EXTLIB_FREE(map->buckets);

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
        ext_map_bucket_* buck = &map->buckets[idx];
        if(!EXT_MAP_IS_VALID(buck)) {
            if(EXT_MAP_IS_EMPTY(buck)) {
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
    EXTLIB_FREE(map->entries);
    EXTLIB_FREE(map->buckets);
    #ifndef NDEBUG
    *map = (ext_map){0};
    #endif
}

const void* ext_map_get(const ext_map* map, const void* entry) {
    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);

    ext_map_bucket_* buck = &map->buckets[idx];
    if(!EXT_MAP_IS_VALID(buck)) {
        return NULL;
    }

    return entry_at(map->entries, map->entry_sz, idx);
}

bool ext_map_put(ext_map* map, const void* entry) {
    if(map->num_entries + 1 > EXT_MAP_MAX_ENTRY_LOAD(map->capacity_mask + 1)) {
        map_grow(map);
    }

    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);
    ext_map_bucket_* buck = &map->buckets[idx];

    bool is_new = !EXT_MAP_IS_VALID(buck);
    if(is_new) {
        map->size++;
        if(EXT_MAP_IS_EMPTY(buck)) map->num_entries++;
    }

    buck->hash = hash;
    memcpy(entry_at(map->entries, map->entry_sz, idx), entry, map->entry_sz);

    return is_new;
}

bool ext_map_erase(ext_map* map, const void* entry) {
    uint32_t hash = hash_entry(map, entry);
    size_t idx = find_index(map, entry, hash);

    ext_map_bucket_* buck = &map->buckets[idx];
    if(EXT_MAP_IS_VALID(buck)) {
        buck->hash = EXT_MAP_TOMB_MARK;
        map->size--;
        return true;
    }

    return false;
}

void ext_map_clear(ext_map* map) {
    if(map->capacity_mask > 0) {
        for(size_t i = 0; i <= map->capacity_mask; i++) {
            map->buckets[i].hash = EXT_MAP_EMPTY_MARK;
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
        if(EXT_MAP_IS_VALID(&map->buckets[i])) {
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
        if(EXT_MAP_IS_VALID(&map->buckets[i])) {
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
#endif

#endif  // EXTLIB_H
