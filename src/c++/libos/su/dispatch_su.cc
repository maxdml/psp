#include <arpa/inet.h>
#include <sys/stat.h>
#include <psp/libos/su/DispatchSu.hh>
#include <psp/libos/su/NetSu.hh>
#include <bitset>
#include <math.h>

// To fill vtable entries
int Dispatcher::process_request(unsigned long payload) {
    return ENOTSUP;
}

int Dispatcher::dequeue(unsigned long *payload) {
    return ENOTSUP;
}

int Dispatcher::setup() {
    PSP_WARN("Set up dispatcher " << worker_id << "(" << n_workers << " target workers)");
    return 0;
}

void insertionSort(RequestType *rtypes[], int n) {
    int i, j;
    for (i = 1; i < n; ++i) {
        RequestType *key = rtypes[i];
        j = i - 1;
        while (j >= 0 && rtypes[j]->mean_ns > key->mean_ns) {
            rtypes[j + 1] = rtypes[j];
            j = j - 1;
        }
        rtypes[j + 1] = key;
    }
}

int Dispatcher::set_darc() {
    // Reset current number of reserved cores
    n_resas = 0;
    // sort rtypes
    insertionSort(rtypes, n_rtypes);
    for (size_t i = 0; i < n_rtypes; ++i) {
        // Record qlen
        windows[n_windows].qlen[static_cast<int>(rtypes[i]->type) - 1] =
            rtypes[i]->rqueue_head - rtypes[i]->rqueue_tail;
        // Reset type group info
        rtypes[i]->type_group = -1;
        // Update ordering
        type_to_nsorder[static_cast<int>(rtypes[i]->type)] = i;
    }
    // Group similar types together
    n_groups = 0;
    memset(groups, '\0', n_rtypes * sizeof(TypeGroups));
    for (unsigned int j = 0; j < n_rtypes; ++j) {
        auto &rtype = rtypes[j];
        if (rtype->type_group > -1) {
            continue;
        }
        auto &group = groups[n_groups];
        group.members[group.n_members++] = rtype;
        rtype->type_group = n_groups;
        PSP_DEBUG(
            "Group " << n_groups << ": " <<
            req_type_str[static_cast<int>(rtype->type)]
        );
        for (unsigned int i = j+1; i < n_rtypes; ++i) {
            auto &peer = rtypes[i];
            uint64_t diff = peer->mean_ns - rtype->mean_ns;
            if (diff < rtype->mean_ns * delta) {
                group.members[group.n_members++] = peer;
                peer->type_group = n_groups;
                PSP_DEBUG(
                    "Peer joined: " <<
                    req_type_str[static_cast<int>(peer->type)]
                );
            } else {
                break; // Types are sorted by service time
            }
        }
        n_groups++;
    }
    // Assign workers to groups
    for (unsigned int j = 0; j < n_groups; ++j) {
        auto &group = groups[j];
        double group_mean_ns = 0;
        for (unsigned int i = 0; i < group.n_members; ++i) {
            auto &type = group.members[i];
            group_mean_ns += type->mean_ns * type->ratio;
        }
        double cpu_demand = (group_mean_ns / windows[n_windows].mean_ns) * n_workers;
        double full_demand;
        double shared_demand = modf(cpu_demand, &full_demand);
        uint32_t final_cpu_demand;
        if (full_demand == 0) {
            final_cpu_demand = 1;
        } else if (shared_demand <= 0.5) {
            final_cpu_demand = full_demand;
        } else {
            final_cpu_demand = ceil(cpu_demand);
        }

        /*
        if (final_cpu_demand == 0) {
            PSP_WARN(
                "=========================================" <<
                "group " << j << " has " << group.n_members << " members"
                << " mean ns: " << group_mean_ns << " cpu demand: " << cpu_demand
                << " full demand: " << full_demand << " shared demand: "
                << shared_demand
            );
            for (size_t i = 0; i < group.n_members; ++i) {
                PSP_WARN(
                    "member " << i << " mean ns: " << group.members[i]->mean_ns
                    << " ratio: " << group.members[i]->ratio
                );
            }
        }
        */
        PSP_DEBUG("Group " << j << " demand: " << cpu_demand << " (" << final_cpu_demand << ")");
        uint32_t available_workers = n_workers - n_resas;
        if (final_cpu_demand > (available_workers)) {
            // first book whatever cores remains
            unsigned int i;
            for (i = 0; i < available_workers; ++i) {
                PSP_DEBUG("Assigned worker " << n_resas << "( " << n_workers - n_resas << " left)");
                group.res_peers[i] = n_resas++;
                group.n_resas++;
            }
            windows[n_windows].group_res[static_cast<int>(group.members[0]->type) - 1] = group.n_resas;
            // Then use the spillway core (potentially the same as latest allocated core)
            PSP_DEBUG("Assigning spillway core");
            group.res_peers[i] = spillway;
            group.n_resas++;
        } else {
            for (unsigned int i = 0; i < final_cpu_demand; ++i) {
                PSP_DEBUG("Assigned worker " << n_resas);
                group.res_peers[i] = n_resas++;
                group.n_resas++;
            }
            windows[n_windows].group_res[static_cast<int>(group.members[0]->type) - 1] = group.n_resas;
        }
        PSP_DEBUG("reserved: " << n_resas << "/" << n_workers);
        // Setup stealable peers
        PSP_DEBUG("can steal cores " << n_resas << " to " << n_workers);
        for (unsigned int i = n_resas; i < n_workers; ++i) {
            group.stealable_peers[group.n_stealable++] = i;
        }
        windows[n_windows].group_steal[static_cast<int>(group.members[0]->type) - 1] = group.n_stealable;
    }
    PSP_DEBUG("=========================================");
    return 0;
}

