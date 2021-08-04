#include <psp/time.hh>
#include <stdint.h>

uint64_t since_epoch(const tp &time) {
    return boost::chrono::time_point_cast<boost::chrono::nanoseconds>(time).time_since_epoch().count();
}

uint64_t ns_diff(const tp &start, const tp &end) {
    auto ns = boost::chrono::duration_cast<boost::chrono::nanoseconds>(end-start).count();
    if (ns < 0) {
        ns = -1;
    }
    return ns;
}

tp take_time() {
    /*
    uint32_t regs[4];
    uint32_t p;
    asm volatile(
        "cpuid" : "=a" (regs[0]), "=b" (regs[1]),
                  "=c" (regs[2]), "=d" (regs[3]): "a" (p), "c" (0)
    );
    */
    tp time = hr_clock::now();
    /*
    asm volatile(
        "cpuid" : "=a" (regs[0]), "=b" (regs[1]),
                  "=c" (regs[2]), "=d" (regs[3]): "a" (p), "c" (0)
    );
    */
    return time;
}
