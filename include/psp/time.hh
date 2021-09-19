#ifndef PSP_TIME_H_IS_INCLUDED
#define PSP_TIME_H_IS_INCLUDED

#include <psp/cpuid.hh>
#include <psp/logging.hh>

#include <asm/ops.h>
#include <boost/chrono.hpp>
#undef assert
#include <base/assert.h>
#include <base/compiler.h>

extern float cycles_per_ns;

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

inline int get_system_freq(void) {
    CPUID cpuID(0x16, 0x0);
    cycles_per_ns = cpuID.EAX() / 1000.0;
    printf("Processor Base Frequency:  %04d MHz (%.03f cycles per ns)\n", cpuID.EAX(), cycles_per_ns);
    return 0;
};

// From Adam's base OS
inline int time_calibrate_tsc(void) {
    struct timespec sleeptime;
    sleeptime.tv_nsec = 5E8; /* 1/2 second */
    struct timespec t_start, t_end;

    cpu_serialize();
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
        uint64_t ns, end, start;
        double secs;

        start = rdtsc();
        nanosleep(&sleeptime, NULL);
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
        end = rdtscp(NULL);
        ns = ((t_end.tv_sec - t_start.tv_sec) * 1E9);
        ns += (t_end.tv_nsec - t_start.tv_nsec);

        secs = (double)ns / 1000;
        cycles_per_ns = ((uint64_t)((end - start) / secs)) / 1000.0;
        printf("time: detected %.03f ticks / us\n", cycles_per_ns);

        return 0;
    }

    return -1;
}

#endif /* PSP_TIME_H_IS_INCLUDED */
