#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"zero_copy"
#define DEV_NR			1

struct zero_copy_dev {
	struct cdev cdev;
};

struct zero_copy_ctx {
	int pagenr;
	struct page **pages;
	unsigned long addr;
	wait_queue_head_t get_queue;
	wait_queue_head_t put_queue;
};

#endif
