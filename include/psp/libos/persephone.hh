#ifndef PERSEPHONE_H_
#define PERSEPHONE_H_

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <yaml-cpp/yaml.h>

#include <thread>

#include <psp/logging.hh>
#include <psp/time.hh>
#include <psp/libos/io/lrpc/context.hh>
#include <psp/libos/io/udp_dpdk/context.hh>
#include <psp/libos/Request.hh>

#include <deque>

/*************** Gloval variables **************/
namespace po = boost::program_options;

/* Logging */
const int TRACE = 0;
const int APP_TRACE = 1;

extern std::string log_dir;
extern std::string label;
#define MAX_LOG_FILENAME_LEN 128

#define MAX_FILE_PATH_LEN 128
static inline std::string generate_log_file_path(std::string exp_label, char const *log_label) {
    char pathname[MAX_FILE_PATH_LEN];
    snprintf(
        pathname, MAX_FILE_PATH_LEN, "%s/%s/%s", log_dir.c_str(), exp_label.c_str(), log_label
    );
    std::string str_pathname(pathname);
    return pathname;
}

enum class WorkerType {
    UNKNOWN = 0,
    HTTP,
    STORE,
    NET,
    DISPATCH,
    CLIENT,
    SILO,
    MBK,
    RDB
};

[[gnu::unused]] static const char *wt_str[] {
    "UNKNOWN",
    "HTTP",
    "STORE",
    "NET",
    "DISPATCH",
    "CLIENT",
    "SILO",
    "MBK",
    "ROCKSDB",
};

/************** Workers ************/
#define MAX_WORKERS 32
class Worker {
    private:
        bool started = false, exited = false;
        std::thread worker_thread;
        static void main_loop(void *wrkr);
        // Functions to overload in child classes
        virtual int dequeue(unsigned long *payload) = 0;
        virtual int setup() = 0;
        virtual int work(int status, unsigned long payload) = 0;
        virtual int process_request(unsigned long payload) = 0;
    protected:
        template <typename T> int drain_queue(T &rqueue);
        int app_work(int status, unsigned long payload);
        int app_dequeue(unsigned long *payload);
        uint32_t meter_peer_queue(int peer_id);

        uint64_t n_batchs_rcvd = 0;
        uint64_t n_drops = 0;

        int dpt_id = -1;

        bool terminate = false;

    public:
        UdpContext *udp_ctx;
        LrpcContext lrpc_ctx;

        int register_dpt(Worker &dpt);

        uint64_t n_peers = 0;
        int peer_ids[MAX_PEERS];

        bool notify = true;
        uint32_t cpu_id;
        int worker_id;
        enum WorkerType type;
        bool eal_thread = false;

        int join();
        int launch();
        bool has_exited() { return exited; }
        void stop() { terminate = true; }

        Worker(enum WorkerType type);
        Worker(enum WorkerType type, int worker_id);

        virtual ~Worker() = 0;
};

extern Worker *workers[MAX_WORKERS];
extern uint32_t total_workers;

class Psp {
    /* HW related variables */
    std::vector<uint32_t> cpus;
    public: struct rte_mempool *net_mempool = nullptr; /* << A global network mempool. Maybe unused */

    /* ctor */
    public: Psp(std::string &app_cfg, std::string label = "PspApp");
    /* dtor */
    public: ~Psp() {
        print_dpdk_device_stats(0); //XXX
        if (net_mempool) {
            rte_mempool_free(net_mempool);
            net_mempool = nullptr;
        }
    }
    private:
        template <typename A, typename B, typename C>
        int CreateWorker(int su_idx, B *dpt, C *netw, UdpContext *udp_ctx);

    /* Retrieve all workers of a given type */
    public: uint32_t get_workers(enum WorkerType type, Worker **wrkrs) {
        int j = 0;
        for (unsigned int i = 0; i < total_workers; ++i) {
            if (workers[i]->type == type) {
                wrkrs[j++] = workers[i];
            }
        }
        return j;
    }

    public: static void stop_all(int signo) {
        PSP_INFO("Stopping all workers");
        for (unsigned int i = 0; i < total_workers; ++i) {
            PSP_WARN(
                "Setting worker " << workers[i]->worker_id
                << " (" << wt_str[static_cast<int>(workers[i]->type)] << ") to terminate"
            );
            workers[i]->stop();
        }
    }

    /* Utilities */
    public: std::string role;
};

/**************** Utilities ****************/
//FIXME: those should be somewhere else
static inline void pin_thread(pthread_t thread, u_int16_t cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        PSP_ERROR("could not pin thread: " << strerror(errno));
    }
}

#endif //PERSEPHONE_H_
