#ifndef NET_SU_H_
#define NET_SU_H_

#include <arpa/inet.h>
#include <psp/libos/persephone.hh>
#include <psp/libos/Request.hh>
#include <psp/libos/su/DispatchSu.hh>

#define MAX_CLIENTS 64

class NetWorker : public Worker {
    public: uint64_t num_sent = 0; /** < Total number of answered requests */
    private: uint32_t n_rcvd = 0;

    public: uint32_t type_to_nsorder[static_cast<int>(ReqType::LAST)];

    public: int dp_id = -1;
    public: Dispatcher dpt;

    /* Work */
    private: bool is_echo;
    private: struct sockaddr_in saddr;

    private: int process_request(unsigned long payload) override; // To fill vtable entry
    private: int setup() override;
    private: int work(int status, unsigned long payload) override;
    private: int dequeue(unsigned long *payload);

    public: NetWorker(bool is_echo)
            : Worker(WorkerType::NET),
              dp_id(this->worker_id), dpt(this->worker_id),
              is_echo(is_echo)
            {}

    public: ~NetWorker() {
        PSP_INFO(
            "Net worker received " << n_rcvd
            << " (" << n_batchs_rcvd << " batches)."
            " Dropped " << n_drops << "."
        );
    }
};

#endif //NET_SU_H_
