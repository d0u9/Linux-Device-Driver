/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/bits.h>

#define MODULE_NAME		"short_port_remap"

/* ldd-io registers, see tools/qemu/ldd-io.md */
#define LDD_IO_DATA		0	/* write: LEDs, read: received byte */
#define LDD_IO_STATUS		1	/* write 1 to clear */
#define LDD_IO_CTRL		2
#define LDD_IO_MATRIX		8	/* 8 rows, one byte each */
#define LDD_IO_MATRIX_ROWS	8
#define LDD_IO_SIZE		16

#define LDD_IO_STATUS_RX_READY	BIT(0)

/* minor numbers */
enum short_minor {
	SHORT_DATA,		/* /dev/short_port_remap_data */
	SHORT_MATRIX,		/* /dev/short_port_remap_matrix */
	SHORT_NR_MINORS,
};

#define SHORT_MAX_WRITE		256	/* longer writes to DATA are short */
#define SHORT_POLL_US		10000	/* receive: read STATUS <= 10 ms apart */
#define SHORT_POLL_SLICE_US	100000	/* and look for signals every 100 ms */

#endif
