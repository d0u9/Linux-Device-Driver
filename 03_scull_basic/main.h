#ifndef _MAIN_H
#define _MAIN_H

#include <linux/list.h>			/* double linked list support */

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"scull"
#endif

#undef PDEBUG
#ifdef ENABLE_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(format, args...) printk(PRINTK_LEVEL MODULE_NAME ": " format, ##args)
#  else
#    define PDEBUG(format, args...) fprintf(stderr, MODULE_NAME ": " format, ##args)
#  endif
#else
#  define PDEBUG(format, args...)
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS		3
#endif

#ifndef SCULL_BLOCK_SIZE
#define SCULL_BLOCK_SIZE	PAGE_SIZE		// one page per block
#endif

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
