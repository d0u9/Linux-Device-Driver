#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"short_ioport"
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

#define SHORT_NR_PORTS	8		/* use 8 ports by default */

struct short_dev {
	int block_counter;		//record how many blocks now in the list
	struct mutex mutex;
	struct cdev cdev;
	struct list_head block_list;	//list of storage blocks
};

#endif
