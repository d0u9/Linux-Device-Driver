/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <asm/page.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>

#define MODULE_NAME		"poll"
#define POLL_DEV_NR		1
#define BUFF_SIZE		PAGE_SIZE
#define TIMER_INTERVAL		HZ
#define DFT_MSG			"Hello World!"

struct poll_dev {
	struct cdev cdev;
	struct mutex mutex;		/* protects buff and buf_len */
	struct timer_list timer;
	unsigned long timer_counter;	/* only touched by the timer */
	atomic_t can_wr;		/* set by the timer, taken by write() */
	atomic_t can_rd;		/* set by the timer, taken by read() */
	wait_queue_head_t inq;
	wait_queue_head_t outq;
	size_t buf_len;
	char buff[BUFF_SIZE];
};

#endif
