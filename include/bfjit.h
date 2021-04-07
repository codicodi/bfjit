#ifndef BFJIT_H
#define BFJIT_H

#include <stdio.h>
#include <stdlib.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error "Unsupported architecture"
#endif

#define bf_error(...)                               \
    do {                                            \
        fprintf(stderr, "error: " __VA_ARGS__);     \
        fputc('\n', stderr);                        \
        exit(1);                                    \
    } while (0)

#endif
