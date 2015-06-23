#ifndef _GLOBAL_H
#define _GLOBAL_H

#undef PDEBUG

#ifdef ENABLE_DEBUG
#	ifdef __KERNEL__
#		define PDEBUG(fmt, args...) printk(KERN_DEBUG "pipe: " fmt, ##args)
#	else
#		define PDEBUG(fmt, args...) fprintf(stdout, "pipe: " fmt, ##args)
#	endif
#else
#	define PDEBUG(fmt, args...)
#endif

#define MAX_DEV_NR	3

#define MAX_BUFF_SIZE	12

struct pipe_t {
	int read_pos;
	int write_pos;
	int valid_elements;
	char buff[MAX_BUFF_SIZE];
	wait_queue_head_t read_q;
	wait_queue_head_t write_q;
	struct mutex mutex;
	struct cdev dev;
};

#endif
