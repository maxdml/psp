#include <arpa/inet.h>
#include <sys/stat.h>
#include <psp/libos/su/DispatchSu.hh>
#include <psp/libos/su/NetSu.hh>

// To fill vtable entries
int Dispatcher::process_request(unsigned long payload) {
    return ENOTSUP;
}

int Dispatcher::dequeue(unsigned long *payload) {
    return ENOTSUP;
}

int Dispatcher::setup() {
    PSP_INFO("Set up dispatcher " << worker_id << "(" << n_workers << " target workers)");
    return 0;
}

int Dispatcher::work(int status, unsigned long payload) {
    return ENOTSUP;
}

int Dispatcher::signal_free_worker(int peer_id) {
    free_peers |= (1 << peer_id);
    PSP_DEBUG(__builtin_popcount(free_peers) << " free workers");
    return 0;
}

int Dispatcher::enqueue(unsigned long req) {
    if (dp == DFCFS) {
        /* Send request to worker's local queue */
        uint32_t peer_id = last_peer++ % 14;
        if (likely(lrpc_ctx.push(req, peer_id) == 0)) {
            num_dped++;
        } else {
            PSP_DEBUG("LRPC queue full at worker " << peer_id);
            return EXFULL;
        }
    } else if (dp == CFCFS) {
        return push_to_rqueue(req, rtypes[0]);
    //} else {
        //TODO request filter goes here
        //return push_to_rqueue(req, rtypes[type_to_nsorder[static_cast<int>(req->type)]]);
    }
    num_rcvd++;

    return 0;
}

inline int Dispatcher::push_to_rqueue(unsigned long req, RequestType &rtype) {
    if (unlikely(rtype.rqueue_head - rtype.rqueue_tail == RQUEUE_LEN)) {
        PSP_DEBUG("Dispatcher dropped request as ReqType::UNKNOWN is full");
        return EXFULL;
    } else {
        rtype.rqueue[rtype.rqueue_head++ & (RQUEUE_LEN - 1)] = req;
        return 0;
    }
}

int Dispatcher::dispatch() {
    /* Check for work completion signals */
    unsigned long notif;
    //FIXME: only circulate through busy peers?
    for (uint32_t i = 0; i < n_peers; ++i) {
        if (lrpc_ctx.pop(&notif, i) == 0) {
            signal_free_worker(i);
        }
    }

    /* Dispatch */
    if (dp == CFCFS) {
        /* Dispatch from the queues to workers */
        drain_queue(rtypes[0]);
    } else if (dp == SJF) {
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            if (rtypes[i].rqueue_head > rtypes[i].rqueue_tail) {
                drain_queue(rtypes[i]);
            }
        }
    } else if (dp == DYN_RESA) {
        // WIP this has never been tested
        // Remap cores to rtypes if epsilon time has passed
        if (unlikely(since_epoch(last_resa) == 0) or ns_diff(last_resa, take_time()) > epsilon) {
            n_resas = 0;
            for (uint32_t i = 0; i < n_rtypes; ++i) {
                if (n_resas == n_peers) {
                    // We reserved all cores already
                    break;
                }
                auto &rtype = rtypes[i];
                uint32_t n_pending = rtype.rqueue_head - rtype.rqueue_tail;
                // Book at least a core for the request type
                if (unlikely(n_pending == 0)) {
                    // We abuse the free_peers list to store reserved workers
                    //rtype.free_peers[0] = least_loaded_workers()[0];
                    rtype.free_peers[0] = __builtin_ctz(free_peers);
                    rtype.n_resas = 1;
                    continue; //go to next rtype
                }
                uint64_t est_demand = n_pending * rtype.mean_ns;
                uint64_t n_cores = (est_demand + rtype.deadline - 1) / rtype.deadline; //ceil
                //FIXME should in worker load order (i.e local queue depth)
                for (uint32_t i = n_resas; i < n_peers; ++i) {
                    rtype.free_peers[rtype.n_resas++] = i;
                    n_resas++;
                    if (rtype.n_resas == n_cores) {
                        break;
                    }
                }
            }
        }
        // Then drain queues in priority order
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            if (rtypes[i].rqueue_head > rtypes[i].rqueue_tail) {
                drain_queue(rtypes[i]);
            }
        }
    }
    return 0;
}

/*
int Dispatcher::pick_res_peer(uint32_t *peer_id, RequestType &rtype) {
    // First check resa cores
    for (uint32_t i = 0; i < rtype.n_resas; ++i) {
        // Check from the global list of that peers has free cycles
        if (free_peers[rtype.free_peers[i]]) {
            *peer_id = rtype.free_peers[i];
            break;
    }
    // Then if none is free, check the rest
    for (uint32_t i = n_resas; i < n_peers; ++i) {
        if (free_peers[i]) {
            *peer_id = i;
            break;
        }
    }
    if (*peer_id) {
        return 0;
    } else {
        return -1;
    }
}
*/

int Dispatcher::drain_queue(RequestType &rtype) {
    size_t drained = 0;
    while (rtype.rqueue_head > rtype.rqueue_tail and free_peers > 0) {
        unsigned long req = rtype.rqueue[rtype.rqueue_tail & (RQUEUE_LEN - 1)];
        uint32_t peer_id = __builtin_ctz(free_peers);
        if (likely(lrpc_ctx.push(req, peer_id)) == 0) {
            drained++;
            num_dped++;
            rtype.rqueue_tail++;
            free_peers ^= (1 << peer_id);
            PSP_DEBUG(
                "Picked peer " << peer_id << " . " << __builtin_popcount(free_peers) << " free peers"
            );
        }
    }
    return 0;
}