int Dispatcher::update_darc() {
    PSP_DEBUG("=========================================");
    PSP_DEBUG("Checking DARC reservation for " << windows[n_windows].count << " samples");
    // Compute the windows' mean_ns, update each type's ratio and mean_ns
    windows[n_windows].mean_ns = 0;
    uint32_t n_active = 0;
    for (unsigned int j = 0; j < n_rtypes; ++j) {
        auto &rtype = rtypes[j];
        windows[n_windows].counts[static_cast<int>(rtype->type) - 1] += rtype->windows_count;
        // Is the type still present?
        if (rtype->windows_count == 0) {
            assert(rtype->windows_mean_ns == 0);
            assert(rtype->delay == 0);
            //assert(rtype.mean_ns > 0); // keep previously known value
            rtype->mean_ns = 1e9; // infinite service time for latter ordering
            rtype->ratio = 0;
            continue;
        }
        n_active++;
        // Update the type
        rtype->mean_ns = static_cast<uint64_t>((rtype->windows_mean_ns - 600) / cycles_per_ns); //FIXME meh
        rtype->ratio = (rtype->windows_count * 1.0) / windows[n_windows].count;
        // Update the window's mean service time
        windows[n_windows].mean_ns += rtype->mean_ns * rtype->ratio;
        PSP_DEBUG(
            "[" << req_type_str[static_cast<int>(rtype->type)] << "] "
            << "[Window " << n_windows << "]"
            << "mean ns: " << rtype->mean_ns
            << " max delay : " << rtype->max_delay / cycles_per_ns
            << " ratio: " << rtype->ratio
        );
        // Reset the profiling windows
        rtype->windows_mean_ns = 0;
        rtype->windows_count = 0;
    }
    windows[n_windows].count = 0;
    // Check whether or not we should trigger an update
    for (unsigned int j = 0; j < n_rtypes; ++j) {
        auto &rtype = rtypes[j];
        double demand = (rtype->mean_ns * rtype->ratio / windows[n_windows].mean_ns) * n_workers;
        double diff = abs(demand - rtype->prev_demand);
        if (diff > (rtype->prev_demand * .1) or (rtype->ratio > 0 and n_active != prev_active)) {
            windows[n_windows].do_update = true;
            rtype->max_delay = rtype->mean_ns * cycles_per_ns; // in cycles
            rtype->prev_demand = demand;
        }
    }
    prev_active = n_active;
    if (windows[n_windows].do_update) {
        PSP_OK(set_darc());
        windows[n_windows].tsc_end = rdtsc();
        n_windows++;
        windows[n_windows].tsc_start = windows[n_windows - 1].tsc_end;
    }
    return 0;
}

int Dispatcher::signal_free_worker(int peer_id, unsigned long notif) {
    /* Update service time and count */
    uint32_t type = notif >> 60;
    auto &t = rtypes[type_to_nsorder[type]];
    uint64_t cplt_tsc = notif & (0xfffffffffffffff);
    uint64_t service_time = cplt_tsc - peer_dpt_tsc[peer_id];
    t->windows_mean_ns = (service_time + (t->windows_mean_ns * t->windows_count)) / (t->windows_count + 1);
    t->windows_count++;
    windows[n_windows].count++;
    free_peers |= (1 << peer_id);
    return 0;
}

int Dispatcher::work(int status, unsigned long payload) {
    return ENOTSUP;
}

