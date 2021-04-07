#include <time.h>

#include "bfjit.h"
#include "bfjit-time.h"

int64_t bf_clock(void)
{
#if defined TIME_UTC || defined CLOCK_REALTIME
    struct timespec ts;
#if defined TIME_UTC
    if (timespec_get(&ts, TIME_UTC) != 0)
#else
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
#endif
        return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
    bf_error("system has no compatible clock");
}
