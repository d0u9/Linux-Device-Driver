#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <net/ip.h>

#include "main.h"

static void (*ldd_interrupt)(int, void *, struct pt_regs *);

static int timeout = NETDEV_TIMEOUT;
module_param(timeout, int, 0);

int pool_size = 8;
module_param(pool_size, int, 0);

static int lockup = 0;
module_param(lockup, int, 0);

static int use_napi = 0;
module_param(use_napi, int, 0);

static struct net_device *net_devs[2];

void ldd_dev_rx(struct net_device *dev, struct ldd_packet *pkt)
{
	struct sk_buff *skb;
	struct ldd_dev *ldd_dev = netdev_priv(dev);

	/*
	 * The packet has been retrieved from the transmission
	 * medium. Build an skb around it, so upper layers can handle it
	 */
	skb = dev_alloc_skb(pkt->datalen + 2); // TODO: why add 2?
	if (!skb) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "snull rx: low on mem - packet dropped\n");
		ldd_dev->stats.rx_dropped++;
		goto out;
	}
	skb_reserve(skb, 2); /* align IP on 16B boundary */  
	memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

	/* Write metadata, and then pass to the receive level */
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */

	ldd_dev->stats.rx_packets++;
	ldd_dev->stats.rx_bytes += pkt->datalen;
	pr_debug("====== push skb ========\n");

	print_hex_dump(KERN_DEBUG, "skb int raw: ", DUMP_PREFIX_OFFSET,
		       16, 1, skb->data, skb->len, true);
	netif_rx(skb);
  out:
	return;
}

void ldd_release_buffer(struct ldd_packet *pkt)
{
	unsigned long flags;
	struct ldd_dev *ldd_dev = netdev_priv(pkt->dev);
	
	spin_lock_irqsave(&ldd_dev->lock, flags);
	pkt->next = ldd_dev->pkt_pool;
	ldd_dev->pkt_pool = pkt;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);

	if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
		netif_wake_queue(pkt->dev);

	pr_debug("release pkt: dev = %s\n", pkt->dev->name);
}

static
void ldd_enqueue_buf(struct net_device *dev, struct ldd_packet *pkt)
{
	unsigned long flags;
	struct ldd_dev *ldd_dev = netdev_priv(dev);

	spin_lock_irqsave(&ldd_dev->lock, flags);
	pkt->next = ldd_dev->rx_queue;  /* FIXME - misorders packets */
	ldd_dev->rx_queue = pkt;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
}

static
struct ldd_packet *ldd_dequeue_buf(struct net_device *dev)
{
	struct ldd_dev *ldd_dev = netdev_priv(dev);
	struct ldd_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&ldd_dev->lock, flags);
	pkt = ldd_dev->rx_queue;
	if (pkt != NULL)
		ldd_dev->rx_queue = pkt->next;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
	return pkt;
}

static
void ldd_regular_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int statusword;
	unsigned long flags;
	struct ldd_dev *ldd_dev;
	struct ldd_packet *pkt = NULL;
	struct net_device *dev = (struct net_device *)dev_id;

	if (!dev)
		return;

	ldd_dev = netdev_priv(dev);
	spin_lock_irqsave(&ldd_dev->lock, flags);

	statusword = ldd_dev->status;
	ldd_dev->status = 0;

	if (statusword & NETDEV_RX_INTR) {
		pkt = ldd_dev->rx_queue;
		if (pkt) {
			ldd_dev->rx_queue = pkt->next;
			pr_debug("====== recv ========");
			ldd_dev_rx(dev, pkt);
		}
	}

	if (statusword & NETDEV_TX_INTR) {
		ldd_dev->stats.tx_packets++;
		ldd_dev->stats.tx_bytes += ldd_dev->tx_packetlen;
		dev_kfree_skb(ldd_dev->skb);
	}

	spin_unlock_irqrestore(&ldd_dev->lock, flags);

	if (pkt)
		ldd_release_buffer(pkt); /* Do this outside the lock! */
}

