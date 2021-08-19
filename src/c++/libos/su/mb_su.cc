#include <psp/libos/su/MbSu.hh>
#include <arpa/inet.h>
#include "fake_work.h"

int MbWorker::setup() {
    assert(n_peers > 0);
    //pin_thread(pthread_self(), cpu_id);

    PSP_INFO("Set up Microbenchmark worker " << worker_id);
    return 0;
}

int MbWorker::process_request(unsigned long payload) {
    char *id_addr = rte_pktmbuf_mtod_offset(
        static_cast<rte_mbuf *>((void*)payload), char *, NET_HDR_SIZE
    );

    char *type_addr = id_addr + sizeof(uint32_t);
    char *req_addr = type_addr + sizeof(uint32_t) * 2; // also pass request size

    //uint32_t spin_time = 1000;
    unsigned int nloops = *reinterpret_cast<unsigned int *>(req_addr) * FREQ;
    PSP_DEBUG("spinning for " << nloops);
    /*
    printf("spinning for %u\n", nloops);
    double durations[1000];
    for (unsigned int i = 0 ; i < 1000; i++) {
        uint64_t start = rdtscp(NULL);
    */
        fake_work(nloops);
        //fake_work2(*reinterpret_cast<unsigned int *>(req_addr), FREQ);
    /*
        uint64_t end = rdtscp(NULL);
        durations[i] = (end - start) / 2.5;
    }
    std::sort(durations, durations+1000);
    printf("median: %f\n", durations[500]);
    printf("p99.9: %f\n", durations[999]);
    */
    uint32_t type = *reinterpret_cast<uint32_t *>(type_addr);
    switch(static_cast<ReqType>(type)) {
        case ReqType::SHORT:
            n_shorts++;
            break;
        case ReqType::LONG:
            n_longs++;
            break;
        default:
            break;
    }
    n_requests++;

    // Hack response to include completion timestamp
    *reinterpret_cast<uint32_t *> (req_addr) = 0;
    return 0;
}

int MbWorker::work(int status, unsigned long payload) {
    return app_work(status, payload);
}

int MbWorker::dequeue(unsigned long *payload) {
    return app_dequeue(payload);
}
