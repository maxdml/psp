#ifndef DISPATCH_SU_H_
#define DISPATCH_SU_H_

#include <arpa/inet.h>
#include <psp/libos/persephone.hh>
#include <psp/libos/Request.hh>
#include <fstream>

#define MAX_CLIENTS 64

#define RESA_SAMPLES_NEEDED 5e4
#define UPDATE_PERIOD 5 * 1e3 * FREQ //5 usec
#define MAX_WINDOWS 8192

struct profiling_windows {
    uint64_t tsc_start;
    uint64_t tsc_end;
    uint64_t count;
    double mean_ns;
    uint64_t qlen[MAX_TYPES];
    uint64_t counts[MAX_TYPES];
    uint32_t group_res[MAX_TYPES];
    uint32_t group_steal[MAX_TYPES];
    bool do_update;
};

class Dispatcher : public Worker {
    /* Dispatch mode */
    public: enum dispatch_mode {
        DFCFS = 0,
        CFCFS,
        SJF,
        DARC,
        EDF,
        UNKNOWN
    };

    public: const char *dp_str[6] = {
        "DFCFS",
        "CFCFS",
        "SJF",
        "DARC",
        "EDF",
        "UNKNOWN"
    };

    public: static enum dispatch_mode str_to_dp(std::string const &dp) {
       if (dp == "CFCFS") {
            return dispatch_mode::CFCFS;
        } else if (dp == "DFCFS") {
            return dispatch_mode::DFCFS;
        } else if (dp == "SJF") {
            return dispatch_mode::SJF;
        } else if (dp == "DARC") {
            return dispatch_mode::DARC;
        } else if (dp == "EDF") {
            return dispatch_mode::EDF;
        }
        return dispatch_mode::UNKNOWN;
    }

    public: enum dispatch_mode dp;
    // peer ID -> number of "compute slot" available (max= max batch size)
    public: uint32_t free_peers = 0; // a bitmask of free workers
    private: uint8_t last_peer = 0;
    public: RequestType *rtypes[static_cast<int>(ReqType::LAST)];
    public: uint32_t n_rtypes;
    public: uint32_t type_to_nsorder[static_cast<int>(ReqType::LAST)];
    public: uint64_t num_rcvd = 0; /** < Total number of received requests */
    private: uint32_t n_drops = 0;
    private: uint32_t num_dped = 0;
    private: uint64_t peer_dpt_tsc[MAX_WORKERS]; // Record last time we dispatched to a peer
    public: uint32_t n_workers = 0;

    // DARC parameters
    public: uint32_t n_resas;
    public: uint32_t n_groups = 0;
    public: TypeGroups groups[MAX_TYPES];
    private: float delta = 0.2; // Similarity factor
    public: bool first_resa_done;
    public: bool dynamic;
    public: profiling_windows windows[MAX_WINDOWS];
    private: uint32_t prev_active;
    private: uint32_t n_windows = 0;
    public: uint32_t spillway = 0;

    public: int set_darc();
    private: int update_darc();
    private: int drain_queue(RequestType *&rtype);
    private: int dyn_resa_drain_queue(RequestType *&rtype);
    private: int dequeue(unsigned long *payload);
    private: int setup() override;
    private: int work(int status, unsigned long payload) override;
    private: int process_request(unsigned long req) override;
    public: int signal_free_worker(int peer_id, unsigned long type);
    public: int enqueue(unsigned long req, uint64_t cur_tsc);
    public: int dispatch();
    private: inline int push_to_rqueue(unsigned long req, RequestType *&rtype, uint64_t cur_tsc);

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
        PSP_INFO("Latest windows count: " << windows[n_windows].count << ". Performed " << n_windows << " updates.");
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            PSP_INFO(
                "[" << req_type_str[static_cast<int>(rtypes[i]->type)] << "] has "
                << rtypes[i]->rqueue_head - rtypes[i]->rqueue_tail << " pending items"
            );

            PSP_INFO(
                "[" << req_type_str[static_cast<int>(rtypes[i]->type)] << "] average ns: "
                << rtypes[i]->windows_mean_ns / FREQ
            );
            delete rtypes[i];
        }
        PSP_INFO(
            "[" << req_type_str[static_cast<int>(rtypes[type_to_nsorder[0]]->type)] << "] has "
            << rtypes[type_to_nsorder[0]]->rqueue_head - rtypes[type_to_nsorder[0]]->rqueue_tail
            << " pending items"
        );
        delete rtypes[type_to_nsorder[0]];

        if (dp == DARC) {
             // Record windows statistics
             std::string path = generate_log_file_path(label, "server/windows");
             std::cout << "dpt log at " << path << std::endl;
             std::ofstream output(path);
             if (not output.is_open()) {
                 PSP_ERROR("COULD NOT OPEN " << path);
             } else {
                 output << "ID\tSTART\tEND\tGID\tRES\tSTEAL\tCOUNT\tUPDATED\tQLEN" << std::endl;
                 for (size_t i = 0; i < n_windows; ++i) {
                     auto &w = windows[i];
                     for (size_t j = 0; j < n_rtypes; ++j) {
                         output << i << "\t" << std::fixed << w.tsc_start / FREQ
                                << "\t" << std::fixed << w.tsc_end / FREQ
                                << "\t" << j
                                << "\t" << w.group_res[j] << "\t" << w.group_steal[j]
                                << "\t" << w.counts[j] << "\t" << w.do_update
                                << "\t" << w.qlen[j]
                                << std::endl;
                     }
                 }
                 output.close();
             }
        }
    }
};

#endif //DISPATCH_SU_H_
