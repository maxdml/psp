#include <psp/libos/io/udp_dpdk/dpdk.hh>
#include <psp/libos/persephone.hh>

#include <vector>
#include <string>
#include <iostream>

// device configuration
int dpdk_net_init(const char *app_cfg_filename) {
    std::string config_path(app_cfg_filename);
    std::vector<std::string> init_args;
    YAML::Node config = YAML::LoadFile(config_path);
    YAML::Node node = config["network"]["eal_init"];
    if (YAML::NodeType::Sequence == node.Type()) {
        init_args = node.as<std::vector<std::string>>();
    }
    std::cerr << "eal_init: [";
    std::vector<char *> init_cargs;
    for (auto i = init_args.cbegin(); i != init_args.cend(); ++i) {
        if (i != init_args.cbegin()) {
            std::cerr << ", ";
        }
        std::cerr << "\"" << *i << "\"";
        init_cargs.push_back(const_cast<char *>(i->c_str()));
    }
    std::cerr << "]" << std::endl;

    int unused = -1;
    PSP_OK(rte_eal_init(unused, init_cargs.size(), init_cargs.data()));
    const uint16_t nb_ports = rte_eth_dev_count_avail();
    PSP_TRUE(ENOENT, nb_ports > 0);
    PSP_INFO(
        "DPDK reports that " << nb_ports << " ports (interfaces) are available for the application."
    );

    return 0;
}

int init_dpdk_port(uint16_t port_id, rte_mempool *mbuf_pool,
                   uint16_t n_tx_rings, uint16_t n_rx_rings) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    const uint16_t rx_rings = n_rx_rings;
    const uint16_t tx_rings = n_tx_rings;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;

    struct ::rte_eth_dev_info dev_info = {};
    PSP_OK(rte_eth_dev_info_get(port_id, dev_info));

    struct ::rte_eth_conf port_conf = {};
    //port_conf.rxmode.mq_mode = ETH_MQ_RX_NONE;
    //port_conf.rxmode.offloads = DEV_TX_OFFLOAD_UDP_CKSUM;
    port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    port_conf.rxmode.max_rx_pkt_len = 1024;
    std::cout << "mx rx pkt len= " << port_conf.rxmode.max_rx_pkt_len << std::endl;

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
        //port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    } else {
        PSP_WARN("NIC does not supports DEV_TX_OFFLOAD_MBUF_FAST_FREE");
    }

    //port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;
    //port_conf.txmode.offloads |=
    //   (DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM);

    struct ::rte_eth_rxconf rx_conf = dev_info.default_rxconf;
    /*
    rx_conf.offloads = 0x0;
    rx_conf.rx_drop_en = 0;
    rx_conf.rx_free_thresh = 0;
    */

    struct ::rte_eth_txconf tx_conf = dev_info.default_txconf;
    tx_conf.tx_rs_thresh = DEFAULT_TX_RS_THRESH;
    tx_conf.offloads = DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    /*
    tx_conf.tx_rs_thresh = 0;
    tx_conf.offloads = 0x0;
    */

    // configure the ethernet device.
    PSP_OK(-rte_eth_dev_configure(port_id, rx_rings, tx_rings, port_conf));

    // Check that numbers of Rx and Tx descriptors satisfy descriptors limits from the ethernet
    // device information, otherwise adjust them to boundaries.
    int retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
    if (retval != 0) {
        return retval;
    }
    if (nb_rxd != RX_RING_SIZE) {
        printf("Adjusted number of RX descriptors per queue from %d to %u", RX_RING_SIZE, nb_rxd);
    }
    if (nb_txd != TX_RING_SIZE) {
        printf("Adjusted number of TX descriptors per queue from %d to %u", TX_RING_SIZE, nb_txd);
    }

    int socket_id = 0; //FIXME make the NUMA node configurable
    int ret = rte_eth_dev_socket_id(socket_id, port_id);
    if (0 != ret) {
        fprintf(stderr, "WARNING: Failed to get the NUMA socket ID for port %d.\n", port_id);
        socket_id = 0;
    }

    // allocate and set up RX queues
    for (uint16_t i = 0; i < rx_rings; ++i) {
        PSP_OK(rte_eth_rx_queue_setup(port_id, i, nb_rxd, socket_id, rx_conf, mbuf_pool));
    }

    // allocate and set up TX queues
    for (uint16_t i = 0; i < tx_rings; ++i) {
        PSP_OK(rte_eth_tx_queue_setup(port_id, i, nb_txd, socket_id, tx_conf));
    }

    // start the ethernet port.
    PSP_OK(-rte_eth_dev_start(port_id));
    PSP_INFO(
        "Started Network port " << port_id << " rx rings: " << rx_rings
        << ", tx rings: " << tx_rings
    );

    // disable the rx/tx flow control
    struct ::rte_eth_fc_conf fc_conf = {};
    PSP_OK(rte_eth_dev_flow_ctrl_get(port_id, fc_conf));
    fc_conf.mode = RTE_FC_NONE;
    //fc_conf.mode = RTE_FC_FULL;
    PSP_OK(rte_eth_dev_flow_ctrl_set(port_id, fc_conf));
    PSP_OK(wait_for_link_status_up(port_id));

    PSP_OK(rte_eth_dev_info_get(port_id, dev_info));
    return 0;
}


