#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"poll"
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

#ifndef POLL_DEV_NR
#define POLL_DEV_NR		1
#endif

#ifndef BUFF_SIZE
#define BUFF_SIZE		PAGE_SIZE
#endif

#ifndef TIMER_INTERVAL
#define TIMER_INTERVAL		HZ
#endif

#ifndef DFT_MSG
#define DFT_MSG			"Hello World!"
#endif

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
