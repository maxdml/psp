#include "client.hh"

int log_worker_throughputs(std::ostream &output, std::ostream &rate_output,
                           std::vector<std::shared_ptr<Client>> &workers,
                           long unsigned int epoch_size_ns) {
    output << "W_ID\tTIME\tTYPE\tN" << std::endl;
    rate_output << "OFFERED\tACHIEVED" << std::endl;
    // We print 1 line per schedule
    std::map<size_t, std::pair<double, double>> sched_rates;
    for (auto &w: workers) {
        for (size_t i = 0; i < w->schedules.size(); ++i) {
            double offered_rate = w->schedules[i]->getOfferedLoad();
            double achieved_rate = w->schedules[i]->getRequestsPerSecond();
            if (sched_rates.find(i) == sched_rates.end()) {
                sched_rates[i] = std::pair<double,double>(offered_rate, achieved_rate);
            } else {
                sched_rates[i].first += offered_rate;
                sched_rates[i].second += achieved_rate;
            }
        }
        //FIXME: this assumes the first request of the first schedule completed first
        w->log_throughput(output, epoch_size_ns, since_epoch(*w->schedules[0]->send_times.begin()));
        output.flush();
    }

    for (auto &s: sched_rates) {
        PSP_INFO("THROUGHPUT/GOODPUT IN REQUESTS PER SECOND:");
        PSP_INFO(
            "[Sched " << s.first << "] throughput: "
            << s.second.first << ", goodput: " << s.second.second
        );
        rate_output << s.second.first << "\t" << s.second.second << std::endl;
    }
    return 0;
}

int log_worker_latencies(std::ostream &output, std::ostream &hist_output,
                         std::vector<std::shared_ptr<Client>> &client_workers,
                         int downsample) {
    output << "W_ID\t" << ClientRequest::LOG_COLUMNS << std::endl;
    for (auto &w: client_workers) {
        w->log_latency(output, hist_output, downsample);
        output.flush();
    }
    return 0;
}

std::vector<std::shared_ptr<Client>> client_workers;
static void stop_all(int signo) {
    for (auto &w: client_workers) {
        w->stop();
    }
}

