#ifndef UTILS_TIMER_H
#define UTILS_TIMER_H

#include <time.h>

/* Measures wall-clock durations using CLOCK_MONOTONIC. */
typedef struct {
    struct timespec start;
} Stopwatch;

void stopwatch_start(Stopwatch *sw);

/* Seconds elapsed since stopwatch_start(). */
double stopwatch_elapsed(const Stopwatch *sw);

#endif /* UTILS_TIMER_H */
