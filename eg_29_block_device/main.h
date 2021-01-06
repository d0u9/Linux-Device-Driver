#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME			"memdisk"

struct ldd_dev {
	int major;	
	int size;
	u8 *data;
	short users;
	bool media_change;
	spinlock_t lock;
	struct blk_mq_tag_set tag_set;
	struct request_queue *queue;
	struct gendisk *gd;
	struct timer_list timer;
};

#endif
