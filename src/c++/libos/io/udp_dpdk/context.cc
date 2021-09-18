#include <psp/libos/io/udp_dpdk/context.hh>

int UdpContext::poll() {
    // Check RX
    PSP_OK(recv_packets());
    // Check TX
    PSP_OK(send_packets());
    return 0;
}

int UdpContext::recv_packets() {
    // Only dequeue if we have buffer space
    if (pop_head - pop_tail < INBOUND_Q_LEN - MAX_RX_BURST) {
        rte_mbuf *pkts[MAX_RX_BURST];
        size_t count = ::rte_eth_rx_burst(port_id, id, pkts, MAX_RX_BURST);
        if (count == 0) {
            return EAGAIN;
        }
        for (size_t i = 0; i < count; ++i) {
            if (i + RX_PREFETCH_STRIDE < count) {
                rte_prefetch0(rte_pktmbuf_mtod(pkts[i + RX_PREFETCH_STRIDE], char *));
            }
            PSP_OK(parse_packet(pkts[i]));
        }
#ifdef NET_DEBUG
    } else {
        PSP_WARN("Inbound UDP queue full.");
#endif
    }
    return 0;
}

int UdpContext::send_packets() {
    uint16_t nb_pkts = 0;
    rte_mbuf *pkts[MAX_TX_BURST];
    while (push_head > push_tail && nb_pkts < MAX_TX_BURST) {
        PSP_OK(prepare_outbound_packet(
            outbound_queue[push_tail++ & (OUTBOUND_Q_LEN - 1)],
            &pkts[nb_pkts++]
        ));
    }
    if (likely(nb_pkts > 0)) {
        size_t count = ::rte_eth_tx_burst(port_id, id, pkts, nb_pkts);
        if (unlikely(count < nb_pkts)) {
            PSP_ERROR(" Sent " << count << " / " << nb_pkts);
            return EAGAIN;
        }
    }
    return 0;
}

template <typename T>
T* pktmbuf_struct_read(const rte_mbuf *pkt, size_t offset, T& buf) {
    return (T*)rte_pktmbuf_read(pkt, offset, sizeof(buf), &buf);
}

int UdpContext::prepare_outbound_packet(unsigned long mbuf, rte_mbuf **pkt_out) {
    rte_mbuf *pkt = static_cast<rte_mbuf *>((void *)mbuf);
    PSP_NOTNULL(ENOMEM, pkt);

    size_t hdr_offset = 0;
    char *pkt_start = rte_pktmbuf_mtod(pkt, char*);

    auto *eth_hdr = rte_pktmbuf_mtod_offset(pkt, rte_ether_hdr *, hdr_offset);
    hdr_offset += sizeof(*eth_hdr);
    auto *ip_hdr = rte_pktmbuf_mtod_offset(pkt, rte_ipv4_hdr *, hdr_offset);
    hdr_offset += sizeof(*ip_hdr);
    auto *udp_hdr = rte_pktmbuf_mtod_offset(pkt, rte_udp_hdr *, hdr_offset);
    hdr_offset += sizeof(*udp_hdr);
    char *data_start = rte_pktmbuf_mtod_offset(pkt, char *, hdr_offset);
    char *data_offset = data_start + pkt->l4_len; // XXX dirty trick
    data_offset += *(uint32_t *)data_offset; // payload size
    size_t data_len = (data_offset - data_start);
    size_t total_len = (data_offset - pkt_start);

    // Use our local port as source
    udp_hdr->src_port = htons(port);
    uint16_t udp_len = static_cast<uint16_t>(data_len + sizeof(*udp_hdr));
    udp_hdr->dgram_len = htons(udp_len);
    pkt->l4_len = sizeof(*udp_hdr);

    uint16_t ip_len = static_cast<uint16_t>(data_len + sizeof(*udp_hdr) + sizeof(*ip_hdr));
    ip_hdr->total_length = htons(ip_len);
    ip_hdr->version_ihl = IP_VHL_DEF;
    ip_hdr->time_to_live = IP_DEFTTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;

    ip_hdr->src_addr = ip.s_addr;
    pkt->ol_flags = 0x0;
#ifdef OFFLOAD_IP_CKSUM
    pkt->ol_flags |= PKT_TX_IP_CKSUM;
#endif
    pkt->l3_len = sizeof(*ip_hdr);

    //FIXME: if we are a server, remote_mac is zero and we can swap IP addr and UDP ports.
    if (rte_is_zero_ether_addr(&remote_mac)) {
        eth_hdr->d_addr = eth_hdr->s_addr;
        udp_hdr->dst_port = udp_hdr->src_port;
        ip_hdr->dst_addr = ip_hdr->src_addr;
    } else {
        assert(not rte_is_zero_ether_addr(&remote_mac));
        eth_hdr->d_addr = remote_mac;
        udp_hdr->dst_port = remote_port;
        ip_hdr->dst_addr = remote_ip.s_addr;
    }
    eth_hdr->s_addr = my_mac;
    eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV4);
    pkt->l2_len = sizeof(*eth_hdr);

    pkt->data_len = total_len;
    pkt->pkt_len = total_len;
    pkt->nb_segs = 1;

