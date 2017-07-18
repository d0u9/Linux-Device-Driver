#ifndef _MAIN_H
#define _MAIN_H

#define PRINTK_LEVEL KERN_DEBUG

#ifndef MUDULE_NAME
#define MODULE_NAME	"jiq"
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

#ifndef BUF_LEN
#define BUF_LEN		PAGE_SIZE
#endif

struct jiq_dev {
	wait_queue_head_t jiq_wait;
	struct work_struct jiq_work;
	struct delayed_work jiq_work_delay;
	struct timer_list timer;
	struct tasklet_struct tlet;
	int   timeout;
	int   len;
	char  buf[BUF_LEN];
	unsigned long jiffies;
	long delay;
};

#endif


