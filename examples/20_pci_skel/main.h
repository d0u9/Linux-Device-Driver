/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"pci_skel"

/* The ldd-pcie device, see tools/qemu/ldd-pcie.md */
#define LDD_PCIE_VENDOR_ID	0x1234
#define LDD_PCIE_DEVICE_ID	0xcafe

/* BAR 0 registers, 32-bit accesses only */
#define LDD_REG_ID		0x00	/* always LDD_ID */
#define LDD_REG_VERSION		0x04	/* major in 31:16, minor in 15:0 */
#define LDD_REG_CAPS		0x08
#define LDD_REG_SCRATCH		0x0c	/* reads back what was written */

#define LDD_ID			0x4c444450	/* "LDDP" */
#define LDD_CAPS_DMA64		BIT(0)

/* BAR 2 is 1 MiB of on-board memory */
#define LDD_MEM_BAR		2

/* what probe sets up and remove undoes, one per device */
struct skel_dev {
	void __iomem *regs;	/* BAR 0 */
	void __iomem *mem;	/* BAR 2 */
};

#endif