static
int ldd_dev_poll(struct napi_struct *napi, int budget)
{
	int rv, npackets = 0;
	unsigned long flags;
	struct sk_buff *skb;
	struct ldd_dev *ldd_dev = container_of(napi, struct ldd_dev, napi);
	struct net_device *dev = ldd_dev->net_dev;
	struct ldd_packet *pkt;

	pr_debug("========= budget: %d, dev = %s\n", budget, dev->name);

	while (npackets < budget && ldd_dev->rx_queue) {
		pr_debug("------ deque %s!\n", dev->name);
		pkt = ldd_dequeue_buf(dev);
		skb = dev_alloc_skb(pkt->datalen + 2);
		if (!skb) {
			if (printk_ratelimit())
				pr_err("snull: packet dropped\n");
			ldd_dev->stats.rx_dropped++;
			npackets++;
			ldd_release_buffer(pkt);
			continue;
		}
		skb_reserve(skb, 2);
		memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		print_hex_dump(KERN_DEBUG, "skb poll raw: ", DUMP_PREFIX_OFFSET,
			       16, 1, skb->data, skb->len, true);

		rv = netif_receive_skb(skb);
		pr_debug("rv = %s\n", rv == NET_RX_SUCCESS ? "suc" : "drop");

		npackets++;
		ldd_dev->stats.rx_packets++;
		ldd_dev->stats.rx_bytes += pkt->datalen;
		ldd_release_buffer(pkt);
	}

	if (npackets < budget) {
		spin_lock_irqsave(&ldd_dev->lock, flags);
		if (napi_complete_done(napi, npackets)) {
			pr_debug("openirq: %s, npackets = %d\n", dev->name, npackets);
			ldd_dev->rx_int_enabled = true;
		}
		spin_unlock_irqrestore(&ldd_dev->lock, flags);
	}
	return npackets;
}

static
void ldd_napi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int statusword;
	unsigned long flags;
	struct ldd_dev *ldd_dev;

	/*
	 * As usual, check the "device" pointer for shared handlers.
	 * Then assign "struct device *dev"
	 */
	struct net_device *dev = (struct net_device *)dev_id;
	/* ... and check with hw if it's really ours */

	/* paranoid */
	if (!dev)
		return;

	/* Lock the device */
	ldd_dev = netdev_priv(dev);
	// spin_lock_irqsave(&ldd_dev->lock, flags);
	spin_lock(&ldd_dev->lock);

	/* retrieve statusword: real netdevices use I/O instructions */
	statusword = ldd_dev->status;
	ldd_dev->status = 0;
	if (statusword & NETDEV_RX_INTR) {
		pr_debug("schedule: %s, %s, napi=%px\n", dev->name,
			 ldd_dev->net_dev->name, &ldd_dev->napi);
			ldd_dev->rx_int_enabled = false; /* Disable further interrupts */
			napi_schedule(&ldd_dev->napi);
	}
	if (statusword & NETDEV_TX_INTR) {
        	/* a transmission is over: free the skb */
		ldd_dev->stats.tx_packets++;
		ldd_dev->stats.tx_bytes += ldd_dev->tx_packetlen;
		if(ldd_dev->skb) {
			dev_kfree_skb(ldd_dev->skb);
			ldd_dev->skb = 0;
		}
	}

	/* Unlock the device and we are done */
	// spin_unlock_irqrestore(&ldd_dev->lock, flags);
	spin_unlock(&ldd_dev->lock);
}


static
int ldd_netdev_open(struct net_device *dev)
{
	/* request_region(), request_irq(), ....  (like fops->open) */

	struct ldd_dev *ldd_dev = netdev_priv(dev);

	pr_debug("========= open %s ====== \n", dev->name);

	memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);
	if (dev == net_devs[1])
		dev->dev_addr[ETH_ALEN-1]++; /* \0SNUL1 */

	pr_debug("======== enable napi = %px, napi = %px \n",
		 dev, &ldd_dev->napi);
	if (use_napi)
		napi_enable(&ldd_dev->napi);

	ldd_dev->rx_int_enabled = true;
	netif_start_queue(dev);

	return 0;
}

static
int ldd_netdev_release(struct net_device *dev)
{
	struct ldd_dev *ldd_dev = netdev_priv(dev);

	pr_debug("========= release %s  ======= \n", dev->name);

	ldd_dev->rx_int_enabled = false;
	netif_stop_queue(dev);
	pr_debug("====== disable %px \n", &ldd_dev->napi);
	if (use_napi)
		napi_disable(&ldd_dev->napi);
	return 0;
}

static
int ldd_netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	pr_debug("ioctl\n");
	return 0;
}

static
struct ldd_packet *ldd_get_tx_buffer(struct net_device *dev)
{
	unsigned long flags;
	struct ldd_dev *ldd_dev = netdev_priv(dev);
	struct ldd_packet *pkt;
    
