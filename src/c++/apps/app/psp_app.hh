#ifndef PSP_APP_H_
#define PSP_APP_H_

#include <psp/libos/persephone.hh>
#include <psp/libos/su/NetSu.hh>

#include <arpa/inet.h>

#include <fstream>

namespace po = boost::program_options;

class PspApp {
    /* libOS instance */
    public: std::unique_ptr<Psp> psp;

    /* Logging */
    std::string label;

    /* Constructor: sets up service units, hardware, and application */
    public: PspApp(int argc, char *argv[]) {
        std::string cfg;
        std::string cmd_list;
        bool wrkr_offload_tx;

        po::options_description desc{"PSP app options"};
        desc.add_options()
            ("label,l", po::value<std::string>(&label)->default_value("http_server"), "Experiment label")
            ("cfg,c", po::value<std::string>(&cfg)->required(), "Path to configuration file")
            ("cmd-list,u", po::value<std::string>(&cmd_list), "Server commands for setup");
        po::variables_map vm;
        try {
            po::parsed_options parsed =
                po::command_line_parser(argc, argv).options(desc).run();
            po::store(parsed, vm);
            if (vm.count("help")) {
                std::cout << desc << std::endl;
                exit(0);
            }
            notify(vm);
        } catch (const po::error &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << desc << std::endl;
            exit(0);
        }

        /* init libOS and workers threads */
        psp = std::make_unique<Psp>(cfg, label);

        /* Pin main thread */
        pin_thread(pthread_self(), 0);
    }
};

#endif // PSP_APP_H_
