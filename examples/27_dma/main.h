/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/bits.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/sizes.h>
#include <linux/types.h>

#define MODULE_NAME		"dma"

/* The ldd-pcie device, see tools/qemu/ldd-pcie.md */
#define LDD_PCIE_VENDOR_ID	0x1234
#define LDD_PCIE_DEVICE_ID	0xcafe

/* BAR 0 registers, 32-bit accesses only */
#define LDD_REG_ID		0x00
#define LDD_REG_CAPS		0x08
#define LDD_REG_CTRL		0x10
#define LDD_REG_STATUS		0x14
#define LDD_REG_INT_STATUS	0x18	/* write 1 to clear */
#define LDD_REG_INT_MASK	0x1c
#define LDD_REG_MAX_SEG		0x24	/* largest descriptor length */
#define LDD_REG_DESC_ADDR_LO	0x40	/* bus address of the descriptors */
#define LDD_REG_DESC_ADDR_HI	0x44
#define LDD_REG_DESC_COUNT	0x48
#define LDD_REG_DEV_OFFSET	0x4c	/* where in the on-board memory */
#define LDD_REG_DMA_CTRL	0x50
#define LDD_REG_XOR		0x5c	/* XORed into data sent to RAM */

#define LDD_ID			0x4c444450	/* "LDDP" */
#define LDD_CAPS_DMA64		BIT(0)		/* else 32 address bits */
#define LDD_CTRL_RESET		BIT(0)
#define LDD_STATUS_ERR_CODE	GENMASK(15, 8)
#define LDD_INT_DMA_DONE	BIT(0)
#define LDD_INT_DMA_ERROR	BIT(1)
#define LDD_DMA_CTRL_TO_RAM	BIT(0)		/* else RAM to device */
#define LDD_DMA_CTRL_START	BIT(1)

#define LDD_MEM_SIZE		SZ_1M		/* on-board memory, /dev/dma */

/* largest transfer, the size of the data buffer; MAX_SEG may lower it */
#define DMA_CHUNK		SZ_64K

/*
 * A descriptor, read by the device from RAM: little endian whatever the
 * CPU, so the fields are __le and filled with cpu_to_le*().
 */
struct ldd_desc {
	__le64 addr;		/* bus address of the data */
	__le32 len;
	__le32 flags;		/* must be 0 */
};

struct dma_dev {
	struct pci_dev *pdev;
	void __iomem *regs;
	int irq;

	struct ldd_desc *desc;		/* coherent: the descriptor table */
	dma_addr_t desc_bus;		/* the same, as the device sees it */
	void *buf;			/* streaming: data, mapped per transfer */
	size_t chunk;			/* size of buf */

	struct completion dma_done;	/* completed by the DMA interrupt */
	u32 int_status;			/* what the interrupt reported */

	struct mutex lock;		/* one transfer at a time */
	bool removed;			/* under lock: the device is gone */

	struct device dev;		/* /dev/dma; frees this at last put */
	struct cdev cdev;
};

#endif
