#pragma once
#ifndef NCCLIENT_H_
#define NCCLIENT_H_

#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <memory>
#include <csignal>
#include <vector>

#include <iostream>

#include <psp/libos/persephone.hh>
#include "common.hh"
#include "msg.hh"
#include "schedule.hh"

#include <rte_mbuf.h>

class NCClient : public Worker {
    // Log stuff
    private: ClientRequest cr;

    public: NCClient():
        Worker(WorkerType::CLIENT) {
            cr.mbuf = nullptr;
        }

    public: NCClient(
        int m, std::vector<std::unique_ptr<Schedule>> &scheds,
        std::vector<ClientRequest *> &&reqs) : Worker(WorkerType::CLIENT) {}

    public: ~NCClient() {}

    public: int setup() override {
        // Init a connection to remote host
        return 0;
    }

    // To fill vtable entry
    private: int process_request(unsigned long payload) override {
        return ENOTSUP;
    }

    private: int dequeue(unsigned long *payload) {
        return 0;
    }

    private: int send_request() {
        if (udp_ctx->push_head - udp_ctx->push_tail == OUTBOUND_Q_LEN) {
            return EAGAIN;
        }
        unsigned long payload = (unsigned long) cr.mbuf;
        udp_ctx->outbound_queue[udp_ctx->push_head++ & (OUTBOUND_Q_LEN - 1)] = payload;
        PSP_OK(udp_ctx->send_packets());
        return 0;
    }

    private: int work(int status, unsigned long payload) override {
        while (true) {
            if (terminate)
                return 0;
            // Get command from user
            std::string command;
            std::cin >> command; // FIXME blocks and can't catch SIGKILL
            if (command == "quit") {
                return 0;
            }

            // Send it to remote host
            char *payload_addr = nullptr;
            udp_ctx->get_mbuf(&cr.mbuf, &payload_addr);
            memcpy(payload_addr + NET_HDR_SIZE, command.c_str(), command.size());
            reinterpret_cast<rte_mbuf *>(cr.mbuf)->l4_len = command.size();

            PSP_OK(send_request());

            // Wait for answer
            while (udp_ctx->recv_packets() == EAGAIN) {
                if (terminate)
                    return 0;
            };
            PSP_TRUE(ENOENT, udp_ctx->pop_head > udp_ctx->pop_tail);
            unsigned long req = udp_ctx->inbound_queue[udp_ctx->pop_tail++ & (INBOUND_Q_LEN - 1)];

            // Parse
            std::string answer(rte_pktmbuf_mtod_offset(
                static_cast<struct rte_mbuf *>((void *)req), const char*, NET_HDR_SIZE
            ));

            // Print out answer
            std::cout << answer << std::endl;
            std::istringstream iss(answer);
            std::string item;
            while (std::getline(iss, item, ',')) {
                std::cout << item << std::endl;
            }

            PSP_OK(udp_ctx->free_mbuf(req));
        }
    }
};
#endif //NCCLIENT_H_
