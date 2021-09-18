#ifndef COMMON_H_
#define COMMON_H_

#include <psp/libos/persephone.hh>

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <sys/types.h>

#include "schedule.hh"

static inline void read_cmds(std::vector<std::string> &requests_str, std::string const &cmd_list) {
    assert(!cmd_list.empty());
    /* Loop-over commands file to create requests */
    std::ifstream cmdfile(cmd_list.c_str());
    //FIXME: this does not complain when we give a directory, rather than a file
    if (cmdfile.bad() || !cmdfile.is_open()) {
        PSP_ERROR("Failed to open cmd list file " << cmd_list);
        exit(1);
    }
    std::string cmd;
    while (std::getline(cmdfile, cmd)) {
        requests_str.push_back(cmd);
    }
}

namespace bpo = boost::program_options;
static int parse_args(int argc, char **argv, bpo::options_description &opts) {
    opts.add_options()
        ("help", "produce help message");

    bpo::variables_map vm;
    try {
        bpo::parsed_options parsed =
            bpo::command_line_parser(argc, argv).options(opts).run();
        bpo::store(parsed, vm);
        if (vm.count("help")) {
            std::cerr << opts << std::endl;
            return -1;
        }
        notify(vm);
    } catch (const bpo::error &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << opts << std::endl;
        return -1;
    }

    return 0;
}

template <typename T>
int parse_config(std::string &app_cfg, std::vector<UdpContext *> &net_contexts,
                 std::vector<uint32_t> &cpus, struct rte_mempool **net_mempool,
                 std::vector<uint16_t> remote_ports, std::vector<std::string> &remote_ips,
                 std::vector<std::shared_ptr<T>> &client_workers,
                 int max_concurrency) {
    try {
        std::vector<uint32_t> cpus_v;
        YAML::Node config = YAML::LoadFile(app_cfg);
        if (config["log_dir"].IsDefined()) {
            log_dir = config["log_dir"].as<std::string>();
        }
        if (config["cpus"].IsDefined()) {
            cpus = config["cpus"].as<std::vector<uint32_t> >();
        } else {
            PSP_ERROR("Operator must register CPUs to use with this application");
            exit(ENODEV);
        }
        std::string remote_mac = config["network"]["remote_mac"].as<std::string>();
        std::string mac = config["network"]["mac"].as<std::string>();
        uint16_t port_id = config["network"]["device_id"].as<uint16_t>();
        size_t n_net_workers = 0;
        if (config["net_workers"].IsDefined()) {
            YAML::Node net_workers = config["net_workers"];
            if (net_workers.size() > cpus.size()) {
                PSP_ERROR("Not enough service units to accomodate net workers");
            }
            n_net_workers = net_workers.size();
            for (size_t i = 0; i < n_net_workers; ++i) {
                uint16_t port = 32768 + (i % (UINT16_MAX - 32768));
                struct in_addr ip;
                inet_aton(net_workers[i]["ip"].as<std::string>().c_str(), &ip);
                UdpContext *ctx = new UdpContext(
                    i, ip, port, port_id, net_mempool, mac
                );

                struct in_addr remote_ip;
                inet_aton(remote_ips[i % remote_ips.size()].c_str(), &remote_ip);

                ctx->remote_port = remote_ports[i % remote_ports.size()];
                ctx->remote_ip = remote_ip;
                rte_ether_unformat_addr(remote_mac.c_str(), &ctx->remote_mac);
                net_contexts.push_back(ctx);
            }
        } else {
            PSP_ERROR("Operator must register at least one net worker.");
            exit(ENODEV);
        }

        if (config["schedules"].IsDefined()) {
            YAML::Node cfg_schedules = config["schedules"];
            for (size_t n = 0; n < n_net_workers; ++n) {
                std::vector<std::unique_ptr<Schedule>> wrkr_schedules;
                std::vector<ClientRequest *> wrkr_requests;
                //FIXME we should really be parsing these only once...
                for (size_t i = 0; i < cfg_schedules.size(); ++i) {
                    std::unique_ptr<Schedule> sched(new Schedule(i));
                    sched->rate = cfg_schedules[i]["rate"].as<double>() / n_net_workers;
                    sched->duration = boost::chrono::seconds(cfg_schedules[i]["duration"].as<uint64_t>());
                    sched->max_duration = boost::chrono::seconds(cfg_schedules[i]["duration"].as<uint64_t>() + 5);
                    sched->uniform = cfg_schedules[i]["uniform"].as<bool>();
                    sched->ptype = str_to_ptype(cfg_schedules[i]["ptype"].as<std::string>());
                    if (sched->ptype == pkt_type::RAW) {
                        PSP_TRUE(EINVAL, cfg_schedules[i]["pkt_size"].IsDefined());
                        sched->pkt_size = cfg_schedules[i]["pkt_size"].as<uint64_t>();
                    }
                    sched->cmd_ratios = cfg_schedules[i]["cmd_ratios"].as<std::vector<double>>();
                    if (cfg_schedules[i]["cmd_lists"].IsDefined()) {
                        auto &cmd_lists = cfg_schedules[i]["cmd_lists"].as<std::vector<std::string>>();
                        PSP_TRUE(EINVAL, cmd_lists.size() == sched->cmd_ratios.size());
                        sched->requests_str.resize(cmd_lists.size());
                        for (size_t j = 0; j < cmd_lists.size(); ++j) {
                            read_cmds(sched->requests_str[j], cmd_lists[j]);
                            PSP_TRUE(EINVAL, sched->requests_str[j].size() > 0);
                        }
                        if (sched->ptype == pkt_type::IX or sched->ptype == pkt_type::PSP_MB) {
                            for (auto &r: sched->requests_str) {
                                sched->reqs_us.push_back(std::stoi(r[0]));
                            }
                        }
                    } else {
                        PSP_TRUE(ENOENT, cfg_schedules[i]["cmd_mean_ns"].IsDefined());
                        auto &cmd_mean_ns = cfg_schedules[i]["cmd_mean_ns"].as<std::vector<uint32_t>>();
                        PSP_TRUE(EINVAL, cmd_mean_ns.size() == sched->cmd_ratios.size());
                        for (auto &m: cmd_mean_ns) {
                            sched->reqs_us.push_back(m);
                        }
                    }

                    // Generate the actual schedule
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    sched->gen_schedule(gen, wrkr_requests);
                    wrkr_schedules.push_back(std::move(sched));
                }

                // Create the worker
                std::shared_ptr<T> w = std::make_shared<T>(
                    max_concurrency, wrkr_schedules, std::move(wrkr_requests)
                );
                client_workers.push_back(w);
            }
        } else {
            PSP_INFO("No schedule given in config file. Falling back on arguments");
        }
    } catch (YAML::ParserException& e) {
        std::cout << "Failed to parse config: " << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
#endif // COMMON_H_
