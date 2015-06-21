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

#define BUFF_SIZE	512
#define PIPE_DEV_NR	3

struct buff_t {
	int	read_pos;
	int	write_pos;
	struct mutex mutex_r;
	struct mutex mutex_w;
	struct cdev dev;
	char	buff[BUFF_SIZE];
};


#endif