int Dispatcher::enqueue(unsigned long req, uint64_t cur_tsc) {
    if (dp == DFCFS) {
        /* Send request to worker's local queue */
        uint32_t peer_id = last_peer++ % n_workers;
        if (likely(lrpc_ctx.push(req, peer_id) == 0)) {
            num_dped++;
        } else {
            PSP_DEBUG("LRPC queue full at worker " << peer_id);
            return EXFULL;
        }
    } else if (dp == CFCFS) {
        return push_to_rqueue(req, rtypes[type_to_nsorder[static_cast<int>(ReqType::UNKNOWN)]], cur_tsc);
    } else {
        uint32_t type = *rte_pktmbuf_mtod_offset(
            static_cast<rte_mbuf *>((void*)req), char *, NET_HDR_SIZE + sizeof(uint32_t)
        );
        if (unlikely(type == 0 or type > static_cast<int>(ReqType::LAST)))
            // Push to UNKNOWN queue
            return push_to_rqueue(req, rtypes[n_rtypes], cur_tsc);
        return push_to_rqueue(req, rtypes[type_to_nsorder[type]], cur_tsc);
    }
    num_rcvd++;
    return 0;
}

inline int Dispatcher::push_to_rqueue(unsigned long req, RequestType *&rtype, uint64_t tsc) {
    if (unlikely(rtype->rqueue_head - rtype->rqueue_tail == RQUEUE_LEN)) {
        PSP_DEBUG(
            "Dispatcher dropped request as "
            << req_type_str[static_cast<int>(rtype->type)] << " is full"
        );
        return EXFULL;
    } else {
        //PSP_DEBUG("Pushed one request to queue " << req_type_str[static_cast<int>(rtype.type)]);
        rtype->tsqueue[rtype->rqueue_head & (RQUEUE_LEN - 1)] = tsc;
        rtype->rqueue[rtype->rqueue_head++ & (RQUEUE_LEN - 1)] = req;
        return 0;
    }
}

int Dispatcher::dispatch() {
    uint64_t cur_tsc = rdtscp(NULL);
    if (unlikely(not first_resa_done)) {
        if (num_dped > RESA_SAMPLES_NEEDED) {
            windows[n_windows].tsc_start = cur_tsc;
            windows[n_windows].count = num_dped;
            dp = DARC;
            first_resa_done = true;
            PSP_OK(update_darc());
        }
    //} else if ((cur_tsc - windows[n_windows].tsc) > update_frequency and windows[n_windows].count > RESA_SAMPLES_NEEDED) {
    } else if (dp == DARC and likely(dynamic) and windows[n_windows].count > RESA_SAMPLES_NEEDED) {
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            if ((rtypes[i]->rqueue_head - rtypes[i]->rqueue_tail) < 2)
                continue;
            uint64_t d = rtypes[i]->tsqueue[(rtypes[i]->rqueue_tail + 1) & (RQUEUE_LEN - 1)];
            rtypes[i]->delay = cur_tsc - d;
            if (rtypes[i]->delay > rtypes[i]->max_delay) {
                PSP_DEBUG(
                    "[UPDATE_PERIOD] updating type "
                    << req_type_str[static_cast<int>(rtypes[i]->type)]
                    << " because delay " << rtypes[i]->delay / cycles_per_ns
                    << " is greater than " << rtypes[i]->max_delay / cycles_per_ns
                );
                PSP_OK(update_darc());
                break;
            }
        }
    }

    /* Check for work completion signals */
    unsigned long notif;
    //FIXME: only circulate through busy peers?
    for (uint32_t i = 0; i < n_peers; ++i) {
        if (lrpc_ctx.pop(&notif, i) == 0) {
            signal_free_worker(i, notif);
        }
    }

    /* Dispatch */
    if (dp == CFCFS) {
        /* Dispatch from the queues to workers */
        drain_queue(rtypes[type_to_nsorder[static_cast<int>(ReqType::UNKNOWN)]]);
    } else if (dp == SJF) {
        // Assumes rtypes are in ascending order in service time
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            if (rtypes[i]->rqueue_head > rtypes[i]->rqueue_tail) {
                drain_queue(rtypes[i]);
            }
        }
    } else if (dp == EDF) {
        uint64_t dpt_time = rdtscp(NULL);
        while (free_peers) {
            // First identify type closest to deadline
            double min = -1;
            int select = -1;
            for (uint32_t i = 0; i < n_rtypes; ++i) {
                if (rtypes[i]->rqueue_head == rtypes[i]->rqueue_tail) {
                    continue;
                }
                uint64_t qstamp = rtypes[i]->tsqueue[rtypes[i]->rqueue_tail & (RQUEUE_LEN - 1)];
                double ttdl = qstamp + (rtypes[i]->deadline * 2.5);
                if (ttdl < min or min == -1) {
                    select = i;
                    min = ttdl;
                }
            }
            if (select == -1) {
                break;
            }
            auto &rtype = rtypes[select];
            // Then dispatch
            unsigned long req = rtype->rqueue[rtype->rqueue_tail & (RQUEUE_LEN - 1)];
            uint32_t peer_id = __builtin_ctz(free_peers);
            if (likely(lrpc_ctx.push(req, peer_id)) == 0) {
                num_dped++;
                rtype->rqueue_tail++;
                free_peers ^= (1 << peer_id);
                peer_dpt_tsc[peer_id] = rdtscp(NULL);
                /*
                PSP_DEBUG(
                    "Picked peer " << peer_id << " . " << __builtin_popcount(free_peers) << " free peers"
                );
                */
            }
        }
    } else if (dp == DARC) {
        for (uint32_t i = 0; i < n_rtypes; ++i) {
            if (rtypes[i]->rqueue_head > rtypes[i]->rqueue_tail) {
                dyn_resa_drain_queue(rtypes[i]);
            }
        }
        // Check for unexpected requests
        if (unlikely(rtypes[n_rtypes]->rqueue_head > rtypes[n_rtypes]->rqueue_tail)) {
            auto &rtype = rtypes[n_rtypes];
            if ((1 << spillway) & free_peers) {
                unsigned long req = rtype->rqueue[rtype->rqueue_tail & (RQUEUE_LEN - 1)];
                if (likely(lrpc_ctx.push(req, spillway)) == 0) {
                    num_dped++;
                    rtype->rqueue_tail++;
                    free_peers ^= (1 << spillway);
                }
            }
        }
    }
    return 0;
}

