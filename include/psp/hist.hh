#ifndef HIST_H_
#define HIST_H_

#include "base/assert.h"
#include <map>

#define BUCKET_SIZE 1000 // in nanoseconds

typedef struct Histogram_t {
    uint64_t min, max, total, count;
    std::map<uint32_t, uint64_t> buckets;
} Histogram_t;

static inline void insert_value(Histogram_t *hist, uint64_t val) {
    if (val > hist->max) {
        hist->max = val;
    }
    if (val < hist->min or hist->min == 0) {
        hist->min = val;
    }
    hist->total += val;

    int bucket = 0;
    /*
    val >>= 1;
    while (val > 0) {
        val >>=1;
        ++bucket;
    }
    */
    bucket = val / BUCKET_SIZE;
    hist->buckets[bucket]++;
    hist->count++;
};

#endif //HIST_H_
