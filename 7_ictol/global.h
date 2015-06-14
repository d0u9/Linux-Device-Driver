#ifndef _GLOBAL_H
#define _GLOBAL_H

#define MODULE_NAME	"ioctl"
#define DEFAULT_STR	"Hello World!\n"
#define DEFAULT_STR_LEN	sizeof(DEFAULT_STR)
#define DEV_NR		1
#define BUFF_SIZE	4096

#define IOCTL_IOC_MAGIC	'd'

#define IOCTL_RESET	_IO(IOCTL_IOC_MAGIC, 0)
#define IOCTL_LOOPNR	_IOWR(IOCTL_IOC_MAGIC, 1, int)
#define IOCTL_STR	_IOWR(IOCTL_IOC_MAGIC, 2, int)

#define IOCTL_MAXNR	2

#undef PDEBUG
#ifdef ENABLE_DEBUG
#    ifdef __KERNEL__
#        define PDEBUG(fmt, args...)					\
		printk(KERN_DEBUG MODULE_NAME ": " fmt, ##args)
#    else
#        define PDEBUG(fmt, args...)					\
		fprintf(stderr, MODULE_NAME ": " fmt, ##args)
#    endif
#else
#    define PDEBUG(fmt, args...)
#endif

#endif
