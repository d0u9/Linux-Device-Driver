/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/atomic.h>
#include <linux/pci.h>

#define MODULE_NAME		"msi_interrupt"

/* QEMU's edu device, see docs/specs/edu.rst in the QEMU source */
#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

#define EDU_REG_IRQ_STATUS	0x24	/* read: pending interrupt bits */
#define EDU_REG_IRQ_RAISE	0x60	/* write: status |= value, interrupt */
#define EDU_REG_IRQ_ACK		0x64	/* write: status &= ~value */

struct irq_dev {
	struct pci_dev *pdev;
	void __iomem *bar0;
	int irq;
	atomic_t count;			/* interrupts handled */
	u32 last_status;		/* status seen by the last one */
};

#endif
