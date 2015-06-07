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
#define SCULL_BUFF_SIZE	8
#endif

#ifndef MUDULE_NAME
#define MODULE_NAME	"scull"
#endif

struct store_block {
	struct list_head list;
	int pos;
	char data[SCULL_BUFF_SIZE];
};

struct scull_dev {
	atomic_t open_counter;
	struct list_head list;		//list of storage blocks
	int list_entry_counter;		//record how many blocks now in the list
	struct mutex mutex;
	struct cdev cdev;
};

#endif
