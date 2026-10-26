/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _IOC_CMD_H
#define _IOC_CMD_H

/* Shared with user space: fixed-size types only. */
#include <linux/ioctl.h>
#include <linux/types.h>

/* A user buffer; address and length must be multiples of the page size. */
struct zc_buf {
	__u64 addr;
	__u64 len;
};

#define ZC_IOC_MAGIC	'z'

#define ZC_PIN		_IOW(ZC_IOC_MAGIC, 0, struct zc_buf)	/* pin it */
#define ZC_FILL		_IOW(ZC_IOC_MAGIC, 1, __u32)	/* kernel writes it */
#define ZC_SUM		_IOR(ZC_IOC_MAGIC, 2, __u32)	/* kernel reads it */
#define ZC_UNPIN	_IO(ZC_IOC_MAGIC, 3)		/* release it */

#endif
