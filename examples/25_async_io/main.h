#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"async_io"
#define DEV_NR	1
#define BUFF_SIZE		((int)PAGE_SIZE)

struct async_io_dev {
	struct cdev cdev;
	struct mutex mutex;
	int len;
	int read_cur;
	int write_cur;
	char buff[PAGE_SIZE];
};


#endif