int main(int argc, char *argv[]) {
#ifdef LOG_DEBUG
        log_info("Starting rate client with LOG_DEBUG on");
#endif
    /* Parse options */
    uint64_t duration;
    double rate;
    int max_concurrency;
    std::string label, cfg_file;
    std::vector<uint16_t> remote_ports;
    std::vector<std::string> remote_hosts;
    std::vector<std::string> cmd_lists;
    std::vector<double> cmd_ratios;
    std::string output_filename;
    std::string output_dirname;
    bool no_reformat = false;
    bool uniform = false;
    bool mb_reqs = false;
    bool ix_reqs = false;
    uint64_t pkt_size = 0;
    int downsample = 0;
    unsigned int collect_logs = 0;
    namespace po = boost::program_options;
    po::options_description desc{"Rate loop client options"};
    desc.add_options()
        ("no-reformat,f", po::bool_switch(&no_reformat), "Do not apply http reformating on requests")
        ("uniform", po::bool_switch(&uniform), "Use a uniform sending rate")
        ("mb-reqs", po::bool_switch(&mb_reqs), "Generate microbenchmark requests with PSP protocol")
        ("ix-reqs", po::bool_switch(&ix_reqs), "Generate microbenchmark requests with IX protocol")
        ("pkt-size", po::value<uint64_t>(&pkt_size), "Fill packets with X bytes")
        ("req-offset", po::value<uint16_t>(&req_offset), "offset for requests ID")
        ("ip,I", po::value<std::vector<std::string>>(&remote_hosts)->multitoken()->required(), "server IPs")
        ("port,P", po::value<std::vector<uint16_t>>(&remote_ports)->multitoken()->required(), "server's port")
        ("duration,d", po::value<uint64_t>(&duration)->default_value(10), "running duration")
        ("cmd-list,u", po::value<std::vector<std::string>>(&cmd_lists)->multitoken(), "cmd lists")
        ("cmd-ratios,U", po::value<std::vector<double>>(&cmd_ratios)->multitoken(), "cmd ratios")
        ("label,l", po::value<std::string>(&label)->default_value("rateclient"), "experiment label")
        ("rate,r", po::value<double>(&rate), "Sending rate")
        ("max-concurrency,m", po::value<int>(&max_concurrency)->default_value(-1), "maximum number of in-flight requests")
        ("config-path,c", po::value<std::string>(&cfg_file)->required(), "path to configuration file")
        ("out,o", po::value<std::string>(&output_filename), "path to output file (defaults to log directory)")
        ("outdir,o", po::value<std::string>(&output_dirname), "name of output dir")
        ("collect-logs", po::value<unsigned int>(&collect_logs), "Activate log collection")
        ("sample,S", po::value<int>(&downsample), "-1: histogram, 0: full timeseries, >0: timeseries downsampled to this number");

    if (parse_args(argc, argv, desc)) {
        PSP_ERROR("Error parsing arguments");
        exit(-1);
    }

    pin_thread(pthread_self(), 0);

    // Init dpdk
    PSP_OK(dpdk_net_init(cfg_file.c_str()));

    // Parse config (init net contexts, setup schedules)
    struct rte_mempool *net_mempool = nullptr;
    std::vector<UdpContext *> client_net_contexts;
    std::vector<uint32_t> cpus;
    PSP_OK(parse_config(
        cfg_file, client_net_contexts, cpus, &net_mempool, remote_ports, remote_hosts,
        client_workers, max_concurrency
    ));

    // Fall back on CLI arguments to build a single schedule
    if (client_workers.empty()) {
        for (size_t i = 0; i < client_net_contexts.size(); ++i) {
            std::unique_ptr<Schedule> sched(new Schedule(0));
            // Extract commands from list
            PSP_TRUE(EINVAL, cmd_lists.size() == cmd_ratios.size());
            sched->requests_str.resize(cmd_lists.size());
            for (size_t i = 0; i < cmd_lists.size(); ++i) {
                read_cmds(sched->requests_str[i], cmd_lists[i]);
                PSP_TRUE(EINVAL, sched->requests_str[i].size() > 0);
            }
            sched->cmd_ratios = cmd_ratios;
            sched->rate = rate / client_net_contexts.size(); //Spread load across workers
            sched->duration = boost::chrono::seconds(duration);
            sched->max_duration = boost::chrono::seconds(duration + 5);
            sched->uniform = uniform;
            if (ix_reqs) {
                sched->ptype = pkt_type::IX;
            } else if (mb_reqs) {
                sched->ptype = pkt_type::PSP_MB;
            } else {
                sched->ptype = pkt_type::RAW;
                PSP_TRUE(EINVAL, pkt_size > 0);
                sched->pkt_size = pkt_size;
            }

            if (mb_reqs or ix_reqs) { // We expect 1 work time per request type
                for (auto &r: sched->requests_str) {
                    sched->reqs_us.push_back(std::stoi(r[0]));
                }
            }

            // Generate the actual schedule
            std::random_device rd;
            std::mt19937 gen(rd());
            std::vector<ClientRequest *> requests;
            sched->gen_schedule(gen, requests);
            std::vector<std::unique_ptr<Schedule>> schedules;
            schedules.push_back(std::move(sched));

            auto &ctx = client_net_contexts[i];
            std::shared_ptr<Client> w = std::make_shared<Client>(
                max_concurrency, schedules, std::move(requests)
            );
            w->eal_thread = true;
            w->cpu_id = cpus[i];
            w->udp_ctx = ctx;
            client_workers.push_back(w);
            log_info("Created worker %u", w->worker_id);
        }
    } else {
        for (size_t i = 0; i < client_workers.size(); ++i) {
            auto &w = client_workers[i];
            w->eal_thread = true;
            w->cpu_id = cpus[i];
            w->udp_ctx = client_net_contexts[i];
            PSP_INFO("Created worker " << w->worker_id);
        }
    }

    // Init the NIC
    PSP_INFO("Setting up NIC ports");
    if (init_dpdk_port(0, net_mempool, client_net_contexts.size(), client_net_contexts.size()) != 0) {
        exit(1);
    }
    for (size_t i = 0; i < client_net_contexts.size(); ++i) {
        auto &ctx = client_net_contexts[i];
        ctx->set_fdir();
    }

    /* Launch all the workers */
    for (auto &w: client_workers) {
        if (w->launch() != 0) {
            w->stop();
            break;
        }
    }

    if (std::signal(SIGINT, stop_all) == SIG_ERR)
        log_error("can't catch SIGINT");
    if (std::signal(SIGTERM, stop_all) == SIG_ERR)
        log_error("can't catch SIGTERM");

    /* Join threads */
    for (auto w : client_workers) {
        w->join();
    }

    if (collect_logs) {
        std::string log_path = log_dir + label;
        if (output_filename.size() == 0) {
            DIR *dir = opendir(log_path.c_str());
            if (!dir)
                std::cerr << "Log directory does not exist?" << std::endl;

            std::string trace_dir = "";
            if (output_dirname.size() == 0) { //legacy
                struct dirent *dp;
                while ((dp = readdir(dir)) != NULL) {
                    if (strncmp(dp->d_name, "client", 6) == 0 and strlen(dp->d_name) == 7) {
                        trace_dir = dp->d_name;
                        break;
                    }
                }
                if (trace_dir.empty()) {
                    trace_dir = "client";
                }
            } else {
                trace_dir = output_dirname;
            }
            trace_dir =  trace_dir + "/traces";
            output_filename = generate_log_file_path(label, trace_dir.c_str());
        }

        std::ofstream tput_file(output_filename + "_throughput");
        std::ofstream rate_file(output_filename + "_rates");
        if (log_worker_throughputs(tput_file, rate_file, client_workers, 1000000)) {
            PSP_ERROR("Could not log throughput to base output file " << output_filename);
        }
        rate_file.close();
        tput_file.close();

        std::ofstream lat_file(output_filename);
        std::ofstream hist_file(output_filename + "_hist");
        if (log_worker_latencies(lat_file, hist_file, client_workers, downsample)) {
            PSP_ERROR("Could not log latencies to base output file " << output_filename);
        }
        hist_file.close();
        lat_file.close();
    }

    // Manually call the dtor because otherwise schedules will have been destroyed before
    for (auto &w: client_workers) {
        w.reset();
    }

    print_dpdk_device_stats(0);
    return 0;
}
