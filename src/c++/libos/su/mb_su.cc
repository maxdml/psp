#include <psp/libos/su/MbSu.hh>
#include <arpa/inet.h>

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

    uint32_t i = 0;
    //uint32_t spin_time = 1000;
    uint32_t spin_time = *reinterpret_cast<uint32_t *>(req_addr);
    uint32_t nloops = spin_time * 2593; //XXX adjust this based on your CPU speed
    log_debug("spinning for %u (%u loops)", spin_time, nloops);

    //uint64_t start = rdtscp(NULL);
    for (i = 0; i < nloops; ++i) {
        asm volatile("nop");
    }

    /*
    uint64_t end = rdtscp(NULL);

    std::cout << "Spinned for " << (end - start) / 2.5 << " ns, " << std::endl;
    std::cout << "=====================================" << std::endl;
    */
    n_requests++;

    // Set response size to 0
    *reinterpret_cast<uint32_t *> (req_addr) = 0;
    return 0;
}

int MbWorker::work(int status, unsigned long payload) {
    return app_work(status, payload);
}

int MbWorker::dequeue(unsigned long *payload) {
    return app_dequeue(payload);
}
