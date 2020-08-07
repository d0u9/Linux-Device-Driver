#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME	"jiq"
#define BUF_LEN		PAGE_SIZE

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


