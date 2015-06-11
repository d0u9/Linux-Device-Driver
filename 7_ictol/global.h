#ifndef _GLOBAL_H
#define _GLOBAL_H

#define MODULE_NAME "ioctl"

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
