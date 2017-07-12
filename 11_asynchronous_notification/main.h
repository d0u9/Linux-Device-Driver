#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"async_notify"
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

#ifndef ASYNC_NOTIFY_DEV_NR
#define ASYNC_NOTIFY_DEV_NR	1
#endif

#ifndef BUFF_SIZE
#define BUFF_SIZE		PAGE_SIZE
#endif

struct async_notify_dev {
	struct cdev cdev;
	struct mutex mutex;
	struct fasync_struct *async_queue;
	loff_t buf_len;
	char buff[BUFF_SIZE];
};

#endif
