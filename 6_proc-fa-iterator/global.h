#ifndef _GLOBAL_H
#define _GLOBAL_H

#undef PDEBUG

#ifdef ENABLE_DEBUG
#    ifdef __KERNEL__
#        define PDEBUG(fmt, args...)					\
		printk(KERN_DEBUG "proc-fs-iterator: " fmt, ##args)
#    else
#        define PDEBUG(fmt, arg...)					\
		fprintf(stderr, "proc-fs-iterator: " fmt, ##args)
#    endif
#else
#    define PDEBUG(fmt, args...)
#endif

#define MODULE_NAME "proc-fs-iterator"
#define PROC_FILE_NAME "proc-fs-iterator"

struct store_list_head_t {
	atomic_t n;
	struct list_head list;
};

struct store_node {
	struct list_head list;
	char *buf;
};

extern struct store_list_head_t store_list_head;

#endif
