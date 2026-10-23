/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#define MODULE_NAME		"async_notify"
#define ASYNC_NOTIFY_DEV_NR	1
#define BUFF_SIZE		PAGE_SIZE

struct async_notify_dev {
	struct cdev cdev;
	struct mutex mutex;		/* protects buff and buf_len */
	struct fasync_struct *async_queue;
	size_t buf_len;
	char buff[BUFF_SIZE];
};

#endif
