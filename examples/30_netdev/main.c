// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <net/ip.h>

#include "main.h"

static int pool_size = 8;
module_param(pool_size, int, 0444);
MODULE_PARM_DESC(pool_size, "transmit buffers per device (default 8)");

static bool use_napi;
module_param(use_napi, bool, 0444);
MODULE_PARM_DESC(use_napi, "receive with NAPI polling instead of per-packet interrupts");

/* sn0 and sn1: what one sends, the other receives */
static struct net_device *net_devs[2];

/* the interrupt handler in use, called where hardware would interrupt */
static void (*ldd_interrupt)(struct net_device *dev);

static struct net_device *ldd_peer(struct net_device *dev)
{
	return net_devs[dev == net_devs[0] ? 1 : 0];
}

/*
 * Transmit buffers. A device takes one per frame and stops its queue when
 * none is left; the receiver gives it back, which wakes the queue.
 */
static struct ldd_packet *ldd_get_tx_buffer(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	struct ldd_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->pool;
	if (pkt) {
		priv->pool = pkt->next;
		if (!priv->pool)
			netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

static void ldd_release_buffer(struct ldd_packet *pkt)
{
	struct ldd_dev *priv = netdev_priv(pkt->dev);
	unsigned long flags;
	bool was_empty;

	spin_lock_irqsave(&priv->lock, flags);
	was_empty = !priv->pool;
	pkt->next = priv->pool;
	priv->pool = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);

	if (was_empty)
		netif_wake_queue(pkt->dev);
}

/* Called with priv->lock held. */
static struct ldd_packet *ldd_rx_dequeue(struct ldd_dev *priv)
{
	struct ldd_packet *pkt = priv->rx_head;

	if (pkt) {
		priv->rx_head = pkt->next;
		if (!priv->rx_head)
			priv->rx_tail = NULL;
	}
	return pkt;
}

/* A frame arrives at @dev: queue it, oldest first, and interrupt. */
static void ldd_rx_enqueue(struct net_device *dev, struct ldd_packet *pkt)
{
	struct ldd_dev *priv = netdev_priv(dev);
	bool interrupt = false;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (!priv->running) {
		spin_unlock_irqrestore(&priv->lock, flags);
		ldd_release_buffer(pkt);	/* nobody listening: lost */
		return;
	}
	pkt->next = NULL;
	if (priv->rx_tail)
		priv->rx_tail->next = pkt;
	else
		priv->rx_head = pkt;
	priv->rx_tail = pkt;
	if (priv->rx_int_enabled) {
		priv->status |= NETDEV_RX_INTR;
		interrupt = true;
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	if (interrupt)
		ldd_interrupt(dev);
}

/*
 * Wrap a received frame in an skb and hand it to the network stack.
 * dev_alloc_skb() and skb_reserve(NET_IP_ALIGN) together are what
 * netdev_alloc_skb_ip_align() does: the 14-byte Ethernet header is moved
 * 2 bytes on, so that the IP header behind it is 4-byte aligned.
 */
static void ldd_rx(struct net_device *dev, struct ldd_packet *pkt, bool napi)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(pkt->datalen + NET_IP_ALIGN);
	if (!skb) {
		if (net_ratelimit())
			netdev_notice(dev, "rx: low on memory, packet dropped\n");
		DEV_STATS_INC(dev, rx_dropped);
		return;
	}
	skb_reserve(skb, NET_IP_ALIGN);
	skb_put_data(skb, pkt->data, pkt->datalen);

	skb->protocol = eth_type_trans(skb, dev);	/* also sets skb->dev */
	skb->ip_summed = CHECKSUM_UNNECESSARY;	/* see NETIF_F_HW_CSUM */

	DEV_STATS_INC(dev, rx_packets);
	DEV_STATS_ADD(dev, rx_bytes, pkt->datalen);

	/*
	 * From an interrupt, netif_rx() queues the skb for the stack's
	 * softirq; from NAPI's poll, which is that softirq, the stack takes
	 * it at once. napi_gro_receive() would also merge related packets.
	 */
	if (napi)
		netif_receive_skb(skb);
	else
		netif_rx(skb);
}

/* Read and clear the interrupt status; take the finished TX skb along. */
static int ldd_ack_interrupt(struct ldd_dev *priv, struct sk_buff **tx_skb)
{
	unsigned long flags;
	int status;

	spin_lock_irqsave(&priv->lock, flags);
	status = priv->status;
	priv->status = 0;
	*tx_skb = NULL;
	if (status & NETDEV_TX_INTR) {
		*tx_skb = priv->tx_skb;
		priv->tx_skb = NULL;
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return status;
}

static void ldd_tx_done(struct net_device *dev, struct sk_buff *skb)
{
	DEV_STATS_INC(dev, tx_packets);
	DEV_STATS_ADD(dev, tx_bytes, skb->len);
	/* _any: fine with interrupts disabled, as in a real handler */
	dev_kfree_skb_any(skb);
}

/* Without NAPI: every received frame interrupts and is passed up here. */
static void ldd_regular_interrupt(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	struct ldd_packet *pkt = NULL;
	struct sk_buff *tx_skb;
	unsigned long flags;
	int status;

	status = ldd_ack_interrupt(priv, &tx_skb);

	if (status & NETDEV_RX_INTR) {
		spin_lock_irqsave(&priv->lock, flags);
		pkt = ldd_rx_dequeue(priv);
		spin_unlock_irqrestore(&priv->lock, flags);
	}
	if (pkt) {
		ldd_rx(dev, pkt, false);
		ldd_release_buffer(pkt);
	}
	if (tx_skb)
		ldd_tx_done(dev, tx_skb);
}

/*
 * With NAPI: the first frame interrupts; the handler turns receive
 * interrupts off and schedules the poll function, which then takes frames
 * without further interrupts, up to a budget at a time.
 */
static void ldd_napi_interrupt(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	struct sk_buff *tx_skb;
	unsigned long flags;
	int status;

	status = ldd_ack_interrupt(priv, &tx_skb);

	if (status & NETDEV_RX_INTR) {
		spin_lock_irqsave(&priv->lock, flags);
		priv->rx_int_enabled = false;
		spin_unlock_irqrestore(&priv->lock, flags);
		napi_schedule(&priv->napi);
	}
	if (tx_skb)
		ldd_tx_done(dev, tx_skb);
}

static int ldd_poll(struct napi_struct *napi, int budget)
{
	struct ldd_dev *priv = container_of(napi, struct ldd_dev, napi);
	struct net_device *dev = priv->ndev;
	struct ldd_packet *pkt;
	unsigned long flags;
	bool more;
	int done = 0;

	while (done < budget) {
		spin_lock_irqsave(&priv->lock, flags);
		pkt = ldd_rx_dequeue(priv);
		spin_unlock_irqrestore(&priv->lock, flags);
		if (!pkt)
			break;
		ldd_rx(dev, pkt, true);
		ldd_release_buffer(pkt);
		done++;
	}

	/* Budget used up: stay scheduled, NAPI calls again. */
	if (done == budget)
		return done;

	if (napi_complete_done(napi, done)) {
		/*
		 * Turn receive interrupts back on. A frame queued after the
		 * loop above found the queue empty raised no interrupt, so
		 * look once more.
		 */
		spin_lock_irqsave(&priv->lock, flags);
		more = priv->rx_head;
		priv->rx_int_enabled = !more;
		spin_unlock_irqrestore(&priv->lock, flags);
		if (more)
			napi_schedule(napi);
	}
	return done;
}

/*
 * Only IPv4 is passed: the source and destination addresses get the
 * lowest bit of their third byte flipped, so that 192.168.0.2, sent out
 * of sn0, arrives at sn1 as one of its own addresses, 192.168.1.2.
 */
static netdev_tx_t ldd_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	struct net_device *peer = ldd_peer(dev);
	unsigned int len = max_t(unsigned int, skb->len, ETH_ZLEN);
	struct ldd_packet *pkt;
	unsigned long flags;
	struct iphdr *ih;

	if (skb->protocol != htons(ETH_P_IP) || !peer ||
	    skb->len < ETH_HLEN + sizeof(*ih) || skb->len > ETH_FRAME_LEN) {
		DEV_STATS_INC(dev, tx_dropped);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* the queue stops as the last buffer is taken, so this is rare */
	pkt = ldd_get_tx_buffer(dev);
	if (!pkt)
		return NETDEV_TX_BUSY;

	/*
	 * Copy the frame, short ones padded with zeros to the Ethernet
	 * minimum, and change the copy only: the skb's data may be shared
	 * with clones, such as the one TCP keeps to retransmit.
	 */
	memset(pkt->data, 0, len);
	skb_copy_bits(skb, 0, pkt->data, skb->len);
	pkt->datalen = len;

	ih = (struct iphdr *)(pkt->data + ETH_HLEN);
	((u8 *)&ih->saddr)[2] ^= 1;
	((u8 *)&ih->daddr)[2] ^= 1;
	ip_send_check(ih);		/* the IP header checksum */

	/* the "wire": the twin receives it */
	ldd_rx_enqueue(peer, pkt);

	/* and the transmission is done: the TX interrupt frees the skb */
	spin_lock_irqsave(&priv->lock, flags);
	priv->tx_skb = skb;
	priv->status |= NETDEV_TX_INTR;
	spin_unlock_irqrestore(&priv->lock, flags);
	ldd_interrupt(dev);

	return NETDEV_TX_OK;
}

static int ldd_open(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	unsigned long flags;

	if (use_napi)
		napi_enable(&priv->napi);

	spin_lock_irqsave(&priv->lock, flags);
	priv->running = true;
	priv->rx_int_enabled = true;
	spin_unlock_irqrestore(&priv->lock, flags);

	netif_start_queue(dev);
	return 0;
}

static int ldd_stop(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);
	struct ldd_packet *pkt;
	unsigned long flags;

	netif_stop_queue(dev);

	spin_lock_irqsave(&priv->lock, flags);
	priv->running = false;
	priv->rx_int_enabled = false;
	spin_unlock_irqrestore(&priv->lock, flags);

	if (use_napi)
		napi_disable(&priv->napi);

	/* give back what was received but not passed up */
	for (;;) {
		spin_lock_irqsave(&priv->lock, flags);
		pkt = ldd_rx_dequeue(priv);
		spin_unlock_irqrestore(&priv->lock, flags);
		if (!pkt)
			break;
		ldd_release_buffer(pkt);
	}
	return 0;
}

/*
 * Build the Ethernet header. There is no ARP (IFF_NOARP), so the
 * destination is our own address with the last bit flipped: the twin's.
 */
static int ldd_header(struct sk_buff *skb, struct net_device *dev,
		      unsigned short type, const void *daddr,
		      const void *saddr, unsigned int len)
{
	struct ethhdr *eth = skb_push(skb, ETH_HLEN);

	eth->h_proto = htons(type);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN - 1] ^= 0x01;
	return dev->hard_header_len;
}

