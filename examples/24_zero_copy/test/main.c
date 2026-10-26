// SPDX-License-Identifier: GPL-2.0
/*
 * Pin a buffer in /dev/zero_copy0, let the kernel write it and read it,
 * and check what each side sees.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ioc_cmd.h"

#define NPAGES	4

static int fails;

static void check(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		fails++;
}

int main(void)
{
	long page = sysconf(_SC_PAGESIZE), words = page / 4;
	uint32_t *buf, val = 0x5a5a5a5a, sum;
	struct zc_buf zb;
	int fd, i, same;

	fd = open("/dev/zero_copy0", O_RDWR);
	if (fd < 0) {
		perror("/dev/zero_copy0");
		return 1;
	}

	buf = mmap(NULL, NPAGES * page, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	zb.addr = (uintptr_t)buf + 1;
	zb.len = NPAGES * page;
	check(ioctl(fd, ZC_PIN, &zb) < 0 && errno == EINVAL,
	      "unaligned buffer refused");

	zb.addr = (uintptr_t)buf;
	check(ioctl(fd, ZC_PIN, &zb) == 0, "4 pages pinned");
	check(ioctl(fd, ZC_PIN, &zb) < 0 && errno == EBUSY,
	      "second pin refused");

	/* the kernel writes, user space reads */
	check(ioctl(fd, ZC_FILL, &val) == 0, "kernel filled the buffer");
	same = 1;
	for (i = 0; i < NPAGES * words; i++)
		same &= buf[i] == val;
	check(same, "user space sees 0x5a5a5a5a everywhere");

	/* user space writes, the kernel reads */
	for (i = 0; i < NPAGES; i++)
		buf[i * words] = i;
	check(ioctl(fd, ZC_SUM, &sum) == 0 && sum == 0 + 1 + 2 + 3,
	      "kernel read 0, 1, 2, 3 from the pages");

	check(ioctl(fd, ZC_UNPIN) == 0, "unpinned");
	check(ioctl(fd, ZC_FILL, &val) < 0 && errno == EINVAL,
	      "fill without a buffer refused");

	/* left pinned: close() unpins it */
	check(ioctl(fd, ZC_PIN, &zb) == 0, "pinned again, for close()");
	close(fd);
	munmap(buf, NPAGES * page);
	printf("%s\n", fails ? "FAILED" : "PASSED");
	return fails != 0;
}
