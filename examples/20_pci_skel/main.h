/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"pci_skel"

/* QEMU's edu device, see docs/specs/edu.rst in the QEMU source */
#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

/* BAR 0 registers, 32-bit accesses only */
#define EDU_REG_ID		0x00	/* 0xRRrr00ed: major, minor version */
#define EDU_REG_LIVENESS	0x04	/* reads back the inverse of a write */

#endif
