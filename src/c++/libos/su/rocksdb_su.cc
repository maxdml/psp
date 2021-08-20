#include <psp/libos/su/RocksdbSu.hh>
#include <arpa/inet.h>

int RdbWorker::setup() {
    assert(n_peers > 0);
    //pin_thread(pthread_self(), cpu_id);

    PSP_INFO("Set up RocksDB worker " << worker_id);
    return 0;
}

int RdbWorker::process_request(unsigned long payload) {
    char *id_addr = rte_pktmbuf_mtod_offset(
        static_cast<rte_mbuf *>((void*)payload), char *, NET_HDR_SIZE
    );

    char *type_addr = id_addr + sizeof(uint32_t);
    char *req_addr = type_addr + sizeof(uint32_t) * 2; // also pass request size

    rocksdb_readoptions_t *readoptions = rocksdb_readoptions_create();
/*
    uint64_t durations[5000];
    unsigned int i = 0;
    for (i = 0; i < 5000; i++) {
        uint64_t start = rdtscp(NULL);
*/
//        if (*reinterpret_cast<uint32_t *>(type_addr) == 2) {
            /* SCAN */
            rocksdb_iterator_t * iter = rocksdb_create_iterator(db, readoptions);
            rocksdb_iter_seek_to_first(iter);
            while(true) {
                if (!rocksdb_iter_valid(iter)) {
                    break;
                }
                size_t klen;
                const char * retr_key = rocksdb_iter_key(iter, &klen);
                rocksdb_iter_next(iter);
                if (*reinterpret_cast<uint32_t *>(type_addr) == 10)
                    break;
            }
            rocksdb_iter_destroy(iter);
/*
        } else {
            size_t len;
            char key[10];
            char *err = NULL;
            snprintf(key, 10, "key%d", *reinterpret_cast<uint32_t *>(id_addr) % 7000);
            char *returned_value = rocksdb_get(db, readoptions, key, strlen(key), &len, &err);
            //printf("%s:%s\n", key, returned_value);
        }
*/
/*
        uint64_t end = rdtscp(NULL);
        //std::cout << req_type_str[*(uint32_t*)type_addr] << " : " << ((end - start) / 2.5) << std::endl;
        durations[i] = (uint64_t) ((end - start) / 2.5);
    }
*/
    rocksdb_readoptions_destroy(readoptions);
/*
    qsort(durations, i, sizeof(double), compare);
    printf("stats for %u iterations: \n", i);
    printf("median: %lu\n", durations[i/2]);
    printf("p99.9: %lu\n", durations[i * 999/1000]);
    printf("====================\n");
*/
    uint32_t type = *reinterpret_cast<uint32_t *>(type_addr);
    switch(static_cast<ReqType>(type)) {
        case ReqType::GET:
            n_gets++;
            break;
        case ReqType::SCAN:
            n_scans++;
            break;
        default:
            break;
    }
    n_requests++;

    // Set response size to 0
    *reinterpret_cast<uint32_t *> (req_addr) = 0;
    return 0;
}

int RdbWorker::work(int status, unsigned long payload) {
    return app_work(status, payload);
}

int RdbWorker::dequeue(unsigned long *payload) {
    return app_dequeue(payload);
}
