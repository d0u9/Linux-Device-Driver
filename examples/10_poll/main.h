#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"poll"
#define POLL_DEV_NR		1
#define BUFF_SIZE		PAGE_SIZE
#define TIMER_INTERVAL		HZ
#define DFT_MSG			"Hello World!"

struct poll_dev {
	struct cdev cdev;
	struct mutex mutex;
	struct timer_list timer;
	unsigned long timer_counter;
	atomic_t can_wr;
	atomic_t can_rd;
	wait_queue_head_t inq;
	wait_queue_head_t outq;
	loff_t buf_len;
	char buff[BUFF_SIZE];
};

#endif
