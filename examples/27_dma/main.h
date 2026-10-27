/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>

#define MODULE_NAME		"dma"

/* QEMU's edu device, see docs/specs/edu.rst in the QEMU source */
#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

#define EDU_REG_IRQ_STATUS	0x24
#define EDU_REG_IRQ_ACK		0x64
#define EDU_REG_DMA_SRC		0x80	/* 64-bit bus address */
#define EDU_REG_DMA_DST		0x88	/* 64-bit bus address */
#define EDU_REG_DMA_COUNT	0x90
#define EDU_REG_DMA_CMD		0x98

#define EDU_DMA_START		0x01
#define EDU_DMA_TO_RAM		0x02	/* else from RAM to edu */
#define EDU_DMA_IRQ		0x04	/* interrupt when done */
#define EDU_IRQ_DMA		0x100	/* interrupt status of a finished DMA */

#define EDU_BUF_ADDR		0x40000	/* edu's own buffer, in edu's space */
#define EDU_BUF_SIZE		4096
#define EDU_DMA_BITS		28	/* edu only drives 28 address bits */

struct dma_dev {
	struct pci_dev *pdev;
	void __iomem *bar0;
	int irq;

	void *buf;			/* DMA buffer, as the CPU sees it */
	dma_addr_t buf_bus;		/* the same, as the device sees it */
	struct completion dma_done;	/* completed by the DMA interrupt */

	struct mutex lock;		/* one transfer at a time */
	bool removed;			/* under lock: the device is gone */

	struct device dev;		/* /dev/dma; frees this at last put */
	struct cdev cdev;
};

#endif
