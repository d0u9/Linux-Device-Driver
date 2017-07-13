#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME		"seeking"
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

#ifndef BUFF_SIZE
#define BUFF_SIZE		PAGE_SIZE
#endif

#ifndef SEEKING_DEV_NR
#define SEEKING_DEV_NR		1
#endif

#ifndef HEX_DICT
#define HEX_DICT		"0123456789ABCDEF"
#endif

#ifndef HEX_DICT_LEN
#define HEX_DICT_LEN		(ARRAY_SIZE(HEX_DICT) - 1)
#endif

struct seeking_dev {
	struct cdev cdev;
	struct mutex mutex;
	char buff[HEX_DICT_LEN];
};

#endif
