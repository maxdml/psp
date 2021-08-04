#include <psp/libos/persephone.hh>
#include "rte_launch.h"

/***************** Worker methods ***************/
int Worker::register_dpt(Worker &dpt) {
    dpt_id = dpt.worker_id;
    Worker &a = *dynamic_cast<Worker *>(this);
    Worker &b = dpt;
    // Generate two LRPC buffers (in/out). Workers owns their ingress.
    const auto a_buf = a.lrpc_ctx.generate_lrpc_buffer();
    const auto b_buf = b.lrpc_ctx.generate_lrpc_buffer();

    // Setup the ingress/egress pointers for each peer
    lrpc_init_in(
        &a.lrpc_ctx.ingress[0],
        a_buf.first, LRPC_Q_LEN, a_buf.second
    );
    lrpc_init_out(
        &a.lrpc_ctx.egress[0],
        b_buf.first, LRPC_Q_LEN, b_buf.second
    );

    PSP_DEBUG(
        "Worker " << a.worker_id
        << " lrpc input: " << a.lrpc_ctx.ingress[0].tbl
        << ", lrpc output: " << a.lrpc_ctx.egress[0].tbl
    );

    lrpc_init_in(
        &b.lrpc_ctx.ingress[a.worker_id - 1],
        b_buf.first, LRPC_Q_LEN, b_buf.second
    );
    lrpc_init_out(
        &b.lrpc_ctx.egress[a.worker_id - 1],
        a_buf.first, LRPC_Q_LEN, a_buf.second
    );

    PSP_DEBUG(
        "Worker " << b.worker_id
        << " lrpc input: " << b.lrpc_ctx.ingress[a.worker_id - 1].tbl
        << ", lrpc output: " << b.lrpc_ctx.egress[a.worker_id - 1].tbl
    );

    // Register peer ids
    a.peer_ids[a.n_peers++] = b.worker_id;
    b.peer_ids[b.n_peers++] = a.worker_id - 1;

    return 0;
}

int Worker::app_work(int status, unsigned long payload) {
    // Grab the request
    PSP_OK(process_request(payload));

    // Enqueue response to outbound queue
    udp_ctx->outbound_queue[udp_ctx->push_head++ & (OUTBOUND_Q_LEN - 1)] = payload;

    // Notify the dispatcher
    if (notify) {
        unsigned long type = *rte_pktmbuf_mtod_offset(
            static_cast<rte_mbuf *>((void*)payload), char *, NET_HDR_SIZE + sizeof(uint32_t)
        );
        unsigned long notif = (type << 60) ^ rdtscp(NULL);
        if (unlikely(lrpc_ctx.push(notif, 0) == EAGAIN)) {
            PSP_DEBUG("Worker " << worker_id << " could not signal work done to dpt");
        }
    }
    return 0;
}

int Worker::app_dequeue(unsigned long *payload) {
    // Only dequeue new requests if we have buffer space to TX them
    int status = EAGAIN;
    if (udp_ctx->push_head - udp_ctx->push_tail < OUTBOUND_Q_LEN) {
        if (lrpc_ctx.pop(payload, 0) == 0) {
            status = 0;
        }
    }

    //XXX call directly send_packets?
    if (udp_ctx->push_head > udp_ctx->push_tail) {
        PSP_OK(udp_ctx->send_packets());
    }

    return status;
}

void Worker::main_loop(void *wrkr) {
    Worker *me = reinterpret_cast<Worker *>(wrkr);
    PSP_DEBUG("Setting up worker thread " << me->worker_id);
    if (me->setup()) {
        PSP_ERROR("Worker thread " << me->worker_id << " failed to initialize")
        me->exited = true;
        return;
    }
    me->started = true;
    PSP_INFO("Worker thread " << me->worker_id << " started");
    //PSP_INFO("Worker thread " << worker_id << " started");
    while (!me->terminate) {
        unsigned long payload = 0;
        int status = me->dequeue(&payload);
        if (status == EAGAIN) {
            continue;
        }
        int work_status = me->work(status, payload);
        if (work_status) {
            PSP_ERROR("Worker " << me->worker_id << " work() error: " << work_status);
            return;
        }
    }
    me->exited = true;
    PSP_INFO("Worker thread " << me->worker_id << " terminating")
    return;
}

uint32_t Worker::meter_peer_queue(int peer_id) {
    return ENOTSUP;
}

int Worker::join() {
    if (eal_thread) {
        return rte_eal_wait_lcore(this->cpu_id);
    } else {
        if (worker_thread.joinable()) {
            worker_thread.join();
            return 0;
        }
        return -1;
    }
}

int Worker::launch() {
    if (worker_thread.joinable()) {
        PSP_ERROR("Cannot launch thread a second time");
        return -1;
    }

    if (eal_thread) {
        PSP_OK(rte_eal_wait_lcore(this->cpu_id));
        PSP_INFO(
            "Starting EAL worker " << this->worker_id <<
            " on CPU " << this->cpu_id
        );
        PSP_OK(rte_eal_remote_launch(
            (lcore_function_t*)&Worker::main_loop, (void*)this, this->cpu_id
        ));
    } else {
        PSP_INFO("Starting worker " << this->worker_id);
        worker_thread = std::thread(&Worker::main_loop, this);
    }

    // Wait for the thread to start
    while (!started && !exited) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (exited && !started) {
        return -1;
    }
    return 0;
}

Worker::Worker(enum WorkerType type, int worker_id) : worker_id(worker_id), type(type) {}

Worker::Worker(enum WorkerType type) : type(type) {
    worker_id = total_workers++;
}

Worker::~Worker() {
    if (udp_ctx) {
        delete udp_ctx;
        udp_ctx = nullptr;
    }
}
