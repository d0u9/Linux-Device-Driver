#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "ioctl_cmd.h"

#define DEV_FILE		"/dev/dma"
#define TEST_MSG		"hello world!"


int main(int argc, char *argv[])
{
	int fd, err;
	char *msg = TEST_MSG;

	struct ioc_arg ioc_arg = {
		.buf = msg,
		.len = sizeof(TEST_MSG),
	};

	fd = open(DEV_FILE, O_RDONLY);
	err = ioctl(fd, IOC_DMA_TO_DEV, &ioc_arg);
	printf("retval = %d\n", err);
	sleep(1);
	err = ioctl(fd, IOC_DMA_TO_RAM, &ioc_arg);
	printf("retval = %d\n", err);

	return 0;	
}
