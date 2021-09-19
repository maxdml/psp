#pragma once
#ifndef CLIENT_H_
#define CLIENT_H_

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <memory>
#include <csignal>
#include <vector>
#include <random>
#include <unordered_map>
#include <dirent.h>
#include <boost/functional/hash.hpp>

#include <iostream>
#include <fstream>

#include <psp/libos/persephone.hh>
#include <psp/hist.hh>
#include "msg.hh"
#include "schedule.hh"
#include "common.hh"

#include <rte_mbuf.h>

template <typename T>
void sample_into(std::vector<T> &from, std::vector<T>&to,
                 auto lat_get,   // Must match form: ComparableType lat_get(const T&) {}
                 auto time_get,  // Must match form: ComparableType time_get(const T&) {}
                 size_t n) {
    if (from.empty()) {
        return;
    }
    auto time_cmp = [&](T&a, T&b) { return time_get(*a) < time_get(*b); };
    auto lat_cmp = [&](T&a, T&b) { return lat_get(*a) < lat_get(*b); };

    if (from.size() <= n) {
        to.insert(to.end(),
                  std::make_move_iterator(from.begin()),
                  std::make_move_iterator(from.end()));
        if (time_get) {
            std::sort(to.begin(), to.end(), time_cmp);
        }
        return;
    }

    std::sort(from.begin(), from.end(), lat_cmp);
    int spacing = from.size() / n;
    if (spacing == 0) {
        spacing = 1;
    }
    to.push_back(std::move(from[0]));
    for (unsigned int i = spacing; i < from.size() - 1; i+=spacing) {
        to.push_back(std::move(from[i]));
    }
    to.push_back(std::move(from[from.size() - 1]));
    if (time_get) {
        std::sort(to.begin(), to.end(), time_cmp);
    }
}

#define SCHED_SIZE 50000000
class Client : public Worker {
    // Log stuff
    private: Histogram_t hist;

    // Workload stuff
    public: std::vector<ClientRequest *> requests;
    private: uint32_t send_offset = 0;

    // Schedules
    public: std::vector<std::unique_ptr<Schedule>> schedules;
    private: std::vector<tp>::iterator send_time_it;
    private: tp next_send_time;
    private: boost::chrono::nanoseconds start_offset;
    private: ClientRequest *next_cr;

    // Init params
    private: int max_concurrency = 1;

    public: Client(
        int m, std::vector<std::unique_ptr<Schedule>> &scheds,
        std::vector<ClientRequest *> &&reqs) :
        Worker(WorkerType::CLIENT), requests(reqs), max_concurrency(m) {
            for (auto &s: scheds) {
                schedules.push_back(std::move(s));
            }
        }

    public: ~Client() {
        for (size_t i = 0; i < schedules.size(); ++i) {
            auto &s = schedules[i];
            log_info(
                "Duration: %.02lfs -> Sent: %u, Received: %u,"
                " %u sent but not answered, %u behind schedule, "
                " %u skipped, %u events processed, %u send attempts",
                ns_diff(s->start_time, s->end_time) / 1e9,
                s->send_index + 1, s->recv_requests,
                (s->send_index + 1) - s->recv_requests,
                s->n_requests - (s->send_index + 1),
                s->n_skipped, s->ev_count, s->attempts
            );
        }
        for (ClientRequest *req: requests) {
            delete req;
        }
    }

    public: int setup() override {
        PSP_NOTNULL(EPERM, udp_ctx);
        return 0;
    }

    private: int send_request(ClientRequest *cr) {
        if (udp_ctx->push_head - udp_ctx->push_tail == OUTBOUND_Q_LEN) {
            return EAGAIN;
        }
        //PSP_DEBUG("Sending request " << cr->id);
        unsigned long payload = (unsigned long) cr->mbuf;
        udp_ctx->outbound_queue[udp_ctx->push_head++ & (OUTBOUND_Q_LEN - 1)] = payload;
        PSP_OK(udp_ctx->send_packets());
        cr->sending = rdtscp(NULL);
        return 0;
    }

    // To fill vtable entry
    private: int process_request(unsigned long payload) override {
        return ENOTSUP;
    }

    private: int dequeue(unsigned long *payload) {
        return 0;
    }

    private: void gen_next_send_time() {
        next_send_time = (*send_time_it + start_offset);
        send_time_it++;
    }

