#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"ioctl"

#define IOCTL_DEV_NR		1
#define IOCTL_IOC_MAGIC		'd'

#define IOCTL_RESET		_IO(IOCTL_IOC_MAGIC, 0)
#define IOCTL_HOWMANY		_IOWR(IOCTL_IOC_MAGIC, 1, int)
#define IOCTL_MESSAGE		_IOW(IOCTL_IOC_MAGIC, 2, int)

#define IOCTL_MAXNR		2

#define DEFAULT_HOWMANY		3
#define DEFAULT_MESSAGE		"Hello ioctl()!\n"
#define BUFF_SIZE		PAGE_SIZE

struct ioctl_dev {
	struct cdev cdev;
	int howmany;
	loff_t buf_len;
	char buff[BUFF_SIZE];
};

struct ioctl_msg_arg {
	int len;
	char *msg;
};

#endif
