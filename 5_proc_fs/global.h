#ifndef _GLOBAL_H
#define _GLOBAL_H

#undef PDEBUG
#ifdef ENABLE_DEBUG
#    ifdef __KERNEL__
#        define PDEBUG(fmt, args...)					\
		printk(KERN_DEBUG "proc fs: " fmt, ##args)
#    else
#        define PDEBUG(fmt, args...)					\
		fprintf(stderr, "proc fs: " fmt, ##args);
#    endif
#else
#    define PDEBUG(fmt, args...)
#endif

#define SUB_DIR_NAME	"my_sub_dir"
#define PROC_FS_NAME	"proc-fs"

#endif
