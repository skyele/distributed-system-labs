#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

static void build_udp_packet(struct rte_mbuf *m){
	struct ether_hdr *eth_hdr;
    struct ipv4_hdr  *ipv4_hdr;
	struct udp_hdr   *udp_hdr;
	
	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
    ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,  sizeof(struct ether_hdr));
	udp_hdr = rte_pktmbuf_mtod_offset(m, struct udp_hdr *, sizeof(struct ether_hdr)+sizeof(struct ipv4_hdr));

	udp_hdr->src_port = 0x00 << 8;
	udp_hdr->dst_port = 0x01 << 8;
	udp_hdr->dgram_len = (11 + sizeof(struct udp_hdr))<<8;
	udp_hdr->dgram_cksum = 0xffff;

	struct ether_addr s_addr = {{0x14,0x02,0xEC,0x89,0x8D,0x24}};
	struct ether_addr d_addr = {{0x14,0x02,0xEC,0x89,0xED,0x54}};	

	eth_hdr->d_addr = d_addr;
	eth_hdr->s_addr = s_addr;
	eth_hdr->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);
	printf("0x0a00 and %x\n", rte_be_to_cpu_16(ETHER_TYPE_IPv4));
	if(0x0a00 == rte_be_to_cpu_16(ETHER_TYPE_IPv4))
		printf("same!!!!\n");
	ipv4_hdr->version_ihl = (4 << 4) | 5;
	ipv4_hdr->type_of_service = 0;
	ipv4_hdr->src_addr = IPv4(10,80,168,192);
	ipv4_hdr->dst_addr = IPv4(6,80,168,192);
	ipv4_hdr->total_length = 50;
	ipv4_hdr->packet_id = 15;
	ipv4_hdr->fragment_offset = 128;
	ipv4_hdr->time_to_live = 0xff;
	ipv4_hdr->next_proto_id = 0x11;
	ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(void)
{
	struct rte_mempool *mbuf_pool;
	const uint8_t nb_ports = rte_eth_dev_count();
	uint8_t port;

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MY_BUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */
	struct rte_mbuf *packets[1];
	packets[0] = rte_pktmbuf_alloc(mbuf_pool);
	rte_pktmbuf_prepend(packets[0], sizeof(struct udp_hdr)+sizeof(struct ether_hdr)+sizeof(struct ipv4_hdr)+11);
	build_udp_packet(packets[0]);
	void *payload = rte_pktmbuf_mtod_offset(packets[0], void *,
            sizeof(struct udp_hdr)+sizeof(struct ether_hdr)+sizeof(struct ipv4_hdr));
	//add data
	memcpy(payload, "hello world", 11);
	for (;;) {
		rte_eth_tx_burst(0, 0, packets, 1);
		printf("hello udp\n");
		sleep(1);
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();
	printf("port num:%d\n", nb_ports);

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}