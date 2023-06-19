#pragma once

#include <time.h>

static inline double time_diff(const struct timespec *start, const struct timespec *stop) {
	return ((*stop).tv_sec - (*start).tv_sec) + (double)( (*stop).tv_nsec - (*start).tv_nsec ) / (double)1000000000L;
}