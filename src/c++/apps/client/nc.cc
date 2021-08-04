#include "nc.hh"

std::shared_ptr<NCClient> w;
static void stop_nc(int signo) {
    w->stop();
}

int main(int argc, char *argv[]) {
#ifdef LOG_DEBUG
        log_info("Starting rate client with LOG_DEBUG on");
#endif
    /* Parse options */
    uint16_t remote_port;
    std::vector<std::string> remote_hosts;
    std::string cfg_file;
    namespace po = boost::program_options;
    po::options_description desc{"Rate loop client options"};
    desc.add_options()
        ("ip,I", po::value<std::vector<std::string> >(&remote_hosts)->multitoken()->required(), "server IPs")
        ("port,P", po::value<uint16_t>(&remote_port)->default_value(6789), "server's port")
        ("config-path,c", po::value<std::string>(&cfg_file)->required(), "path to configuration file");

    if (parse_args(argc, argv, desc)) {
        PSP_ERROR("Error parsing arguments");
        exit(-1);
    }

    pin_thread(pthread_self(), 0);

    // Create a net context
    PSP_OK(dpdk_net_init(cfg_file.c_str()));
    struct rte_mempool *net_mempool = nullptr;
    std::vector<UdpContext *> client_net_contexts;
    std::vector<uint32_t> cpus;
    std::vector<uint16_t> remote_ports(1, remote_port);
    std::vector<std::shared_ptr<NCClient>> client_workers;
    PSP_OK(parse_config(
        cfg_file, client_net_contexts, cpus, &net_mempool, remote_ports, remote_hosts,
        client_workers, 1
    ));

    auto &ctx = client_net_contexts.front(); // we are expecting a single context
    w = std::make_shared<NCClient>();
    w->eal_thread = true;
    w->cpu_id = cpus[0];
    w->udp_ctx = ctx;

    log_info("Created NC client %u", w->worker_id);

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
    if (w->launch() != 0) {
        w->stop();
    }

    if (std::signal(SIGINT, stop_nc) == SIG_ERR)
        log_error("can't catch SIGINT");
    if (std::signal(SIGTERM, stop_nc) == SIG_ERR)
        log_error("can't catch SIGTERM");

    /* Join threads */
    w->join();

    print_dpdk_device_stats(0);
    return 0;
}
