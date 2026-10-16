#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"async_notify"
#define ASYNC_NOTIFY_DEV_NR	1
#define BUFF_SIZE		PAGE_SIZE

struct async_notify_dev {
	struct cdev cdev;
	struct mutex mutex;
	struct fasync_struct *async_queue;
	loff_t buf_len;
	char buff[BUFF_SIZE];
};

#endif