	spin_lock_irqsave(&ldd_dev->lock, flags);
	pkt = ldd_dev->pkt_pool;
	if(!pkt) {
		pr_debug("Out of Pool\n");
		return pkt;
	}
	ldd_dev->pkt_pool = pkt->next;
	if (ldd_dev->pkt_pool == NULL) {
		pr_info("Pool empty\n");
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
	return pkt;
}

static
int ldd_netdev_hw_tx(struct sk_buff *skb, struct net_device *dev)
{
	int rv, len;
	struct iphdr *iphdr;
	u32 *saddr, *daddr;
	struct ldd_dev *dest_ldd_dev, *src_ldd_dev;
	struct net_device *dest_dev;
	struct ldd_packet *tx_buffer;

	pr_debug("======= New tx ==========\n");


	// print_hex_dump(KERN_DEBUG, "skb tx raw: ", DUMP_PREFIX_OFFSET,
		       // 16, 1, skb->data, skb->len, true);

	iphdr = (struct iphdr*)skb_network_header(skb);
	saddr = &iphdr->saddr;
	daddr = &iphdr->daddr;

	pr_debug("before saddr: 0x%x, daddr: 0x%x\n", *saddr, *daddr);
	((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
	((u8 *)daddr)[2] ^= 1;
	pr_debug("after saddr: 0x%x, daddr: 0x%x\n", *saddr, *daddr);

	if (skb->len < ETH_ZLEN) {
		rv = skb_padto(skb, ETH_ZLEN);
		if (rv) {
			pr_err("Pad failed\n");
			return rv;
		}
		len = ETH_ZLEN;
	} else {
		len = skb->len;
	}

	ip_send_check(iphdr);

	/*
	 * Ok, now the packet is ready for transmission: first simulate a
	 * receive interrupt on the twin device, then  a
	 * transmission-done on the transmitting device
	 */
	dest_dev = net_devs[dev == net_devs[0] ? 1 : 0];
	dest_ldd_dev = netdev_priv(dest_dev);

	tx_buffer = ldd_get_tx_buffer(dev);
	if(!tx_buffer) {
		pr_debug("Out of tx buffer, len is %i\n",len);
		return -ENOMEM;
	}

	tx_buffer->datalen = len;
	memcpy(tx_buffer->data, skb->data, len);
	pr_debug("datato %s -> %s\n", dev->name, dest_dev->name);

	ldd_enqueue_buf(dest_dev, tx_buffer);
	if (dest_ldd_dev->rx_int_enabled) {
		dest_ldd_dev->status |= NETDEV_RX_INTR;
		ldd_interrupt(0, dest_dev, NULL);
	}


	src_ldd_dev = netdev_priv(dev);
	src_ldd_dev->tx_packetlen = len;
	src_ldd_dev->tx_packetdata = skb->data;
	src_ldd_dev->status |= NETDEV_TX_INTR;
#if 0
	if (lockup && ((src_ldd_dev->stats.tx_packets + 1) % lockup) == 0) {
        	/* Simulate a dropped transmit interrupt */
		netif_stop_queue(dev);
		PDEBUG("Simulate lockup at %ld, txp %ld\n", jiffies,
				(unsigned long) src_ldd_dev->stats.tx_packets);
	}
	else
		ldd_interrupt(0, dev, NULL);
#endif
	ldd_interrupt(0, dev, NULL);

	return 0;
}

static
int ldd_netdev_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct ldd_dev *ldd_dev = netdev_priv(dev);

	netif_trans_update(dev);

	/* Remember the skb, so we can free it at interrupt time */
	ldd_dev->skb = skb;

	/* actual deliver of data is device-specific, and not shown here */
	return ldd_netdev_hw_tx(skb, dev);
}

static
struct net_device_stats *ldd_netdev_stats(struct net_device *dev)
{
	struct ldd_dev *ldd_dev = netdev_priv(dev);
	return &ldd_dev->stats;
}

int ldd_netdev_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	eth->h_proto = htons(type);
	pr_debug("in-> saddr = %llx, daddr = %llx\n",
		 saddr ? *((u64*)saddr) : 0,
		 daddr ? *((u64*)daddr) : 0);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN - 1]   ^= 0x01;
	return (dev->hard_header_len);
}

static const struct header_ops ldd_header_ops = {
	.create  = ldd_netdev_header,
};

