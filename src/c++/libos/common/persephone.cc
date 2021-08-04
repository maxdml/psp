#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <yaml-cpp/yaml.h>

#include <psp/logging.hh>
#include <psp/libos/persephone.hh>
#include <psp/libos/su/NetSu.hh>
#include <psp/libos/su/MbSu.hh>
#include <psp/libos/su/DispatchSu.hh>
#include <psp/annot.h>

std::string log_dir = "./";
Worker *workers[MAX_WORKERS];
uint32_t total_workers = 0;

/********** CONTROL PLANE ******************/
Psp::Psp(std::string &app_cfg, std::string label) {
    /* Let network libOS init its specific EAL */
    dpdk_net_init(app_cfg.c_str());

    /* Parse the configuration */
    try {
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

        uint16_t n_tqs = 0;
        uint16_t n_rqs = 0;
        uint16_t port_id;
        // Setup net workers
        std::string mac = config["network"]["mac"].as<std::string>();
        port_id = config["network"]["device_id"].as<uint16_t>();
        size_t n_net_workers = 0;
        if (config["net_workers"].IsDefined()) {
            YAML::Node net_workers = config["net_workers"];
            if (net_workers.size() > cpus.size()) {
                PSP_ERROR("Not enough service units to accomodate net workers");
            }
            n_net_workers = net_workers.size();
            for (size_t i = 0; i < n_net_workers; ++i) {
                bool is_echo = false;
                if (net_workers[i]["is_echo"].IsDefined() and
                    net_workers[i]["is_echo"].as<uint32_t>()) {
                    is_echo = true;
                }
                NetWorker *net_worker = new NetWorker(is_echo);
                net_worker->eal_thread = true;
                net_worker->cpu_id = cpus[i];

                uint16_t port = net_workers[i]["port"].as<uint16_t>();
                struct in_addr ip;
                inet_aton(net_workers[i]["ip"].as<std::string>().c_str(), &ip);
                net_worker->udp_ctx = new UdpContext(
                    i, ip, port, port_id, &net_mempool, mac
                );
                n_tqs++;
                n_rqs++;

                workers[i] = net_worker;
                PSP_INFO(
                    "Created server net worker " << net_worker->worker_id
                );

                Dispatcher &dpt = net_worker->dpt;
                // Set dispatch mode
                std::string dispatch_mode = net_workers[i]["dp"].as<std::string>();
                dpt.set_dp(dispatch_mode);
                PSP_INFO("DP: " << dispatch_mode);
            }
        } else {
            PSP_ERROR("Operator must register at least one net worker.");
            exit(ENODEV);
        }

        // Setup application worker
        Worker *net_workers[MAX_WORKERS];
        get_workers(WorkerType::NET, net_workers);
        auto netw = dynamic_cast<NetWorker *>(net_workers[0]);
        Dispatcher &dpt = netw->dpt;
        if (config["workers"].IsDefined()) {
            YAML::Node workers = config["workers"];
            uint32_t n_workers = workers["number"].as<uint32_t>();
            std::string type = workers["type"].as<std::string>();
            for (size_t i = n_net_workers; i < n_workers + n_net_workers; ++i) {
                // Set UDP context
                UdpContext *udp_ctx = new UdpContext(
                    i, netw->udp_ctx->ip, netw->udp_ctx->port,
                    port_id, &net_mempool, mac
                );
                udp_ctx->remote_port = netw->udp_ctx->port;
                udp_ctx->remote_ip = netw->udp_ctx->ip;
                n_tqs++;
                // Create worker instance
                if (type == "MB") {
					CreateWorker<MbWorker>(i, &dpt, netw, udp_ctx);
                }
                // Update dispatcher
                dpt.n_workers++;
            }
            dpt.free_peers = __builtin_powi(2, dpt.n_workers) - 1;
            PSP_INFO("Registered " << n_workers << " workers");
        } else {
            PSP_WARN("No worker registered?");
        }

        // Register request types
        std::map<uint64_t, RequestType> rtypes;
        if (config["requests"].IsDefined()) {
            YAML::Node req_types = config["requests"];
            size_t n_types = req_types.size();
            if (n_types > static_cast<int>(ReqType::LAST)) {
                PSP_ERROR("Too many declard types (max " << static_cast<int>(ReqType::LAST) << ")");
                exit(EINVAL);
            }
            for (size_t i = 0; i < n_types; ++i) {
                std::string req_type = req_types[i]["type"].as<std::string>();
                double mean_ns = req_types[i]["mean_ns"].as<double>(); // now in usecs
                double deadline = req_types[i]["deadline"].as<double>();
                double ratio = req_types[i]["ratio"].as<double>();
                RequestType rtype(str_to_type(req_type), mean_ns, deadline, ratio);
                rtypes[mean_ns] = rtype;
                PSP_INFO(
                    "Registered request type " << req_type
                    << ". mean_ns: " << mean_ns << ". ratio: " << ratio
                );
            }
            int i = 0;
            if (dpt.dp != Dispatcher::dispatch_mode::CFCFS) {
                for (auto &rtype: rtypes) {
                    dpt.rtypes[i] = rtype.second;
                    dpt.type_to_nsorder[static_cast<int>(rtype.second.type)] = i;
                    i++;
                }
            }
            dpt.n_rtypes = i;
            dpt.type_to_nsorder[static_cast<int>(ReqType::UNKNOWN)] = i;
        } else {
                if (dpt.dp != Dispatcher::dispatch_mode::CFCFS or dpt.dp != Dispatcher::dispatch_mode::DFCFS) {
                    PSP_ERROR("Type aware policy " << dpt.dp << " requires request metadata");
                    exit(ENODEV);
                }
        }

        /* Setup NIC ports */
        PSP_INFO("Setting up NIC ports");
        if (init_dpdk_port(port_id, net_mempool, n_tqs, n_rqs) != 0) {
            exit(1);
        }

        /* Setup fdir on net worker rxqs */
        //netw->udp_ctx->set_fdir();
    } catch (YAML::ParserException& e) {
        std::cout << "Failed to parse config: " << e.what() << std::endl;
        exit(1);
    }
}

template <typename A, typename B, typename C>
int Psp::CreateWorker(int idx, B *dpt, C *netw, UdpContext* udp_ctx) {
	A *worker = new A();
    worker->udp_ctx = udp_ctx;
    worker->register_dpt(*dpt);
    worker->eal_thread = true;
    worker->cpu_id = cpus[idx];
    workers[idx] = worker;
    if (dpt->dp == Dispatcher::dispatch_mode::DFCFS) {
        worker->notify = false;
    }
	return 0;
}
