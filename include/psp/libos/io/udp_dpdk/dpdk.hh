#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

// device configuration
#define RX_RING_SIZE 128
#define TX_RING_SIZE 128

#define DEFAULT_TX_RS_THRESH 32

#define MAX_PATTERN_NUM 3
#define MAX_ACTION_NUM 3
int rte_eth_dev_flow_ctrl_get(uint16_t port_id, struct rte_eth_fc_conf &fc_conf);
int rte_eth_dev_flow_ctrl_set(uint16_t port_id, const struct rte_eth_fc_conf &fc_conf);
rte_flow * generate_ipv4_flow(uint16_t port_id, uint16_t rx_q,
                              uint32_t src_ip, uint32_t src_mask,
                              uint32_t dest_ip, uint32_t dest_mask,
                              struct rte_flow_error *error);
int dpdk_net_init(const char *app_cfg_filename);
int init_dpdk_port(uint16_t port_id, rte_mempool *mbuf_pool,
                   uint16_t n_tx_rings, uint16_t n_rx_rings);

int rte_eth_dev_socket_id(int &sockid_out, uint16_t port_id);
int rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
                           uint16_t nb_rx_desc, int socket_id,
                           const struct rte_eth_rxconf &rx_conf, struct rte_mempool *mb_pool);
int rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id, uint16_t nb_tx_desc,
                           unsigned int socket_id, const struct rte_eth_txconf &tx_conf);
int rte_eal_init(int &count_out, int argc, char *argv[]);
int wait_for_link_status_up(uint16_t port_id);
int rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link &link);
int print_link_status(FILE *f, uint16_t port_id, const struct rte_eth_link *link);
int rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr &mac_addr);
int rte_eth_dev_mac_addr_add(uint16_t port_id, struct rte_ether_addr &mac_addr);
int print_ether_addr(FILE *f, struct rte_ether_addr &eth_addr);
int rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info &dev_info);
int rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue, const struct rte_eth_conf &eth_conf);
int print_dpdk_device_stats(const uint16_t port_id);
