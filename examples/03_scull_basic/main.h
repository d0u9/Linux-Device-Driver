/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mutex.h>

#define MODULE_NAME		"scull"
#define SCULL_NR_DEVS		3
#define SCULL_BLOCK_SIZE	PAGE_SIZE	/* one page per block */

struct scull_block {
	size_t offset;			/* bytes written into data[] */
	char data[SCULL_BLOCK_SIZE];
	struct list_head block_list;
};

struct scull_dev {
	int block_counter;		/* number of blocks in block_list */
	struct mutex mutex;
	struct cdev cdev;
	struct list_head block_list;	/* list of storage blocks */
};

#endif