static const struct header_ops ldd_header_ops = {
	.create = ldd_header,
};

/* Statistics live in dev->stats, which the core reports by itself. */
static const struct net_device_ops ldd_netdev_ops = {
	.ndo_open       = ldd_open,
	.ndo_stop       = ldd_stop,
	.ndo_start_xmit = ldd_start_xmit,
};

/* Called by alloc_netdev() to set up the new device. */
static void ldd_setup(struct net_device *dev)
{
	struct ldd_dev *priv = netdev_priv(dev);

	ether_setup(dev);
	dev->netdev_ops = &ldd_netdev_ops;
	dev->header_ops = &ldd_header_ops;
	dev->flags |= IFF_NOARP;
	/*
	 * Claim to checksum in hardware, so the stack leaves TCP and UDP
	 * checksums undone; the receiver then says CHECKSUM_UNNECESSARY. The
	 * address rewrite would break those checksums anyway.
	 */
	dev->features |= NETIF_F_HW_CSUM;

	priv->ndev = dev;
	spin_lock_init(&priv->lock);
}

static int ldd_create(int i)
{
	u8 addr[ETH_ALEN] = { 0x00, 'S', 'N', 'U', 'L', '0' + i };
	struct net_device *dev;
	struct ldd_dev *priv;
	int j, err;

	dev = alloc_netdev(sizeof(*priv), "sn%d", NET_NAME_ENUM, ldd_setup);
	if (!dev)
		return -ENOMEM;
	priv = netdev_priv(dev);
	eth_hw_addr_set(dev, addr);

	priv->pkt_arr = kzalloc_objs(*priv->pkt_arr, pool_size);
	if (!priv->pkt_arr) {
		err = -ENOMEM;
		goto err_free;
	}
	for (j = 0; j < pool_size; j++) {
		priv->pkt_arr[j].dev = dev;
		priv->pkt_arr[j].next = j + 1 < pool_size ?
					&priv->pkt_arr[j + 1] : NULL;
	}
	priv->pool = priv->pkt_arr;

	/*
	 * A weight of 2 lets poll() be seen running out of budget with a
	 * few frames; netif_napi_add() uses the usual 64.
	 */
	if (use_napi)
		netif_napi_add_weight(dev, &priv->napi, ldd_poll, 2);

	net_devs[i] = dev;
	err = register_netdev(dev);
	if (err)
		goto err_pool;
	return 0;

err_pool:
	net_devs[i] = NULL;
	kfree(priv->pkt_arr);
err_free:
	free_netdev(dev);		/* also deletes the NAPI context */
	return err;
}

/*
 * Unregister both before freeing either: a device still up may send to
 * its twin, and finds it not running until it is freed.
 */
static void ldd_destroy(int n)
{
	struct ldd_dev *priv;
	int i;

	for (i = 0; i < n; i++)
		unregister_netdev(net_devs[i]);
	for (i = 0; i < n; i++) {
		priv = netdev_priv(net_devs[i]);
		kfree(priv->pkt_arr);
		free_netdev(net_devs[i]);
		net_devs[i] = NULL;
	}
}

static int __init m_init(void)
{
	int i, err;

	if (pool_size < 1)
		return -EINVAL;

	ldd_interrupt = use_napi ? ldd_napi_interrupt : ldd_regular_interrupt;

	for (i = 0; i < 2; i++) {
		err = ldd_create(i);
		if (err) {
			ldd_destroy(i);
			return err;
		}
	}
	return 0;
}

static void __exit m_exit(void)
{
	ldd_destroy(2);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("snull: two network interfaces wired to each other");
