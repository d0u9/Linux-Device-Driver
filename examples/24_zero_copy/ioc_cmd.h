#ifndef _IOC_CMD_H
#define _IOC_CMD_H

#define IOCTL_IOC_MAGIC		'd'

#define IOCTL_OPEN		_IO(IOCTL_IOC_MAGIC, 0)
#define IOCTL_GET		_IOWR(IOCTL_IOC_MAGIC, 1, int)
#define IOCTL_PUT		_IOW(IOCTL_IOC_MAGIC, 2, int)

struct ioc_msg {
	unsigned long addr;
	unsigned long len;
};

#endif
