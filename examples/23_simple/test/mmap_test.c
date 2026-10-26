// SPDX-License-Identifier: GPL-2.0
/*
 * mmap_test DEVICE: map part of /dev/simple0 or /dev/simple1 and check
 * which pages show up, that two shared mappings see the same memory, and
 * that a range past the buffer is refused.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIMPLE_PAGES	8	/* as in main.h */

static uint32_t *map(int fd, int offset, int pages)
{
	long page = sysconf(_SC_PAGESIZE);
	void *p = mmap(NULL, pages * page, PROT_READ | PROT_WRITE,
		       MAP_SHARED, fd, offset * page);

	return p == MAP_FAILED ? NULL : p;
}

int main(int argc, char *argv[])
{
	long words = sysconf(_SC_PAGESIZE) / 4;
	uint32_t *a, *b, saved;
	int fd, i;

	if (argc != 2) {
		fprintf(stderr, "usage: %s /dev/simpleN\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}

	/* pages 2 to 5: the first word of each is 0xdead0000 + its index */
	a = map(fd, 2, 4);
	if (!a) {
		perror("mmap");
		return 1;
	}
	for (i = 0; i < 4; i++)
		printf("page %d: %#x\n", 2 + i, a[i * words]);

	/* a second shared mapping of the same pages sees writes to the first */
	b = map(fd, 2, 4);
	if (!b) {
		perror("mmap");
		return 1;
	}
	saved = a[0];
	a[0] = 0x12345678;
	printf("shared: wrote %#x, other mapping reads %#x\n", a[0], b[0]);
	a[0] = saved;

	/* pages 6 to 9: past the buffer */
	if (map(fd, 6, 4))
		printf("past the end: mapped, but should fail\n");
	else
		printf("past the end: %s\n", strerror(errno));

	munmap(a, 4 * words * 4);
	munmap(b, 4 * words * 4);
	close(fd);
	return 0;
}
