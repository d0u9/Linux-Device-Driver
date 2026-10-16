#ifndef _MAIN_H
#define _MAIN_H

#include <linux/list.h>			/* double linked list support */

#define MODULE_NAME		"scull"
#define SCULL_NR_DEVS		3
#define SCULL_BLOCK_SIZE	PAGE_SIZE		// one page per block

struct scull_block {
	loff_t offset;
	char data[SCULL_BLOCK_SIZE];
	struct list_head block_list;
};

struct scull_dev {
	int block_counter;		//record how many blocks now in the list
	struct mutex mutex;
	struct cdev cdev;
	struct list_head block_list;	//list of storage blocks
};

#endif
