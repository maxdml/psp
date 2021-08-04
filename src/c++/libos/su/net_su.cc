#include <arpa/inet.h>
#include <sys/stat.h>
#include <psp/libos/su/NetSu.hh>

#define MAX_CLIENTS 64

namespace po = boost::program_options;

int NetWorker::setup() {
    //pin_thread(pthread_self(), cpu_id);
    PSP_NOTNULL(EPERM, udp_ctx);
    PSP_INFO("Set up net worker " << worker_id);
    return 0;
}

// Just a filler
int NetWorker::dequeue(unsigned long *payload) {
    return ENOTSUP;
}

// To fill vtable entry
int NetWorker::process_request(unsigned long payload) {
    return ENOTSUP;
}

int NetWorker::work(int status, unsigned long payload) {
    // Dispatch enqueued requests
    if (dpt.dp != Dispatcher::dispatch_mode::DFCFS) {
        PSP_OK(dpt.dispatch());
    }

    // Check if we got packets from the network
    if (udp_ctx->recv_packets() != EAGAIN) {
        uint64_t cur_tsc = rdtscp(NULL);
        //Process a batch of packets
        size_t batch_dequeued = 0;
        n_batchs_rcvd++;
        while (udp_ctx->pop_head > udp_ctx->pop_tail and batch_dequeued < MAX_RX_BURST) {
            unsigned long req = udp_ctx->inbound_queue[udp_ctx->pop_tail & (INBOUND_Q_LEN - 1)];
            /*
            if (unlikely(is_echo)) {
                //PSP_OK(udp_ctx->free_mbuf(&sga));
                if (unlikely(udp_ctx->push_head - udp_ctx->push_tail == OUTBOUND_Q_LEN)) {
                    PSP_WARN("Outbound UDP queue full. Freeing mbuf " << req);
                    PSP_OK(udp_ctx->free_mbuf(req));
                } else {
                    udp_ctx->pop_tail++;
                    udp_ctx->outbound_queue[udp_ctx->push_head++ & (OUTBOUND_Q_LEN - 1)] = req;
                }
            } else {
            */
                int ret = dpt.enqueue(req, cur_tsc);
                if (ret == EXFULL or ret == ENOENT) {
                    // Free the request because we can't enqueue it
                    PSP_OK(udp_ctx->free_mbuf(req));
                    //break;
                }
                udp_ctx->pop_tail++;
            //}
            batch_dequeued++;
        }
        //PSP_DEBUG("Net worker dequeued " << batch_dequeued << " requests");
        n_rcvd += batch_dequeued;
    }
    /*
    if (unlikely(is_echo)) {
        PSP_OK(udp_ctx->send_packets());
    }
    */

    return 0;
}
