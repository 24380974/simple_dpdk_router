#include "main.h"

#define  SLEEP_TIME 5

struct app_params app = {
	/* Ports*/
	.n_ports = APP_MAX_PORTS,
	.port_rx_ring_size = 128,
	.port_tx_ring_size = 512,

	/* Rings */
	.ring_rx_size = 128,
	.ring_tx_size = 128,

	/* Buffer pool */
	.pool_buffer_size = 2048 + RTE_PKTMBUF_HEADROOM,
	.pool_size = 32 * 1024,
	.pool_cache_size = 256,

	/* Burst sizes */
	.burst_size_rx_read = 64,
	.burst_size_rx_write = 64,
	.burst_size_fw_read = 64,
	.burst_size_fw_write = 64,
	.burst_size_tx_read = 64,
	.burst_size_tx_write = 64,
};

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /* Header Split disabled */
		.hw_ip_checksum = 1, /* IP checksum offload enabled */
		.hw_vlan_filter = 0, /* VLAN filtering disabled */
		.jumbo_frame    = 0, /* Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /* CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
	.rx_free_thresh = 64,
	.rx_drop_en = 0,
};

static struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = 36,
		.hthresh = 0,
		.wthresh = 4,
	},
	.tx_free_thresh = 0,
	.tx_rs_thresh = 0,
};

static void app_init_mbuf_pools(void) {
	/* Init the buffer pool */
	RTE_LOG(INFO, USER1, "Creating the mbuf pool ...\n");
	app.pool = rte_pktmbuf_pool_create("mempool", app.pool_size,
		app.pool_cache_size, 0, app.pool_buffer_size, rte_socket_id());
	if (app.pool == NULL)
		rte_panic("Cannot create mbuf pool\n");
}

static void app_init_rings(void) {
	uint32_t i;
  RTE_LOG(INFO, USER1, "Creating the RX/TX rings ...\n");
  /* Create RX rings */
	for (i = 0; i < app.n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "app_ring_rx_%u", i);

		app.rings_rx[i] = rte_ring_create(
			name,
			app.ring_rx_size,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (app.rings_rx[i] == NULL)
			rte_panic("Cannot create RX ring %u\n", i);
	}

  /* Create TX rings */
	for (i = 0; i < app.n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "app_ring_tx_%u", i);

		app.rings_tx[i] = rte_ring_create(
			name,
			app.ring_tx_size,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (app.rings_tx[i] == NULL)
			rte_panic("Cannot create TX ring %u\n", i);
	}

}

static void app_ports_check_link(void) {
	uint32_t all_ports_up, i;

	all_ports_up = 1;

	for (i = 0; i < app.n_ports; i++) {
		struct rte_eth_link link;
		uint8_t port;

		port = (uint8_t) app.ports[i];
		memset(&link, 0, sizeof(link));
		rte_eth_link_get_nowait(port, &link);
		RTE_LOG(INFO, USER1, "Port %u (%u Gbps) %s\n",
			port,
			link.link_speed / 1000,
			link.link_status ? "UP" : "DOWN");

		if (link.link_status == ETH_LINK_DOWN)
			all_ports_up = 0;
	}

	if (all_ports_up == 0)
		rte_panic("Some NIC ports are DOWN\n");
}

static void app_init_ports(void) {
	uint32_t i;

	/* Init NIC ports, then start the ports */
	for (i = 0; i < app.n_ports; i++) {
		uint8_t port;
		int ret;

		port = (uint8_t) app.ports[i];
		RTE_LOG(INFO, USER1, "Initializing NIC port %u ...\n", port);

		/* Init port */
		ret = rte_eth_dev_configure(
			port,
			1,
			1,
			&port_conf);
		if (ret < 0)
			rte_panic("Cannot init NIC port %u (%d)\n", port, ret);

		rte_eth_promiscuous_enable(port);

		/* Init RX queues */
		ret = rte_eth_rx_queue_setup(
			port,
			0,
			app.port_rx_ring_size,
			rte_eth_dev_socket_id(port),
			&rx_conf,
			app.pool);
		if (ret < 0)
			rte_panic("Cannot init RX for port %u (%d)\n",
				(uint32_t) port, ret);

		/* Init TX queues */
		ret = rte_eth_tx_queue_setup(
			port,
			0,
			app.port_tx_ring_size,
			rte_eth_dev_socket_id(port),
			&tx_conf);
		if (ret < 0)
			rte_panic("Cannot init TX for port %u (%d)\n",
				(uint32_t) port, ret);

		/* Start port */
		ret = rte_eth_dev_start(port);
		if (ret < 0)
			rte_panic("Cannot start port %u (%d)\n", port, ret);
	}

	RTE_LOG(INFO, USER1, "Waiting for %d seconds before checking link status...\n", SLEEP_TIME);

	sleep(SLEEP_TIME);

	app_ports_check_link();
}

void app_init(void) {
	app_init_mbuf_pools();
	app_init_rings();
	app_init_ports();

	RTE_LOG(INFO, USER1, "Initialization completed\n");
}
