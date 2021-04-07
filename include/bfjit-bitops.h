#ifndef BFJIT_BITOPS_H
#define BFJIT_BITOPS_H

#include <assert.h>
#include <stdint.h>
#if defined _MSC_VER && !defined __clang__
#include <intrin.h>
#endif

static unsigned bf_ctz(uint32_t x)
{
    assert(x != 0);
#if defined _MSC_VER && !defined __clang__
    unsigned long index;
    _BitScanForward(&index, x);
    return index;
#else
    return __builtin_ctz(x);
#endif
}

#endif
