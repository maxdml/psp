#ifndef PSP_APP_H_
#define PSP_APP_H_

#include <psp/libos/persephone.hh>
#include <psp/libos/su/NetSu.hh>
#include <psp/libos/su/RocksdbSu.hh>

#include <arpa/inet.h>

#include <random>
#include <fstream>

namespace po = boost::program_options;

class PspApp {
    private: rocksdb_t *rocks_db;

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

        // If this is a RocksDB app
        Worker *rdb_workers[MAX_WORKERS];
        uint32_t ntds = psp->get_workers(WorkerType::RDB, rdb_workers);
        if (ntds > 0) {
            // Init RocksDB options
            rocksdb_options_t *options = rocksdb_options_create();
            rocksdb_options_set_allow_mmap_reads(options, 1);
            rocksdb_options_set_allow_mmap_writes(options, 1);
            rocksdb_slicetransform_t * prefix_extractor = rocksdb_slicetransform_create_fixed_prefix(8);
            rocksdb_options_set_prefix_extractor(options, prefix_extractor);
            rocksdb_options_set_plain_table_factory(options, 0, 10, 0.75, 3);
            rocksdb_options_increase_parallelism(options, 0);
            rocksdb_options_optimize_level_style_compaction(options, 0);
            rocksdb_options_set_create_if_missing(options, 1);

            // Open DB
            char *err = NULL;
            char DBPath[] = "/tmp/my_db";
            rocks_db = rocksdb_open(options, DBPath, &err);
            if (err) {
                PSP_ERROR("Could not open RocksDB database: " << err);
                exit(1);
            }
            for (unsigned int i = 0; i < ntds; ++i) {
                dynamic_cast<RdbWorker *>(rdb_workers[i])->db = rocks_db;
            }

            //options->rep.env->thread_pools_.clear();
            //env->threads_to_join_.clear();
            PSP_INFO("Initialized RocksDB");
            return;
        }
    }
};

#endif // PSP_APP_H_
