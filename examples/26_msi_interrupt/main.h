/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/atomic.h>
#include <linux/bits.h>
#include <linux/pci.h>

#define MODULE_NAME		"msi_interrupt"

/* The ldd-pcie device, see tools/qemu/ldd-pcie.md */
#define LDD_PCIE_VENDOR_ID	0x1234
#define LDD_PCIE_DEVICE_ID	0xcafe

/* BAR 0 registers, 32-bit accesses only */
#define LDD_REG_ID		0x00
#define LDD_REG_INT_STATUS	0x18	/* pending events; write 1 to clear */
#define LDD_REG_INT_MASK	0x1c	/* 1: the event interrupts */
#define LDD_REG_INT_RAISE	0x20	/* write 1: the event happens */

#define LDD_ID			0x4c444450	/* "LDDP" */

/* the events, the same bits in all three registers; MSI-X vector n is bit n */
#define LDD_INT_DMA_DONE	BIT(0)
#define LDD_INT_DMA_ERROR	BIT(1)
#define LDD_INT_SOFT		BIT(2)
#define LDD_INT_ALL		GENMASK(2, 0)
#define LDD_NR_EVENTS		3

struct msi_dev;

/* one per vector in use, passed to its handler as dev_id */
struct msi_vec {
	struct msi_dev *msi;
	int irq;		/* Linux IRQ number */
	u32 bit;		/* MSI-X: the one event of this vector */
	char name[32];		/* shown in /proc/interrupts */
};

struct msi_dev {
	struct pci_dev *pdev;
	void __iomem *regs;
	int nvec;			/* vectors in use: 3 with MSI-X, else 1 */
	struct msi_vec vec[LDD_NR_EVENTS];
	atomic_t count[LDD_NR_EVENTS];	/* events handled, per event */
	u32 last_status;		/* events seen by the last interrupt */
};

#endif
