#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"pipe_simple_sleep"
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

#ifndef BUFF_SIZE
#define BUFF_SIZE		(1 << 3)	// 8 bytes only
#endif

#ifndef PIPE_DEV_NR
#define PIPE_DEV_NR		4
#endif

struct pipe_dev {
	struct cdev cdev;
	struct mutex mutex;
	wait_queue_head_t rd_queue;
	wait_queue_head_t wr_queue;
	int  buff_len;
	char buff[BUFF_SIZE];
};

#endif
