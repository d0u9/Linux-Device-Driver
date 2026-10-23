/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <asm/page.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define MODULE_NAME	"jiq"
#define BUF_LEN		PAGE_SIZE
#define LINE_MAX_LEN	80	/* stop when less than one line is left */

struct jiq_dev {
	struct mutex lock;		/* one reader at a time */
	wait_queue_head_t jiq_wait;
	struct work_struct jiq_work;
	struct delayed_work jiq_work_delayed;
	struct timer_list timer;
	struct work_struct bh_work;	/* was a tasklet */
	int done;			/* set when the buffer is complete */
	size_t len;
	char buf[BUF_LEN];
	unsigned long jiffies;
	unsigned long delay;
};

#endif
