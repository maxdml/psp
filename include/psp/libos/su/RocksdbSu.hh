#ifndef RDB_SU_H_
#define RDB_SU_H_

#include <psp/libos/persephone.hh>
#include <psp/libos/Request.hh>
#include <psp/annot.h>

#include <c.h> // RocksDB C bindings

#define MAX_RESPONSE_SIZE 32
#define RESPONSE_TPL "Spinned for %u"

class RdbWorker : public Worker {
    public: RdbWorker() : Worker(WorkerType::RDB) {}
    public : ~RdbWorker() {
                log_info(
                    "RocksDB worker %d processed %u requests (%u GETs, %u SCANs), dropped %lu requests",
                    worker_id, n_requests, n_gets, n_scans, n_drops
                );
             }
    private: uint32_t n_requests = 0;
    private: uint32_t n_gets = 0;
    private: uint32_t n_scans = 0;
    private: int setup() override;
    private: int dequeue(unsigned long *payload);
    private: int work(int status, unsigned long payload) override;
    private: int process_request(unsigned long payload) override;
    public: rocksdb_t *db;
};

#endif //RDB_SU_H_
