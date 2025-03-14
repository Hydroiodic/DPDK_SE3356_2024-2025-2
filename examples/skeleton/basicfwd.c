/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <inttypes.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define TX_BURST_SIZE 8

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */

/* Main functional part of port initialization. 8< */
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    memset(&port_conf, 0, sizeof(struct rte_eth_conf));

    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0) {
        printf("Error during getting device (port %u) info: %s\n", port,
               strerror(-retval));
        return retval;
    }

    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(
            port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    /* Starting Ethernet port. 8< */
    retval = rte_eth_dev_start(port);
    /* >8 End of starting of ethernet port. */
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct rte_ether_addr addr;
    retval = rte_eth_macaddr_get(port, &addr);
    if (retval != 0)
        return retval;

    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
           " %02" PRIx8 " %02" PRIx8 "\n",
           port, RTE_ETHER_ADDR_BYTES(&addr));

    /* Enable RX in promiscuous mode for the Ethernet device. */
    retval = rte_eth_promiscuous_enable(port);
    /* End of setting RX port in promiscuous mode. */
    if (retval != 0)
        return retval;

    return 0;
}
/* >8 End of main functional part of port initialization. */

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */

/* Basic forwarding application lcore. 8< */
static __rte_noreturn void lcore_main(void) {
    uint16_t port;

    /*
     * Check that the port is on the same NUMA node as the polling thread
     * for best performance.
     */
    RTE_ETH_FOREACH_DEV(port)
    if (rte_eth_dev_socket_id(port) >= 0 &&
        rte_eth_dev_socket_id(port) != (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node to "
               "polling thread.\n\tPerformance will "
               "not be optimal.\n",
               port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());

    /* Main work of application loop. 8< */
    for (;;) {
        /*
         * Receive packets on a port and forward them on the paired
         * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
         */
        RTE_ETH_FOREACH_DEV(port) {
            // Use an array to store the packets
            struct rte_mbuf *pkts_burst[TX_BURST_SIZE];
            uint16_t nb_rx =
                rte_eth_rx_burst(port, 0, pkts_burst, TX_BURST_SIZE);
            if (nb_rx == 0)
                continue;

            // Log the number of packets received
            printf("Receiving a %hu-burst\n", nb_rx);

            // Process the packets
            for (uint16_t i = 0; i < nb_rx; i++) {
                struct rte_mbuf *m = pkts_burst[i];

                // Parse ethernet header for IPv4
                struct rte_ether_hdr *eth_hdr =
                    rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
                if (eth_hdr->ether_type !=
                    rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                    continue;
                }

                // Check IPv4 header
                struct rte_ipv4_hdr *ipv4_hdr =
                    (struct rte_ipv4_hdr *)(eth_hdr + 1);
                if (ipv4_hdr->next_proto_id != IPPROTO_UDP) {
                    continue;
                }

                // Check UDP header
                struct rte_udp_hdr *udp_hdr =
                    (struct rte_udp_hdr *)(ipv4_hdr + 1);
                if (udp_hdr->dst_port != rte_cpu_to_be_16(5678)) {
                    continue;
                }

                // Check payload
                char *payload = (char *)(udp_hdr + 1);
                size_t payload_len = m->pkt_len - sizeof(struct rte_ether_hdr) -
                                     sizeof(struct rte_ipv4_hdr) -
                                     sizeof(struct rte_udp_hdr);

                // Dump Ethernet, IPv4, and UDP headers
                printf("<Packet Start>\n");

                printf("Ethernet Header = {\n");
                printf("\tSource MAC: ");
                for (int i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
                    printf("%02X ", eth_hdr->src_addr.addr_bytes[i]);
                }
                printf(",\n");
                printf("\tDestination MAC: ");
                for (int i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
                    printf("%02X ", eth_hdr->dst_addr.addr_bytes[i]);
                }
                printf(",\n");
                printf("\tEthernet Type: 0x%04X,\n",
                       rte_be_to_cpu_16(eth_hdr->ether_type));
                printf("},\n");

                printf("IPv4 Header = {\n");
                printf("\tVersion: %d,\n", ipv4_hdr->version_ihl >> 4);
                printf("\tIHL: %d,\n", ipv4_hdr->version_ihl & 0x0F);
                printf("\tTotal Length: %d,\n",
                       rte_be_to_cpu_16(ipv4_hdr->total_length));
                printf("\tPacket ID: %d,\n",
                       rte_be_to_cpu_16(ipv4_hdr->packet_id));
                uint32_t src_addr = rte_be_to_cpu_32(ipv4_hdr->src_addr);
                uint32_t dst_addr = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
                printf("\tsrc_addr: %d.%d.%d.%d,\n", (src_addr >> 24) & 0xFF,
                       (src_addr >> 16) & 0xFF, (src_addr >> 8) & 0xFF,
                       src_addr & 0xFF);
                printf("\tdst_addr: %d.%d.%d.%d,\n", (dst_addr >> 24) & 0xFF,
                       (dst_addr >> 16) & 0xFF, (dst_addr >> 8) & 0xFF,
                       dst_addr & 0xFF);
                printf("},\n");

                printf("UDP Header = {\n");
                printf("\tSource Port: %d,\n",
                       rte_be_to_cpu_16(udp_hdr->src_port));
                printf("\tDestination Port: %d,\n",
                       rte_be_to_cpu_16(udp_hdr->dst_port));
                printf("\tLength: %d,\n", rte_be_to_cpu_16(udp_hdr->dgram_len));
                printf("},\n");

                // Dump UDP payload
                printf("Payload = {\n\t");
                for (size_t i = 0; i < payload_len; i++) {
                    printf("'%c', ", payload[i]);
                }
                printf("\n}\n");

                printf("<Packet End>\n");

                // free the mbuf
                rte_pktmbuf_free(m);
            }

            printf("\n");
        }
    }
    /* >8 End of loop. */
}
/* >8 End Basic forwarding application lcore. */

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint16_t portid;

    /* Initializion the Environment Abstraction Layer (EAL). 8< */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    /* >8 End of initialization the Environment Abstraction Layer (EAL). */

    argc -= ret;
    argv += ret;

    /* Check that there is only one port to send/receive on. */
    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports != 1)
        rte_exit(EXIT_FAILURE, "Error: number of ports must be one\n");

    /* Creates a new mempool in memory to hold the mbufs. */

    /* Allocates mempool to hold the mbufs. 8< */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    /* >8 End of allocating mempool to hold mbuf. */

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initializing all ports. 8< */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);
    /* >8 End of initializing all ports. */

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

    /* Call lcore_main on the main core only. Called on single lcore. 8< */
    lcore_main();
    /* >8 End of called on single lcore. */

    /* clean up the EAL */
    rte_eal_cleanup();

    return 0;
}
