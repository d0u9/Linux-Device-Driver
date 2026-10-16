#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

int main(void)
{
	int i, fd;
	uint32_t *val;
	void *map_addr1, *map_addr2;

	fd = open("/dev/simple1", O_RDWR);
	if (fd < 0) {
		printf("open fail: %s\n", strerror(errno));
		return -1;
	}
	printf("Open done!\n");

	map_addr1 = mmap(NULL, 4096 * 6, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 4096);
	if (map_addr1 == ((void*)-1)) {
		printf("map1 fail: %s\n", strerror(errno));
		return -1;
	}
	printf("mmap done: %p\n", map_addr1);

	val = (uint32_t*)(map_addr1);
	for (i = 0; i < 10; i++) {
		printf("%#x\n", *(val + (i * 1)));
	}

	// if (munmap(map_addr1, 4096) < 0) {
		// printf("unmap1 fail: %s\n", strerror(errno));
		// return -1;
	// }

	close(fd);

	return 0;
}