#ifdef NET_DEBUG
    printf("===== Send: ETHERNET header =====\n");
    char src_buf[RTE_ETHER_ADDR_FMT_SIZE];
    char dst_buf[RTE_ETHER_ADDR_FMT_SIZE];
    rte_ether_format_addr(src_buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
    rte_ether_format_addr(dst_buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
    printf("[context %d]send: src eth addr: %s\n", id, src_buf);
    printf("[context %d]send: dst eth addr: %s\n", id, dst_buf);

    printf("===== Send: IP header =====\n");
    printf("send: ip src addr: %x\n", ntohl(ip_hdr->src_addr));
    printf("send: ip dst addr: %x\n", ntohl(ip_hdr->dst_addr));
    printf("send: ip len: %d\n", ntohs(ip_hdr->total_length));

    printf("===== Send: UDP header =====\n");
    printf("send: udp len: %d\n", ntohs(udp_hdr->dgram_len));
    printf("send: udp src port: %d\n", ntohs(udp_hdr->src_port));
    printf("send: udp dst port: %d\n", ntohs(udp_hdr->dst_port));

    printf("send: pkt len: %u\n", pkt->pkt_len);
    printf("send: data len: %u\n", pkt->data_len);
    rte_pktmbuf_dump(stderr, pkt, pkt->pkt_len);
    printf("====================\n");
#endif

    *pkt_out = pkt;
    return 0;
}

int UdpContext::parse_packet(struct rte_mbuf *pkt) {
#ifdef NET_DEBUG
	// Check Ethernet header
    size_t offset = 0;
    ::rte_ether_hdr eth_buf;
    auto * const eth_hdr = pktmbuf_struct_read(pkt, 0, eth_buf);
    offset += sizeof(*eth_hdr);
    auto eth_type = ntohs(eth_hdr->ether_type);

    printf("====================\n");
    std::cout << "using mbuf: " << pkt << std::endl;
    printf("recv: pkt len: %d\n", pkt->pkt_len);
    printf("recv: eth src addr: ");
    PSP_OK(print_ether_addr(stdout, eth_hdr->s_addr));
    printf("\n");
    printf("recv: eth dst addr: ");
    PSP_OK(print_ether_addr(stdout, eth_hdr->d_addr));
    printf("\n");
    printf("recv: eth type: %x\n", eth_type);

    // Check if destination MAC address is ours or a broadcast address
    if (!rte_is_same_ether_addr(&my_mac, &eth_hdr->d_addr)
        and !rte_is_same_ether_addr(&ether_broadcast, &eth_hdr->d_addr)) {
        char my_buf[RTE_ETHER_ADDR_FMT_SIZE];
        char dst_buf[RTE_ETHER_ADDR_FMT_SIZE];
        rte_ether_format_addr(my_buf, RTE_ETHER_ADDR_FMT_SIZE, &my_mac);
        rte_ether_format_addr(dst_buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
        printf(
            "[context %d]recv: dropped (wrong dst eth addr %s, we are at %s)!\n",
            id, dst_buf, my_buf
        );
        rte_pktmbuf_free(pkt);
        return 0;
    }

    // check ip header
    ::rte_ipv4_hdr ip_hdr_buf;
    auto * const ip_hdr = pktmbuf_struct_read(pkt, offset, ip_hdr_buf);
    offset += sizeof(*ip_hdr);
    // In network byte order.
    in_addr_t ipv4_src_addr = ip_hdr->src_addr;
    in_addr_t ipv4_dst_addr = ip_hdr->dst_addr;

    if (IPPROTO_UDP != ip_hdr->next_proto_id) {
        printf("recv: dropped (not UDP, instead %d)!\n", (int)ip_hdr->next_proto_id);
        rte_pktmbuf_free(pkt);
        return 0;
    }

    printf("recv: ip src addr: %x\n", ntohl(ipv4_src_addr));
    printf("recv: ip dst addr: %x\n", ntohl(ipv4_dst_addr));
    printf("recv: ip tot len: %i\n", ntohs(ip_hdr->total_length));

    // check udp header
    ::rte_udp_hdr udp_hdr_buf;
    auto *const udp_hdr = pktmbuf_struct_read(pkt, offset, udp_hdr_buf);
    offset += sizeof(*udp_hdr);
    // In network byte order.
    in_port_t udp_src_port = udp_hdr->src_port;
    in_port_t udp_dst_port = udp_hdr->dst_port;
    if (ntohs(udp_dst_port) != port) {
        printf(
            "dropping packet (dst port: %u != %u)",
            ntohs(udp_dst_port), port
        );
        rte_pktmbuf_free(pkt);
        return 0;
    }
    printf("recv: udp src port: %d\n", ntohs(udp_src_port));
    printf("recv: udp dst port: %d\n", ntohs(udp_dst_port));
    printf("recv: udp cksum: %u\n", udp_hdr->dgram_cksum);
    rte_pktmbuf_dump(stderr, pkt, pkt->pkt_len);
#endif

    inbound_queue[pop_head++ & (INBOUND_Q_LEN - 1)] = (unsigned long) (void *) pkt;
    return 0;
}

int UdpContext::init_mempool(struct rte_mempool **mempool_out,
                             const uint16_t numa_socket_id,
                             const char *name) {

    // create pool of memory for ring buffers.
    *mempool_out = rte_pktmbuf_pool_create_by_ops(
        name,
        NUM_MBUFS * rte_eth_dev_count_avail(),
        MBUF_CACHE_SIZE,
        0,
        MBUF_DATA_SIZE,
        numa_socket_id,
        "ring_mp_sc"
    );
    PSP_NOTNULL(EPERM, mempool_out);
    PSP_TRUE(EINVAL, (*mempool_out)->cache_size == MBUF_CACHE_SIZE);

    log_debug("Created mempool %s of size (%d * %d) * (%d)  = %d Bytes",
            name,
            NUM_MBUFS, rte_eth_dev_count_avail(), MBUF_DATA_SIZE,
            NUM_MBUFS * rte_eth_dev_count_avail() * MBUF_DATA_SIZE);

    // Create a cache for the context
    if (rte_mempool_default_cache(*mempool_out, numa_socket_id)) {
        PSP_DEBUG("Context has a default cache");
    } else {
        PSP_WARN("Cache disabled for mempool on numa node " << numa_socket_id);
    }

    return 0;
}

int UdpContext::set_fdir() {
    struct in_addr saddr;
    const char *subnet = "0.0.0.0";
    inet_aton(subnet, &saddr);
    uint32_t src_mask, dst_mask;
    src_mask = 0x0;
    dst_mask = 0xffffffff;
    //uint8_t src_mask_bits = ceil(log(src_mask) / log(2));
    uint8_t src_mask_bits = 0;
    uint8_t dst_mask_bits = ceil(log(dst_mask) / log(2));

    struct rte_flow_error error;

    /* Generate an ingress rule for the context's rx queue */
    PSP_DEBUG("Attempting to register new ingress rule on rxq " << id);
    PSP_DEBUG(
        "SRC " << inet_ntoa(saddr) << "/" << unsigned(src_mask_bits) << ": 0" <<
        ". DST " << inet_ntoa(ip) << "/" << unsigned(dst_mask_bits) << ": " << port <<
        "--> rxq " << id
    );

    struct rte_eth_ntuple_filter ntuple_filter;
    ntuple_filter.src_ip = htonl(saddr.s_addr);
    ntuple_filter.dst_ip = ip.s_addr; // Stored in network order
    ntuple_filter.src_ip_mask = src_mask;
    ntuple_filter.dst_ip_mask = dst_mask;
    ntuple_filter.proto = IPPROTO_UDP;
    ntuple_filter.proto_mask = 0xff;
    ntuple_filter.src_port = 0x0;
    ntuple_filter.dst_port = RTE_BE16(port); // Stored in host order
    ntuple_filter.src_port_mask = 0x0;
    ntuple_filter.dst_port_mask = 0xffff;

    rte_ether_addr src_mac = {
        .addr_bytes = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
    };

    struct rte_flow *flow = generate_ipv4_flow(
        port_id, id, ntuple_filter, &error
    );

    if (!flow) {
        PSP_ERROR("Flow can't be created. Error " << error.type
                  << ", message: " << error.message);//? error.message : "(no stated reason)");
        return ENOTRECOVERABLE;
    }
    flows.push_back(flow);

    PSP_INFO("Registered new ingress rule:");
    PSP_INFO(
        "SRC " << inet_ntoa(saddr) << "/" << unsigned(src_mask_bits) <<
        ". DST " << inet_ntoa(ip) << "/" << unsigned(dst_mask_bits) << " -> rxq " <<  id
    );

    return 0;
}

int UdpContext::get_mbuf(void **mbuf_out, char **payload_addr) {
    // Just a shorcut
    if (*mbuf_out != nullptr) {
        struct rte_mbuf *mbuf = static_cast<struct rte_mbuf *>(*mbuf_out);
        *payload_addr = static_cast<char *>(mbuf->buf_addr) + mbuf->data_off;
        return 0;
    }

    rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (unlikely(mbuf == nullptr)) {
        if (rte_mempool_full(mbuf_pool)) {
            PSP_ERROR("mpool is full");
        }
        std::cerr << "Error allocating pktmbuf: "
                  << "mpool usage: " << rte_mempool_in_use_count(mbuf_pool)
                  << "available (" << rte_mempool_avail_count(mbuf_pool)
                  << ")" << std::endl;
        return ENOMEM;
    } else {
        //rte_mbuf_sanity_check(mbuf, true);
        *mbuf_out = static_cast<void *>(mbuf);
        *payload_addr = static_cast<char *>(mbuf->buf_addr) + mbuf->data_off;
        return 0;
    }
}

int UdpContext::free_mbuf(unsigned long mbuf) {
   rte_pktmbuf_free(static_cast<struct rte_mbuf *>((void *)mbuf));
   return 0;
}
