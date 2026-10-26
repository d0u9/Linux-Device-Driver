/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/bits.h>

#define MODULE_NAME		"short_irq_probe"

/* ldd-io registers, see tools/qemu/ldd-io.md */
#define LDD_IO_DATA		0	/* write: LEDs, read: received byte */
#define LDD_IO_STATUS		1	/* write 1 to clear */
#define LDD_IO_CTRL		2
#define LDD_IO_SIZE		16

#define LDD_IO_STATUS_RX_READY	BIT(0)
#define LDD_IO_CTRL_IRQ_EN	BIT(0)
#define LDD_IO_CTRL_LOOPBACK	BIT(1)

#define SHORT_MAX_WRITE		256	/* longer writes are short */

#endif