static const struct net_device_ops ldd_netdev_ops = {
	.ndo_open            = ldd_netdev_open,
	.ndo_stop            = ldd_netdev_release,
	.ndo_start_xmit      = ldd_netdev_tx,
	.ndo_do_ioctl        = ldd_netdev_ioctl,
	// .ndo_set_config      = snull_config,
	.ndo_get_stats       = ldd_netdev_stats,
	// .ndo_change_mtu      = snull_change_mtu,
	// .ndo_tx_timeout      = snull_tx_timeout,
};

static
int ldd_setup_pool(struct net_device *dev)
{
	int i;
	struct ldd_dev *ldd_dev = netdev_priv(dev);
	struct ldd_packet *pkt, *next;

	pr_debug("pool_size = %d\n", pool_size);
	ldd_dev->pkt_arr = kcalloc(pool_size, sizeof(struct ldd_packet), GFP_KERNEL);
	if (!ldd_dev->pkt_arr) {
		pr_err("Alloc memeory for pool failed\n");
		return -ENOMEM;
	}

	next = NULL;
	for (i = pool_size - 1; i >= 0; i--) {
		pkt = ldd_dev->pkt_arr + i;
		pkt->next = next;
		pkt->dev = dev;
		next = pkt;
	}

	ldd_dev->pkt_pool = ldd_dev->pkt_arr;

	return 0;
}

static
void ldd_teardown_pool(struct net_device *dev)
{
	struct ldd_dev *ldd_dev = netdev_priv(dev);

	kfree(ldd_dev->pkt_arr);
}    


static
void ldd_dev_init(struct net_device *dev)
{
	struct ldd_dev *ldd_dev;

	ether_setup(dev);
	dev->watchdog_timeo = timeout;
	dev->netdev_ops = &ldd_netdev_ops;
	dev->header_ops = &ldd_header_ops;
	dev->flags           |= IFF_NOARP;
	dev->features        |= NETIF_F_HW_CSUM;

	ldd_dev = netdev_priv(dev);
	memset(ldd_dev, 0, sizeof(struct ldd_dev));

	ldd_dev->net_dev = dev;
	spin_lock_init(&ldd_dev->lock);
	ldd_dev->rx_int_enabled = false;
	if (use_napi) {
		netif_napi_add(dev, &ldd_dev->napi, ldd_dev_poll, 2);
	}

}

static
void ldd_cleanup(void)
{
	int i;
	for (i = 0; i < 2; i++) {
		if (net_devs[i]) {
			unregister_netdev(net_devs[i]);
			ldd_teardown_pool(net_devs[i]);
			free_netdev(net_devs[i]);
		}
	}
}

static
int __init m_init(void)
{
	int i, rv;
	struct ldd_dev *ldd_dev;

	ldd_interrupt = use_napi ? ldd_napi_interrupt : ldd_regular_interrupt;

	ldd_dev = kzalloc(sizeof(struct ldd_dev), GFP_KERNEL);
	if (!ldd_dev)
		return -ENOMEM;

	net_devs[0] = alloc_netdev(sizeof(struct ldd_dev), "sn%d",
				   NET_NAME_UNKNOWN, ldd_dev_init);
	net_devs[1] = alloc_netdev(sizeof(struct ldd_dev), "sn%d",
				   NET_NAME_UNKNOWN, ldd_dev_init);
	if (net_devs[0] == NULL || net_devs[1] == NULL) {
		rv = -ENOMEM;
		goto out;
	}

	pr_debug("dev[0] = %px, dev[1] = %px\n", net_devs[0], net_devs[1]);
	ldd_dev = netdev_priv(net_devs[0]);
	pr_debug("0 napi = %px\n", &ldd_dev->napi);
	ldd_dev = netdev_priv(net_devs[1]);
	pr_debug("1 napi = %px\n", &ldd_dev->napi);

	for (i = 0; i < 2;  i++) {
		rv = ldd_setup_pool(net_devs[i]);
		if (rv) {
			pr_err("Setup pool failed for device %d\n", i);
			goto out;
		}

		rv = register_netdev(net_devs[i]);
		if (rv) {
			pr_err("registering device %s failed\n",
			       net_devs[i]->name);
			rv = -ENODEV;
			goto out;
		}

	}

#if 0
	for (i = 0; i < 2;  i++) {
		ldd_dev = netdev_priv(net_devs[i]);
		pr_debug("====== enable napi dev = %px, napi = %px\n",
			 net_devs[i], &ldd_dev->napi);
		if (use_napi)
			napi_enable(&ldd_dev->napi);
	}
#endif


	return 0;
out:
	ldd_cleanup();
	return rv;
}

static
void __exit m_exit(void)
{
	ldd_cleanup();
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("PCI Driver skel");