    private: int gen_next_req(Schedule *sched) {
        if (likely(next_cr == nullptr)) {
            next_cr = requests[sched->send_index + send_offset];
            next_cr->id ^= (worker_id << 27); // XOR should be equivalent to OR here?
            char *payload_addr = nullptr;
            PSP_NULL(EINVAL, next_cr->mbuf);
            if (unlikely(udp_ctx->get_mbuf(&next_cr->mbuf, &payload_addr) == ENOMEM))
                return EAGAIN;
            switch (sched->ptype) {
                case pkt_type::IX: {
                    IXMessage req = {
                        .type = static_cast<uint16_t>(next_cr->type),
                        .seq_num = 42,
                        .queue_length = {0, 0, 0},
                        .client_id = static_cast<uint16_t>(worker_id),
                        .req_id = next_cr->id,
                        .pkts_length = sizeof(IXMessage),
                        .runNs = next_cr->run_ns,
                        .genNs = 0
                    };
                    memcpy(payload_addr + NET_HDR_SIZE, static_cast<void *>(&req), sizeof(req));
                    reinterpret_cast<rte_mbuf *>(next_cr->mbuf)->l4_len = sizeof(req);
                    break;
                }
                case pkt_type::RAW: {
                    char *id_addr = payload_addr + NET_HDR_SIZE;
                    *reinterpret_cast<uint32_t *>(id_addr) = next_cr->id;
                    memset(id_addr + sizeof(uint32_t), 1, sched->pkt_size - sizeof(uint32_t));
                    reinterpret_cast<rte_mbuf *>(next_cr->mbuf)->l4_len = sched->pkt_size;
                    break;
                }
                case pkt_type::PSP_MB: {
                    char *id_addr = payload_addr + NET_HDR_SIZE;
                    *reinterpret_cast<uint32_t *>(id_addr) = next_cr->id;
                    char *type_addr = id_addr + sizeof(uint32_t);
                    *reinterpret_cast<uint32_t *>(type_addr) = static_cast<uint32_t>(next_cr->type);
                    char *req_addr = type_addr +  sizeof(uint32_t);
                    *reinterpret_cast<uint32_t *>(req_addr) = sizeof(uint32_t);
                    *reinterpret_cast<uint32_t *>(req_addr + sizeof(uint32_t)) = next_cr->run_ns;
                    reinterpret_cast<rte_mbuf *>(next_cr->mbuf)->l4_len = sizeof(uint32_t) * 4;
                    break;
                }
                default:
                    PSP_UNREACHABLE();
            }
        }
        return 0;
    }

    private: int work(int status, unsigned long payload) override {
        for (auto &s: schedules) {
            PSP_OK(play_schedule(s.get()));
        }
        terminate = true;
        return 0;
    }

