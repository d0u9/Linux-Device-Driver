/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>

#define MODULE_NAME		"netdev"

/* the simulated interrupt status */
#define NETDEV_RX_INTR		0x0001	/* a packet arrived */
#define NETDEV_TX_INTR		0x0002	/* a transmission is done */

/* A buffer of the simulated hardware, holding one frame. */
struct ldd_packet {
	struct ldd_packet *next;
	struct net_device *dev;		/* the device whose pool it is from */
	int datalen;
	u8 data[ETH_FRAME_LEN];
};

struct ldd_dev {
	struct net_device *ndev;
	spinlock_t lock;		/* protects the fields below */
	bool running;			/* open: frames are accepted */
	bool rx_int_enabled;
	int status;			/* NETDEV_*_INTR pending */
	struct ldd_packet *pkt_arr;	/* the pool's memory */
	struct ldd_packet *pool;	/* free transmit buffers */
	struct ldd_packet *rx_head;	/* received, oldest first */
	struct ldd_packet *rx_tail;
	struct sk_buff *tx_skb;		/* freed by the TX interrupt */
	struct napi_struct napi;
};

#endif
