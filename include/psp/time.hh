#ifndef PSP_TIME_H_IS_INCLUDED
#define PSP_TIME_H_IS_INCLUDED

#define FREQ 2.6

#include <asm/ops.h>
#include <boost/chrono.hpp>
#undef assert
#include <base/assert.h>
using hr_clock = boost::chrono::steady_clock;
typedef hr_clock::time_point tp;

uint64_t since_epoch(const tp &time);
uint64_t ns_diff(const tp &start, const tp &end);
tp take_time();

static const auto system_start_time = take_time();

static inline uint64_t take_start_time() {
    cpu_serialize();
    return rdtsc();
}

static inline void take_end_time(uint64_t *time) {
    *time = rdtscp(NULL);
    cpu_serialize();
}

static inline uint64_t cycles_to_ns(uint64_t time) {
    return time / FREQ;
}

#endif /* PSP_TIME_H_IS_INCLUDED */
