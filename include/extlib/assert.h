#ifndef ASSERT_H
#define ASSERT_H

#define UNUSED(x) ((void)(x))

#ifndef NDEBUG
    #include <stdio.h>
    #include <stdlib.h>

    #define ASSERT(cond, msg)                                                                    \
        ((cond) ?                                                                                \
         ((void)0) :                                                                             \
         (fprintf(stderr, "%s [line:%d] in %s(): %s failed: %s\n", __FILE__, __LINE__, __func__, \
                  #cond, msg), abort()))

    #define UNREACHABLE()                                                                        \
        (fprintf(stderr, "%s [line:%d] in %s(): Reached unreachable code\n", __FILE__, __LINE__, \
                 __func__), abort())

#else
    #define ASSERT(cond, msg) ((void)0)
    #define UNREACHABLE()     ((void)0)
#endif

#endif // ASSERT_H
