#include <stdio.h>
#include <math.h>

#define KB         1024
#define CPUFREQ    10000000
#define TSTATES_US (1.0 / CPUFREQ * 1000000)

#define ONE_MILLIS us_to_tstates(1000);

static inline unsigned long us_to_tstates(double us)
{
    return (unsigned long) (floor(us / TSTATES_US));
}
