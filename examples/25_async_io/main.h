/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/mutex.h>

#define MODULE_NAME		"async_io"
#define DEV_NR			1
#define BUFF_SIZE		((int)PAGE_SIZE)
#define ASYNC_DELAY_MS		100	/* how late an async request completes */

/* A ring buffer: what is written can be read back once. */
struct async_io_dev {
	struct cdev cdev;
	struct mutex mutex;		/* protects the fields below */
	int len;			/* bytes in the buffer */
	int read_cur;
	int write_cur;
	char buff[BUFF_SIZE];
};

#endif
