#include <psp/libos/io/lrpc/context.hh>
#include <psp/annot.h>
#include <psp/logging.hh>

const std::pair<lrpc_msg *, uint32_t *> LrpcContext::generate_lrpc_buffer() {
    /* Create a buffer for the queue */
    size_t s = sizeof(lrpc_msg) * LRPC_Q_LEN;
    lrpc_msg *buf = static_cast<lrpc_msg *>(malloc(s));
    if (!buf) {
        PSP_ERROR("Could not allocate LRPC buffer: " << strerror(errno));
    }
    PSP_DEBUG("Created LRPC buf at " << buf);
    memset(buf, 0, s);

    /* Associated head pointer */
    uint32_t *wb = static_cast<uint32_t *>(malloc(CACHE_LINE_SIZE));
    if (!buf) {
        PSP_ERROR("Could not allocate LRPC wb: " << strerror(errno));
    }
    PSP_DEBUG("Created LRPC wb at " << wb);
    memset(wb, 0, CACHE_LINE_SIZE);

    return std::move(std::make_pair(buf, wb));
}

int LrpcContext::pop(unsigned long *payload, uint32_t peer_id) {
    uint64_t cmd_out;
    unsigned long payload_out;
    if (lrpc_recv(&ingress[peer_id], &cmd_out, &payload_out)) {
        *payload = payload_out;
        return 0;
    } else {
        return EAGAIN;
    }
}

int LrpcContext::push(unsigned long payload, uint32_t peer_id) {
    if (not lrpc_send(&egress[peer_id], LRPC_TX, payload)) {
        return EAGAIN;
    }
    return 0;
}
