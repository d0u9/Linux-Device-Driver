#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#undef PDEBUG
#ifdef ENABLE_DEBUG
#    ifdef __KERNEL__
#	define PDEBUG(format, args...) printk(PRINTK_LEVEL "scull: " format, ##args)
#    else
#	define PDEBUG(format, args...) fprintf(stderr, fmt, ##args)
#    endif
#else
#    define PDEBUG(format, args...)
#endif

#ifndef SCULL_MAJOR
#define SCULL_MAJOR	0
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS	4
#endif

#ifndef SCULL_BUFF_SIZE
#define SCULL_BUFF_SIZE	4096
#endif

struct store_block {
	struct list_head list;
	char data[SCULL_BUFF_SIZE];
};

struct scull_dev {
	struct list_head list;		//list of storage blocks
	atomic_t list_entry_counter;		//record how many blocks now in the list
	atomic_t open_counter;
	struct mutex mutex;
	struct cdev cdev;
};

#endif
