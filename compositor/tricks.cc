#include "tricks.h"

#include <time.h>
#include <math.h>

double linuxTimeInMs()
{
	struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    time_t s2  = spec.tv_sec;
    return round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds
}
