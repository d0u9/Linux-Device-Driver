#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME			"netdev"

#define NETDEV_RX_INTR			0x0001
#define NETDEV_TX_INTR			0x0002
#define NETDEV_TIMEOUT			5

struct ldd_packet {
	struct ldd_packet *next;
	struct net_device *dev;
	int	datalen;
	u8 data[ETH_DATA_LEN];
};

struct ldd_dev {
	struct net_device *net_dev;
	struct net_device_stats stats;
	struct ldd_packet *pkt_pool;
	struct ldd_packet *pkt_arr;
	struct ldd_packet *rx_queue;
	int tx_packetlen;
	u8 *tx_packetdata;
	int status;
	spinlock_t lock;
	struct sk_buff *skb;
	bool rx_int_enabled;
	struct napi_struct napi;
};

#endif