int rte_eth_dev_flow_ctrl_get(uint16_t port_id, struct rte_eth_fc_conf &fc_conf) {
    fc_conf = {};
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    int ret = ::rte_eth_dev_flow_ctrl_get(port_id, &fc_conf);
    if (0 == ret) {
        return 0;
    }

    if (0 > ret) {
        return 0 - ret;
    }

    PSP_UNREACHABLE();
}

int rte_eth_dev_flow_ctrl_set(uint16_t port_id, const struct rte_eth_fc_conf &fc_conf) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    int ret = ::rte_eth_dev_flow_ctrl_set(port_id, const_cast<struct rte_eth_fc_conf *>(&fc_conf));
    if (0 == ret) {
        return 0;
    }

    if (0 > ret) {
        return 0 - ret;
    }

    PSP_UNREACHABLE();
}

rte_flow * generate_ipv4_flow(uint16_t port_id, uint16_t rx_q,
                              uint32_t src_ip, uint32_t src_mask,
                              uint32_t dest_ip, uint32_t dest_mask,
                              struct rte_flow_error *error) {
    struct rte_flow_attr attr;
    /* Match only ingress packets */
    memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

    /* Storage for all actions and patterns */
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action action[MAX_ACTION_NUM];
    memset(pattern, 0, sizeof(pattern));
    memset(action, 0, sizeof(action));
    struct rte_flow *flow = NULL;

    /* Count action for stats */
    /* XXX Not supported on X710
    struct rte_flow_action_count count;
    count.shared = 0;
    count.id = rx_q;
    struct rte_flow_action count_action = { RTE_FLOW_ACTION_TYPE_COUNT, &count};
    */

    /* Queue action for packet steering */
    struct rte_flow_action_queue queue = { .index = rx_q };
    struct rte_flow_action queue_action = { RTE_FLOW_ACTION_TYPE_QUEUE, &queue};

    /* IP and UDP spec and masks */
    struct rte_flow_item_ipv4 ip_spec;
    struct rte_flow_item_ipv4 ip_mask;
    memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
    memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));

    /* Create the action sequence: count matching packets and move it to queue */
    //action[0] = count_action;
    action[0] = queue_action;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

    // ETH
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

    // IPV4
    ip_spec.hdr.dst_addr = dest_ip;
    ip_mask.hdr.dst_addr = dest_mask;
    ip_spec.hdr.src_addr = src_ip;
    ip_mask.hdr.src_addr = src_mask;
    pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[1].spec = &ip_spec;
    pattern[1].mask = &ip_mask;

    // END
    pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

    int res = rte_flow_validate(port_id, &attr, pattern, action, error);
    if (!res)
        flow = rte_flow_create(port_id, &attr, pattern, action, error);

    return flow;
}

