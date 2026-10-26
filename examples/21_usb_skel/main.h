/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/atomic.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/wait.h>

#define MODULE_NAME		"usb_skel"

/* ldd-usb, see tools/qemu/ldd-usb.md */
#define LDD_USB_VENDOR_ID	0x1234
#define LDD_USB_PRODUCT_ID	0x7863
#define LDD_USB_TICK_LEN	8	/* { le32 1, le32 interval } */

#define SKEL_MINOR_BASE		192
#define SKEL_TIMEOUT_MS		1000	/* for the bulk IN read */

/* One per plugged-in device, shared by every open file. */
struct usb_skel {
	struct usb_device	*udev;
	struct usb_interface	*interface;

	/* bulk IN: time of the last tick */
	u8			bulk_in_addr;
	size_t			bulk_in_size;
	u8			*bulk_in_buffer;

	/* bulk OUT: interval */
	u8			bulk_out_addr;

	/* interrupt IN: one report per tick */
	struct urb		*tick_urb;
	__le32			*tick_buffer;
	atomic_t		ticks;		/* reports received */
	wait_queue_head_t	tick_wait;

	struct mutex		io_mutex;	/* I/O against disconnect */
	bool			disconnected;	/* under io_mutex */
	int			open_count;	/* under io_mutex */

	struct kref		kref;
};

/* One per open file. */
struct skel_file {
	struct usb_skel		*dev;
	int			seen;		/* ticks already read */
};

#endif