int Dispatcher::dyn_resa_drain_queue(RequestType *&rtype) {
    auto &group = groups[rtype->type_group];
    uint64_t cur_tsc = rdtscp(NULL);
    while (rtype->rqueue_head > rtype->rqueue_tail and free_peers > 0) {
        PSP_DEBUG(
            "Dispatching " << rtype->rqueue_head - rtype->rqueue_tail
            << " " << req_type_str[static_cast<int>(rtype->type)]
        );
        uint32_t peer_id = MAX_WORKERS + 1;
        // Lookup for a core reserved to this type's group
        for (uint32_t i = 0; i < group.n_resas; ++i) {
            uint32_t candidate = group.res_peers[i];
            if ((1 << candidate) & free_peers) {
                peer_id = candidate;
                PSP_DEBUG("Using reserved core " << peer_id);
                break;
            }
        }
        // Otherwise attempt to steal worker
        if (peer_id == MAX_WORKERS + 1) {
            for (unsigned int i = 0; i < group.n_stealable; ++i) {
                uint32_t candidate = group.stealable_peers[i];
                if ((1 << candidate) & free_peers) {
                    peer_id = candidate;
                    PSP_DEBUG("Stealing core " << peer_id);
                    break;
                }
            }
        }
        // No peer found
        if (peer_id == MAX_WORKERS + 1) {
            return 0;
        }
        // Dispatch
        unsigned long req = rtype->rqueue[rtype->rqueue_tail & (RQUEUE_LEN - 1)];
        if (likely(lrpc_ctx.push(req, peer_id)) == 0) {
            num_dped++;
            rtype->rqueue_tail++;
            free_peers ^= (1 << peer_id);
            if (dp == DARC)
                peer_dpt_tsc[peer_id] = cur_tsc;
            /*
            PSP_DEBUG(
                "Picked peer " << peer_id << " . " << __builtin_popcount(free_peers) << " free peers"
            );
            */
        }
    }

    return 0;
}

int Dispatcher::drain_queue(RequestType *&rtype) {
    uint64_t cur_tsc = rdtscp(NULL);
    while (rtype->rqueue_head > rtype->rqueue_tail and free_peers > 0) {
        unsigned long req = rtype->rqueue[rtype->rqueue_tail & (RQUEUE_LEN - 1)];
        uint32_t peer_id = __builtin_ctz(free_peers);
        if (likely(lrpc_ctx.push(req, peer_id)) == 0) {
            num_dped++;
            rtype->rqueue_tail++;
            free_peers ^= (1 << peer_id);
            peer_dpt_tsc[peer_id] = cur_tsc;
            /*
            PSP_DEBUG(
                "Picked peer " << peer_id << " . " << __builtin_popcount(free_peers) << " free peers"
            );
            */
        }
    }
    return 0;
}
