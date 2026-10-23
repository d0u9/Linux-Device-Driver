/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#define MODULE_NAME		"pipe_advanced_sleep"
#define BUFF_SIZE		(1 << 3)	/* 8 bytes only */
#define PIPE_DEV_NR		4

struct pipe_dev {
	struct cdev cdev;
	struct mutex mutex;
	wait_queue_head_t rd_queue;
	wait_queue_head_t wr_queue;
	size_t buff_len;		/* bytes written into buff */
	size_t rd_pos;			/* bytes of buff already read */
	char buff[BUFF_SIZE];
};

#endif