int rte_eth_dev_socket_id(int &sockid_out, uint16_t port_id) {
    sockid_out = 0;

    int ret = ::rte_eth_dev_socket_id(port_id);
    if (-1 == ret) {
        // `port_id` is out of range.
        return ERANGE;
    }

    if (0 <= ret) {
        sockid_out = ret;
        return 0;
    }

    PSP_UNREACHABLE();
}

int rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
                           uint16_t nb_rx_desc, int socket_id,
                           const struct rte_eth_rxconf &rx_conf, struct rte_mempool *mb_pool) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    PSP_DEBUG(
        "configuring rx queue " << rx_queue_id
        << " on port " << port_id << " with " << nb_rx_desc << " rx rings"
    );
    int ret = ::rte_eth_rx_queue_setup(port_id, rx_queue_id, nb_rx_desc, socket_id, &rx_conf, mb_pool);
    if (0 == ret) {
        return 0;
    }

    if (0 > ret) {
        return 0 - ret;
    }

    PSP_UNREACHABLE();
}

int rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id, uint16_t nb_tx_desc,
                           unsigned int socket_id, const struct rte_eth_txconf &tx_conf) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    PSP_DEBUG(
        "configuring tx queue " << tx_queue_id
        << " on port " << port_id << " with " << nb_tx_desc << " tx rings"
    );
    int ret = ::rte_eth_tx_queue_setup(port_id, tx_queue_id, nb_tx_desc, socket_id, &tx_conf);
    if (0 == ret) {
        return 0;
    }

    if (0 > ret) {
        return 0 - ret;
    }

    PSP_UNREACHABLE();
}


int rte_eal_init(int &count_out, int argc, char *argv[]) {
    count_out = -1;
    PSP_NOTNULL(EINVAL, argv);
    PSP_TRUE(ERANGE, argc >= 0);
    for (int i = 0; i < argc; ++i) {
        PSP_NOTNULL(EINVAL, argv[i]);
    }

    int ret = ::rte_eal_init(argc, argv);
    if (-1 == ret) {
        return rte_errno;
    }

    if (-1 > ret) {
        PSP_UNREACHABLE();
    }

    count_out = ret;
    return 0;
}

int wait_for_link_status_up(uint16_t port_id) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    const size_t sleep_duration_ms = 100;
    const size_t retry_count = 90;

    struct rte_eth_link link = {};
    for (size_t i = 0; i < retry_count; ++i) {
        PSP_OK(rte_eth_link_get_nowait(port_id, link));
        if (ETH_LINK_UP == link.link_status) {
            PSP_OK(print_link_status(stderr, port_id, &link));
            return 0;
        }

        rte_delay_ms(sleep_duration_ms);
    }

    PSP_OK(print_link_status(stderr, port_id, &link));
    return ECONNREFUSED;
}

int print_link_status(FILE *f, uint16_t port_id, const struct rte_eth_link *link) {
    PSP_NOTNULL(EINVAL, f);
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    struct rte_eth_link link2 = {};
    if (NULL == link) {
        PSP_OK(rte_eth_link_get_nowait(port_id, link2));
        link = &link2;
    }
    if (ETH_LINK_UP == link->link_status) {
        const char * const duplex = ETH_LINK_FULL_DUPLEX == link->link_duplex ?  "full" : "half";
        fprintf(f, "Port %d Link Up - speed %u " "Mbps - %s-duplex\n", port_id, link->link_speed, duplex);
    } else {
        printf("Port %d Link Down\n", port_id);
    }

    return 0;
}


int rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info &dev_info) {
    dev_info = {};
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));
    ::rte_eth_dev_info_get(port_id, &dev_info);
    return 0;
}

int rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue, const struct rte_eth_conf &eth_conf) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));
    int ret = ::rte_eth_dev_configure(port_id, nb_rx_queue, nb_tx_queue, &eth_conf);
    if (0 >= ret) {
        return ret;
    }
    PSP_UNREACHABLE();
}

int rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link &link) {
    link = {};
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    ::rte_eth_link_get_nowait(port_id, &link);
    return 0;
}

int rte_eth_dev_mac_addr_add(uint16_t port_id, struct rte_ether_addr &mac_addr) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    ::rte_eth_dev_mac_addr_add(port_id, &mac_addr, 0);
    return 0;
}

int rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr &mac_addr) {
    PSP_TRUE(ERANGE, ::rte_eth_dev_is_valid_port(port_id));

    // todo: how to detect invalid port ids?
    ::rte_eth_macaddr_get(port_id, &mac_addr);
    return 0;
}

int print_ether_addr(FILE *f, struct rte_ether_addr &eth_addr) {
    PSP_NOTNULL(EINVAL, f);

    char buf[RTE_ETHER_ADDR_FMT_SIZE];
    rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_addr);
    fputs(buf, f);
    return 0;
}

int print_dpdk_device_stats(const uint16_t port_id) {
    int ret;

    /* Print basic stats */
    struct rte_eth_stats stats;
    ret = ::rte_eth_stats_get(port_id, &stats);
    if (ret) {
        printf("dpdk: error getting eth stats");
    }

    printf("eth stats for port %d", port_id);

    printf("[port %u], RX-packets: %" PRIu64 " RX-dropped: %" PRIu64 " RX-bytes: %" PRIu64 "\n",
            port_id, stats.ipackets, stats.imissed, stats.ibytes);

    printf("[port %u] TX-packets: %" PRIu64 " TX-bytes: %" PRIu64 "\n",
            port_id, stats.opackets, stats.obytes);

    printf("RX-error: %" PRIu64 " TX-error: %" PRIu64 " RX-mbuf-fail: %" PRIu64 "\n",
            stats.ierrors, stats.oerrors, stats.rx_nombuf);

    /* Print extended stats */
    int len;
    struct rte_eth_xstat *xstats;
    struct rte_eth_xstat_name *xstats_names;
    static const char *stats_border = "_______";

    printf("EXTENDED PORT STATISTICS:\n================\n");
    // Trick to retrieve number of stats
    len = ::rte_eth_xstats_get(port_id, NULL, 0);
    if (len < 0) {
        printf("rte_eth_xstats_get(%u) failed: %d", port_id, len);
        return 1;
    }

    xstats = static_cast<rte_eth_xstat *>(calloc(len, sizeof(*xstats)));
    if (xstats == NULL) {
        printf("Failed to calloc memory for xstats");
        return 1;
    }

    ret = ::rte_eth_xstats_get(port_id, xstats, len);
    if (ret < 0 || ret > len) {
        free(xstats);
        printf("rte_eth_xstats_get(%u) len%i failed: %d", port_id, len, ret);
        return 1;
    }

    xstats_names = static_cast<rte_eth_xstat_name *>(calloc(len, sizeof(*xstats_names)));
    if (xstats_names == NULL) {
        free(xstats);
        printf("Failed to calloc memory for xstats_names");
        return 1;
    }

    ret = rte_eth_xstats_get_names(port_id, xstats_names, len);
    if (ret < 0 || ret > len) {
        free(xstats);
        free(xstats_names);
        printf("rte_eth_xstats_get_names(%u) len%i failed: %d", port_id, len, ret);
        return 1;
    }
    for (int i = 0; i < len; i++) {
        if (xstats[i].value == 0) {
            continue;
        }
        printf("Port %u: %s %s:\t\t%" PRIu64 "\n",
            port_id, stats_border,
            xstats_names[i].name,
            xstats[i].value
        );
    }

    return 0;
}