    private: int play_schedule(Schedule *sched) {
        // Adjust start time
        sched->start_time = take_time();
        start_offset = boost::chrono::duration_cast<boost::chrono::nanoseconds>(
            sched->start_time - sched->send_times[0]
        );
        send_time_it = sched->send_times.begin();
        sched->end_time = sched->send_times.back() + start_offset;
        PSP_DEBUG("Start time: " << since_epoch(sched->start_time)/1e9 << " s");
        PSP_DEBUG("start offset: " << start_offset.count()/1e9 << " s");
        PSP_DEBUG("Schedule start time : " << (since_epoch(sched->send_times[0]) + start_offset.count())/1e9 << " s");
        PSP_DEBUG("Schedule end time : " << since_epoch(sched->end_time)/1e9 << " s");
        next_cr = nullptr;
        gen_next_send_time();
        bool sending_complete = false;
        tp iter_time = take_time();
        int status = EAGAIN;
        terminate = false;
        PSP_INFO("Running schedule for " << ns_diff(sched->send_times[0], sched->send_times.back()) / 1e9);
        while (sched->recv_requests < sched->n_requests && !terminate) {
            /* While we are not due a request, check for answers */
            status = EAGAIN;
            do {
                if ((iter_time - sched->start_time) >= sched->max_duration) {
                    PSP_INFO(
                        "Terminating the schedule because " \
                        "(iter_time - sched->start_time) >= sched->max_duration"
                    );
                    terminate = true;
                    break;
                }
                status = udp_ctx->recv_packets();
                iter_time = take_time();
                sched->ev_count++;
            } while (status == EAGAIN and iter_time < next_send_time);
            int inflight = sched->send_index - sched->recv_requests;
            /* While we are due a request */
            while (iter_time >= next_send_time and iter_time <= sched->end_time and sched->send_index < sched->n_requests) {
                sched->attempts++;
                if (max_concurrency < 0 or (max_concurrency > inflight)) {
                    if (gen_next_req(sched) == 0) {
                        PSP_NOTNULL(EINVAL, next_cr);
                        int rtn = send_request(next_cr);
                        if (rtn == 0) {
                            sched->send_index++;
                            // reset next_cr so we can generate a new one
                            next_cr = nullptr;
                        } else if (rtn == EAGAIN) {
                            // Push queue is full. Need to drain it.
                            sched->n_skipped++;
                            break;
                        } else {
                            PSP_UNREACHABLE();
                        }
                    } else {
                        // Likely no more mbufs available. Need to drain push queue.
                        sched->n_skipped++;
                        break;
                    }
                } else {
                    // Max concurrency reached
                    sched->n_skipped++;
                }
                gen_next_send_time();
                if (next_send_time >= sched->end_time) {
                    PSP_INFO("Sending complete");
                    sending_complete = true;
                }
                inflight = sched->send_index - sched->recv_requests;
            }
            if (inflight == 0 and (sending_complete or iter_time > sched->end_time)) {
                PSP_INFO(
                    "Terminating the schedule because " \
                    " sending complete or iter_time > sched->end_time"
                );
                terminate = true;
                continue;
            }
            /* We dequeued some answers from the NIC */
            if (status == 0) {
                PSP_DEBUG("Attempting to dequeue up to " << udp_ctx->pop_head - udp_ctx->pop_tail);
                uint64_t pop_time = rdtscp(NULL);
                size_t batch_dequeued = 0;
                while (udp_ctx->pop_head > udp_ctx->pop_tail and batch_dequeued < MAX_RX_BURST) {
                    unsigned long req = udp_ctx->inbound_queue[udp_ctx->pop_tail++ & (INBOUND_Q_LEN - 1)];
                    uint32_t rid;
                    switch (sched->ptype) {
                        case pkt_type::IX: {
                            IXMessage *msg = rte_pktmbuf_mtod_offset(
                                static_cast<rte_mbuf *>((void*)req), IXMessage *, NET_HDR_SIZE
                             );
                            rid = msg->req_id;
                            break;
                        }
                        default: {
                            rid = *rte_pktmbuf_mtod_offset(
                                static_cast<rte_mbuf *>((void*)req), uint32_t *, NET_HDR_SIZE
                             );
                        }
                    }
                    /*
                    std::cout << *(rte_ipv4_hdr*)(((rte_mbuf *)req)->buf_addr+128+14) << std::endl;
                    std::cout << *(rte_udp_hdr*)(((rte_mbuf *)req)->buf_addr+128+34) << std::endl;
                    */
                    auto &cr = requests[rid ^ (worker_id << 27)];
                    PSP_DEBUG(
                        "[client " << worker_id << "] received response to "
                        << rid << "(" << (rid ^ (worker_id << 27)) << ")"
                    );
                    cr->completed = pop_time;
                    PSP_OK(udp_ctx->free_mbuf(req));
                    batch_dequeued++;
                }
                sched->recv_requests += batch_dequeued;
            }
        }
        if (sending_complete) {
            sched->last_send_time = sched->end_time;
        } else {
            sched->last_send_time = *send_time_it + start_offset;
        }
        sched->end_time = take_time();
        send_offset += sched->n_requests;
        PSP_INFO("Ran schedule for " << ns_diff(sched->start_time, sched->end_time) / 1e9);
        return 0;
    }

