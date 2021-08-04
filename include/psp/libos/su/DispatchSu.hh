#ifndef DISPATCH_SU_H_
#define DISPATCH_SU_H_

#include <arpa/inet.h>
#include <psp/libos/persephone.hh>
#include <psp/libos/Request.hh>

#define MAX_CLIENTS 64

#define TOKENS_REFRESH_NS 1e6

class Dispatcher : public Worker {
    /* Dispatch mode */
    public: enum dispatch_mode {
        DFCFS = 0,
        CFCFS,
        SJF,
        CSCQ,
        EDFNP,
        RTC,
        DYN_RESA,
        UNKNOWN
    };

    public: static enum dispatch_mode str_to_dp(std::string const &dp) {
       if (dp == "CFCFS") {
            return dispatch_mode::CFCFS;
        } else if (dp == "DFCFS") {
            return dispatch_mode::DFCFS;
        } else if (dp == "SJF") {
            return dispatch_mode::SJF;
        } else if (dp == "CSCQ") {
            return dispatch_mode::CSCQ;
        } else if (dp == "EDFNP") {
            return dispatch_mode::EDFNP;
        } else if (dp == "DYN_RESA") {
            return dispatch_mode::DYN_RESA;
        }
        return dispatch_mode::UNKNOWN;
    }

    public: enum dispatch_mode dp;
    // peer ID -> number of "compute slot" available (max= max batch size)
    public: uint32_t free_peers = 0; // a bitmask of free workers
    private: uint8_t last_peer = 0;
    public: RequestType rtypes[static_cast<int>(ReqType::LAST)];
    public: uint32_t n_rtypes;
    public: uint32_t type_to_nsorder[static_cast<int>(ReqType::LAST)];
    public: uint64_t num_rcvd = 0; /** < Total number of received requests */
    private: uint32_t n_drops = 0;
    private: uint32_t num_dped = 0;

    // DYN_RESA parameters
    private: tp last_resa; //TODO init in ctor
    private: uint32_t n_resas;
    private: uint64_t epsilon = 5000;

    public: uint32_t n_workers = 0;

    private: int drain_queue(RequestType &rtype);
    private: int dequeue(unsigned long *payload);
    private: int setup() override;
    private: int work(int status, unsigned long payload) override;
    private: int process_request(unsigned long req) override;
    public: int signal_free_worker(int peer_id);
    public: int enqueue(unsigned long req);
    public: int dispatch();
    private: inline int push_to_rqueue(unsigned long req, RequestType &rtype);

    public: void set_dp(std::string &policy) {
        dp = Dispatcher::str_to_dp(policy);
    }

    public: Dispatcher() : Worker(WorkerType::DISPATCH) {}
    public: Dispatcher(int worker_id) : Worker(WorkerType::DISPATCH, worker_id) {}

    public: ~Dispatcher() {
        PSP_INFO(
            "Nested dispatcher received " << num_rcvd << " (" << n_batchs_rcvd << " batches)"
            << " dispatched " << num_dped << " but dropped " << n_drops << " requests"
        );

        for (uint32_t i = 0; i < n_rtypes; ++i) {
            PSP_INFO(
                "[" << req_type_str[static_cast<int>(rtypes[i].type)] << "] has "
                << rtypes[i].rqueue_head - rtypes[i].rqueue_tail << " pending items"
            );
        }
        PSP_INFO(
            "[" << req_type_str[static_cast<int>(rtypes[type_to_nsorder[0]].type)] << "] has "
            << rtypes[type_to_nsorder[0]].rqueue_head - rtypes[type_to_nsorder[0]].rqueue_tail
            << " pending items"
        );
    }
};

#endif //DISPATCH_SU_H_
