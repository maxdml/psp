#pragma once
#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include "msg.hh"
#include <random>

uint16_t req_offset = 1;

enum class pkt_type {
    PSP_MB = 0,
    IX,
    RAW,
    UNKNOWN
};

[[gnu::unused]] static enum pkt_type str_to_ptype(std::string const &type) {
   if (type == "PSP_MB") {
        return pkt_type::PSP_MB;
    } else if (type == "IX") {
        return pkt_type::IX;
    } else if (type == "RAW") {
        return pkt_type::RAW;
    }
    return pkt_type::UNKNOWN;
}

class Schedule {
    public: uint32_t schedule_id;
    public: boost::chrono::seconds duration;
    public: boost::chrono::seconds max_duration;
    public: tp start_time;
    public: tp end_time;
    public: tp last_send_time;
    public: uint32_t n_requests = 0;
    public: double rate = 0;
    public: bool uniform = false;
    public: enum pkt_type ptype;
    public: uint64_t pkt_size = 0;
    public: std::vector<double> cmd_ratios;
    public: std::vector<std::vector<std::string>> requests_str; // One day I could use these again to store full payload (e.g. HTTP)
    public: std::vector<uint32_t> reqs_us;
    public: std::vector<tp> send_times;
    public: uint32_t send_index = 0;
    public: uint32_t recv_requests = 0;
    public: uint32_t attempts = 0;
    public: uint32_t ev_count = 0;
    public: uint32_t n_skipped = 0;

    public: Schedule (uint32_t i) : schedule_id(i) {}

    // Goodput
    public: double getRequestsPerSecond() {
        return ((double)recv_requests) / (ns_diff(start_time, end_time)/ 1e9);
    }

    // Throughput
    public: double getOfferedLoad() {
        return ((double)send_index) / (ns_diff(start_time, last_send_time)/ 1e9);
    }

    public: int gen_schedule(std::mt19937 &seed, std::vector<ClientRequest *> &requests) {
        std::exponential_distribution<double> exp_dist(rate / 1e9);
        std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
        uint32_t base_rid = requests.size();
        tp start_time = take_time();
        uint64_t time = 0;
        uint64_t end_time = duration.count() * 1e9;
        int type_counts[static_cast<int>(ReqType::LAST)];
        for (int i = 0; i < static_cast<int>(ReqType::LAST); ++i) {
            type_counts[i] = 0;
        }
        while (time <= end_time) {
            // Select request type (0: short, 1:long)
            double r = uniform_dist(seed);
            int cmd_idx = -1;
            for (size_t i = 0; i < cmd_ratios.size(); ++i) {
                if (r < cmd_ratios[i]) {
                    cmd_idx = i;
                    break;
                } else {
                    r -= cmd_ratios[i];
                }
            }
            likely((cmd_idx >= 0)) ? (void)0 : abort();
            type_counts[req_offset + cmd_idx]++;

            //Pick interval
            if (uniform) {
                double interval_ns = 1e9*1.0 / rate;
                time += interval_ns;
            } else {
                uint64_t next_ns = exp_dist(seed);
                time += next_ns;
            }
            boost::chrono::nanoseconds send_time(time);
            send_times.push_back(send_time + start_time); // Fill send time
            // Generate the request itself
            uint32_t rid = base_rid + n_requests++;
            ClientRequest *cr = new ClientRequest();
            requests.push_back(cr);
            PSP_TRUE(EINVAL, requests[rid] == cr);
            cr->id = rid;
            cr->type = static_cast<ReqType>(req_offset + cmd_idx);
            cr->mbuf = nullptr; // filled at send time
            switch (ptype) {
                case pkt_type::PSP_MB:
                case pkt_type::IX:
                    cr->run_ns = reqs_us[cmd_idx];
                    break;
                default:
                    cr->run_ns = 0;
            }
            cr->schedule_id = schedule_id;
        }
        PSP_INFO("Created " << n_requests << " requests spanning " << duration << ":");
        for (int i = 0; i < static_cast<int>(ReqType::LAST); ++i) {
            if (type_counts[i] > 0) {
                PSP_INFO(req_type_str[i] << ": " << type_counts[i]);
            }
        }
        return 0;
    }
};

#endif // SCHEDULE_H_