    public: int log_latency(std::ostream &output, std::ostream &hist_output, int downsample) {
        if (requests.size() == 0) {
            return ENOENT;
        }
        if (downsample > 0) {
            std::unordered_map<enum ReqType, std::vector<ClientRequest *>> requests_by_type{};
            for (ClientRequest *req: requests) {
                if (requests_by_type.find(req->type) == requests_by_type.end()) {
                    requests_by_type[req->type] = std::vector<ClientRequest *>{};
                }
                requests_by_type[req->type].push_back(req);
            }
            std::vector<ClientRequest *> downsampled_requests;
            for (auto &reqtype : requests_by_type) {
                sample_into(reqtype.second, downsampled_requests,
                            req_latency, req_time, downsample);
            }
            for (auto &req : downsampled_requests) {
                output << worker_id << "\t" << *req << std::endl;
            }
        } else if (downsample == 0) {
            for (ClientRequest *req: requests) {
                output << worker_id << "\t" << *req << std::endl;
            }
        } else if (downsample == -1) {
            // Discard first 10%
            std::vector<ClientRequest *> pruned_requests(requests.begin() + requests.size() * 0.1, requests.end());
            //std::vector<ClientRequest *> pruned_requests(requests.begin()+1e5, requests.end());

            // Short all requests by response time
            auto lat_cmp = [](ClientRequest *a, ClientRequest *b) { return req_latency(*a) < req_latency(*b); };
            std::sort(pruned_requests.begin(), pruned_requests.end(), lat_cmp);

            // Histograms
            PSP_INFO("Processing " << pruned_requests.size() << " samples");
            hist.buckets.clear(); // Not sure why I have to clean it there?
            hist.min = 0; hist.max = 0; hist.count = 0; hist.total = 0;
            for (uint64_t i = 0; i < pruned_requests.size(); ++i) {
                if (pruned_requests[i]->completed == 0) {
                    continue;
                }
                // Store values in nanoseconds
                insert_value(&hist, (pruned_requests[i]->completed - pruned_requests[i]->sending) / cycles_per_ns);
            }
            if (hist.count == 0) {
                PSP_WARN("No values inserted in histogram");
                return -1;
            }
            // First line is histo for all samples
            hist_output << "TYPE\tMIN\tMAX\tCOUNT\tTOTAL";
            for (uint64_t i = 0; i < hist.buckets.size(); ++i) {
                if (hist.buckets[i] > 0) {
                    hist_output << "\t" << i * 1000;
                }
            }
            hist_output << std::endl;
            hist_output << "UNKNOWN\t"
                        << hist.min << "\t"
                        << hist.max << "\t"
                        << hist.count << "\t"
                        << hist.total;
            for (unsigned i = 0; i < hist.buckets.size(); ++i) {
                if (hist.buckets[i] > 0) {
                    hist_output << "\t" << hist.buckets[i];
                }
            }
            hist_output << std::endl;
            check_hist(pruned_requests, hist.count - 1);
            // Then one line per request type
            std::unordered_map<enum ReqType, std::vector<ClientRequest *>> requests_by_type{};
            for (uint64_t i = 0; i < pruned_requests.size(); ++i) {
                if (pruned_requests[i]->completed == 0) {
                    continue;
                }
                auto &req_ptr = pruned_requests[i];
                if (requests_by_type.find(req_ptr->type) == requests_by_type.end()) {
                    requests_by_type[req_ptr->type] = std::vector<ClientRequest *>{};
                }
                requests_by_type[req_ptr->type].push_back(req_ptr);
            }
            for (auto &rtype: requests_by_type) {
                hist.buckets.clear();
                hist.min = 0; hist.max = 0; hist.count = 0; hist.total = 0;
                auto reqs = rtype.second;
                for (uint64_t i = 0; i < reqs.size(); ++i) {
                    // Store values in nanoseconds
                    insert_value(&hist, (reqs[i]->completed - reqs[i]->sending) / cycles_per_ns);
                }
		if (hist.count == 0) {
		    PSP_WARN(
			"No values inserted in histogram for type "
                        << req_type_str[static_cast<int>(rtype.first)]
		    );
                    continue;
		}
                hist_output << "TYPE\tMIN\tMAX\tCOUNT\tTOTAL";
                for (uint64_t i = 0; i < hist.buckets.size(); ++i) {
                    if (hist.buckets[i] > 0) {
                        hist_output << "\t" << i * 1000;
                    }
                }
                hist_output << std::endl;
                hist_output << req_type_str[static_cast<int>(rtype.first)] << "\t"
                            << hist.min << "\t"
                            << hist.max << "\t"
                            << hist.count << "\t"
                            << hist.total;
                for (unsigned i = 0; i < hist.buckets.size(); ++i) {
                    if (hist.buckets[i] > 0) {
                        hist_output << "\t" << hist.buckets[i];
                    }
                }
                hist_output << std::endl;
                PSP_TRUE(EINVAL, hist.count == reqs.size());

                check_hist(reqs, reqs.size() - 1);
            }
        }
        return 0;
    }

