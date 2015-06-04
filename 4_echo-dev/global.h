#ifndef _GLOBAL_H
#define _GLOBAL_H

#undef PDEBUG
#ifdef ENABLE_DEBUG
#    ifdef __KERNEL__
#        define PDEBUG(fmt, args...) printk(KERN_DEBUG "echo-dev: " fmt, ##args)
#    else
#        define PDEBUG(fmt, args...) fprintf(stderr, "echo-dev: " fmt, ##args)
#    endif
#else
#    define PDEBUG(fmt, args...)
#endif

#define ECHO_NR_DEVS	4
#define ECHO_MAJOR	0
#define ECHO_MINOR	0
#define ECHO_BUF_SIZE	8

struct echo_dev {
	atomic_t opt_counter;
	int data_len;
	int data_max_len;
	char *data;
	struct mutex mutex;
	struct cdev cdev;
};

#endif
