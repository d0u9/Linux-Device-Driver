#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME	"proc_fs_basic"
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

#ifndef SUB_DIR_NAME
#define SUB_DIR_NAME		"proc_demo"
#endif

#ifndef PROC_FS_NAME
#define PROC_FS_NAME		"proc_fs"
#endif

#ifndef PROC_FS_NAME_MUL
#define PROC_FS_NAME_MUL	"proc_fs_mul"
#endif

#ifndef PRINT_NR
#define PRINT_NR		3
#endif

#endif
