#ifndef EXT_ASSERT_H
#define EXT_ASSERT_H

#define EXT_UNUSED(x) ((void)(x))

#ifndef NDEBUG
    #include <stdio.h>   // IWYU pragma: keep
    #include <stdlib.h>  // IWYU pragma: keep

    #define EXT_ASSERT(cond, msg)                                                               \
        ((cond) ? ((void)0)                                                                     \
                : (fprintf(stderr, "%s [line:%d] in %s(): %s failed: %s\n", __FILE__, __LINE__, \
                           __func__, #cond, msg),                                               \
                   abort()))

    #define EXT_UNREACHABLE()                                                                    \
        (fprintf(stderr, "%s [line:%d] in %s(): Reached unreachable code\n", __FILE__, __LINE__, \
                 __func__),                                                                      \
         abort())

#else
    #define EXT_ASSERT(cond, msg) ((void)0)

    #if defined(__GNUC__) || defined(__clang__)
        #define EXT_UNREACHABLE() __builtin_unreachable()
    #elif defined(_MSC_VER)
        #include <stdlib.h>
        #define EXT_UNREACHABLE() __assume(0)
    #else
        #define EXT_UNREACHABLE() ((void)0)
    #endif
#endif

#ifndef EXT_LIB_NO_SHORTHANDS
    #define ASSERT      EXT_ASSERT
    #define UNREACHABLE EXT_UNREACHABLE
    #define UNUSED      EXT_UNUSED
#endif

#endif  // ASSERT_H