    public: int log_throughput(std::ostream &output, long unsigned int epoch_size_ns, uint64_t first_cplted) {
        std::unordered_map<std::pair<enum ReqType, int>,
                           long unsigned int,
                           boost::hash<std::pair<enum ReqType, int>>> type_epoch_counts;
        for (ClientRequest *req: requests) {
            if (req->completed == 0)
                continue;
            auto type_epoch = std::pair(req->type, ((req->completed / cycles_per_ns) - first_cplted) / epoch_size_ns);
            if (type_epoch_counts.find(type_epoch) == type_epoch_counts.end()) {
                type_epoch_counts[type_epoch] = 0;
            }
            type_epoch_counts[type_epoch]++;
        }
        std::cout << " identified " << type_epoch_counts.size() << " epochs" << std::endl;
        for (auto &it : type_epoch_counts) {
            output << worker_id << "\t"
                   << (it.first.second * epoch_size_ns) << "\t"
                   << req_type_str[static_cast<int>(it.first.first)] << "\t"
                   << it.second << "\n";
        }
        return 0;
    }

    public: static uint64_t req_latency(const ClientRequest &req) {
        if (req.completed == 0) {
            return LLONG_MAX;
        } else {
            return (req.completed - req.sending) / cycles_per_ns;
        }
    }

    public: static uint64_t req_time(const ClientRequest &req) {
        return req.sending / cycles_per_ns;
    }

    private: int check_hist(std::vector<ClientRequest *> &reqs, uint64_t nr) {
        std::cout << "======================================" << std::endl;
        std::cout << "Hist min: " << hist.min
                  << ". Min: " << (reqs[0]->completed - reqs[0]->sending) / cycles_per_ns << std::endl;
        uint32_t c = 0;
        for (uint32_t i = 0; i < hist.buckets.size(); ++i) {
            if (hist.buckets[i] == 0)
                continue;
            if (c + hist.buckets[i] >= hist.count * .25 and c < hist.count * .25) {
                std::cout << std::fixed << "Hist 25th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 25th: "
                          << (reqs[nr*25/100]->completed - reqs[nr*25/100]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .5 and c < hist.count * .5) {
                std::cout << std::fixed << "Hist 50th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 50th: "
                          << (reqs[nr/2]->completed - reqs[nr/2]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .75 and c < hist.count * .75) {
                std::cout << std::fixed << "Hist 75th: " <<  (i*1000 + (i+1)*1000) / 2.0
                //std::cout << "Hist 75th: " << ((1UL << i) + (1UL << (i-1))) / 2.0
                          << ". 75th: "
                          << (reqs[nr*75/100]->completed - reqs[nr*75/100]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .9 and c < hist.count * .9) {
                std::cout << std::fixed << "Hist 90th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 90th: "
                          << (reqs[nr*90/100]->completed - reqs[nr*90/100]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .99 and c < hist.count * .99) {
                std::cout << std::fixed << "Hist 99th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 99th: "
                          << (reqs[nr*99/100]->completed - reqs[nr*99/100]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .999 and c < hist.count * .999) {
                std::cout << std::fixed << "Hist 99.9th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 99.9th: "
                          << (reqs[nr*999/1000]->completed - reqs[nr*999/1000]->sending) / cycles_per_ns
                          << std::endl;
            }
            if (c + hist.buckets[i] >= hist.count * .9999 and c < hist.count * .9999) {
                std::cout << std::fixed << "Hist 99.99th: " <<  (i*1000 + (i+1)*1000) / 2.0
                          << ". 99.99th: "
                          << (reqs[nr*9999/10000]->completed - reqs[nr*9999/10000]->sending) / cycles_per_ns
                          << std::endl;
            }
            c += hist.buckets[i];
        }
        PSP_TRUE(EINVAL, c == hist.count);

        std::cout << "Hist max: " << hist.max
                  << ". Max: " << (reqs[nr]->completed - reqs[nr]->sending) / cycles_per_ns << std::endl;
        std::cout << "Hist count: " << c << ". Num requests: " << nr+1 << std::endl;
        std::cout << "======================================" << std::endl;
        return 0;
    }
};
#endif //CLIENT_H_
