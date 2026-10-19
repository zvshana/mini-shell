#include "utils/timer.h"

void stopwatch_start(Stopwatch *sw)
{
    clock_gettime(CLOCK_MONOTONIC, &sw->start);
}

double stopwatch_elapsed(const Stopwatch *sw)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - sw->start.tv_sec)
         + (double)(now.tv_nsec - sw->start.tv_nsec) / 1e9;
}
