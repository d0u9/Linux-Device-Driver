#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "ioc_cmd.h"

int main(void)
{
	int i, err, fd;
	void *map_addr;
	uint32_t *val;
	unsigned long mplen = 4096 * 6;
	struct ioc_msg msg;

	map_addr = mmap(NULL, mplen, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (map_addr == ((void*)-1)) {
		printf("map1 fail: %s\n", strerror(errno));
		return -1;
	}
	printf("mmap done: %p\n", map_addr);


	// if (munmap(map_addr, 4096) < 0) {
		// printf("unmap1 fail: %s\n", strerror(errno));
		// return -1;
	// }

	msg.addr = (unsigned long)map_addr;
	msg.len  = mplen;

	fd = open("/dev/zero_copy0", O_RDWR);
	if (fd < 0) {
		printf("open fail: %s\n", strerror(errno));
		return -1;
	}
	printf("Open done!\n");

	err = ioctl(fd, IOCTL_OPEN, &msg);
	if (err < 0) {
		printf("ioctl open fail: %s\n", strerror(errno));
		return -1;
	}

	unsigned long intval = 0xdeedbbbf;
	err = ioctl(fd, IOCTL_GET, intval);
	if (err < 0) {
		printf("ioctl get fail: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < (mplen / 4096); i++) {
		val = (uint32_t*)(map_addr + (i * 4096));
		printf("%#x\n", *val);
		*val = 0xafafafaf;
	}

	err = ioctl(fd, IOCTL_PUT, NULL);
	if (err < 0) {
		printf("ioctl put fail: %s\n", strerror(errno));
		return -1;
	}

	close(fd);


	return 0;
}

