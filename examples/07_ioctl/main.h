/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#define MODULE_NAME		"ioctl"

#define IOCTL_DEV_NR		1

#define DEFAULT_HOWMANY		3
#define DEFAULT_MESSAGE		"Hello ioctl()!\n"
#define BUFF_SIZE		PAGE_SIZE

struct ioctl_dev {
	struct cdev cdev;
	struct mutex lock;		/* protects the fields below */
	unsigned int howmany;
	size_t buf_len;
	char buff[BUFF_SIZE];
};

void ioctl_dev_set_default(struct ioctl_dev *dev);

#endif
