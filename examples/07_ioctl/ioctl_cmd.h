/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ioctl interface of the ioctl example, shared by the kernel module and the
 * userspace test program in test/.
 */
#ifndef _IOCTL_CMD_H
#define _IOCTL_CMD_H

#include <linux/ioctl.h>

/*
 * Magic numbers are listed in
 * Documentation/userspace-api/ioctl/ioctl-number.rst in the kernel source.
 * 'd' is already registered there (drm, pcmcia), which is fine for an
 * example but a real driver should pick an unused number and register it.
 */
#define IOCTL_IOC_MAGIC		'd'

/* Restore the default message and count. No argument. */
#define IOCTL_RESET		_IO(IOCTL_IOC_MAGIC, 0)
/*
 * Set how many times the message is printed. The count is passed by value
 * in the ioctl argument, not through a pointer, hence _IO. The previous count
 * is the return value.
 */
#define IOCTL_HOWMANY		_IO(IOCTL_IOC_MAGIC, 1)
/* Set the message. The argument points to a struct ioctl_msg_arg. */
#define IOCTL_MESSAGE		_IOW(IOCTL_IOC_MAGIC, 2, struct ioctl_msg_arg)

#define IOCTL_MAXNR		2

struct ioctl_msg_arg {
	int len;
	char *msg;
};

#endif
