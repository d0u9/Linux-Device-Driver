#ifndef _GLOBAL_H
#define _GLOBAL_H

#undef PDEBUG
#ifdef ENABLE_DEBUG
#	ifdef __KERNEL__
#		define PDEBUG(fmt, args...) printk(KERN_DEBUG "pipe: " fmt, ##args)
#	else
#		define PDEBUG(fmt, args...) fprintf(stderr, "pipe: " fmt, ##args)
#	endif
#else
#	define PDEBUG(fmt, args...)
#endif

#define BUFF_SIZE	12
#define PIPE_DEV_NR	3

struct buff_t {
	int	read_pos;
	int	write_pos;
	int	size;
	struct mutex mutex;
	char	buff[BUFF_SIZE];
	wait_queue_head_t read_queue;
	wait_queue_head_t write_queue;
	struct cdev dev;
};

#endif
