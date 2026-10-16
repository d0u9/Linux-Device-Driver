#ifndef _IOCTL_CMD_H
#define _IOCTL_CMD_H

#define IOC_MAGIC		'd'
#define IOC_DMA_TO_DEV		_IOW(IOC_MAGIC, 0, struct ioc_arg)
#define IOC_DMA_TO_RAM		_IOW(IOC_MAGIC, 1, struct ioc_arg)

struct ioc_arg {
	int len;
	char *buf;
};

#endif
