#ifndef LRPC_CTX_H_
#define LRPC_CTX_H_

#include <psp/libos/io/lrpc.h>
#include <stdlib.h>
#include <utility>

#define MAX_PEERS 32
#define LRPC_Q_LEN 4096

enum {
    LRPC_TX = 0
};

// Host LRPC channels between a worker and its peers
class LrpcContext {
    public: struct lrpc_chan_in ingress[MAX_PEERS];
    public: struct lrpc_chan_out egress[MAX_PEERS];

    public: int pop(unsigned long *payload, uint32_t peer_id);
    public: int push(unsigned long payload, uint32_t peer_id);

    public: const std::pair<lrpc_msg *, uint32_t *> generate_lrpc_buffer();

    public: LrpcContext() {
        memset(ingress, 0, sizeof(struct lrpc_chan_in) * MAX_PEERS);
        memset(egress, 0, sizeof(struct lrpc_chan_out) * MAX_PEERS);
    };
    public: ~LrpcContext() {
        for (int i = 0; i < MAX_PEERS; ++i) {
            if (ingress[i].tbl) {
                free(ingress[i].tbl);
                free(ingress[i].recv_head_wb);
            } else {
                break;
            }
        }
    }
};

#endif //LRPC_CTX_H_
