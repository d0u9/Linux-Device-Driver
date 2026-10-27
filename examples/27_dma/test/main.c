// SPDX-License-Identifier: GPL-2.0
/*
 * Write to edu's buffer through /dev/dma and read it back. Each read and
 * write is a DMA transfer that edu runs about 100 ms after it is started.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEV_FILE	"/dev/dma"
#define EDU_BUF_SIZE	4096
#define MSG		"hello dma"

static int fails;

static void check(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		fails++;
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(void)
{
	char buf[64], line[80];
	long t0, t;
	int fd;

	fd = open(DEV_FILE, O_RDWR);
	if (fd < 0) {
		perror(DEV_FILE);
		return 1;
	}

	t0 = now_ms();
	check(pwrite(fd, MSG, strlen(MSG), 0) == strlen(MSG),
	      "write \"" MSG "\" to edu at offset 0");
	t = now_ms() - t0;
	snprintf(line, sizeof(line), "write waited for the DMA: %ld ms", t);
	check(t >= 90, line);

	memset(buf, 0, sizeof(buf));
	t0 = now_ms();
	check(pread(fd, buf, strlen(MSG), 0) == strlen(MSG) &&
	      !strcmp(buf, MSG), "read it back from edu");
	t = now_ms() - t0;
	snprintf(line, sizeof(line), "read waited for the DMA: %ld ms", t);
	check(t >= 90, line);

	check(pwrite(fd, MSG, strlen(MSG), EDU_BUF_SIZE - 4) == 4,
	      "write at the end stops at 4096 bytes");
	check(pread(fd, buf, sizeof(buf), EDU_BUF_SIZE) == 0,
	      "read past the end returns 0");
	check(pwrite(fd, MSG, strlen(MSG), EDU_BUF_SIZE) < 0 &&
	      errno == ENOSPC, "write past the end fails with ENOSPC");

	close(fd);
	printf("%s\n", fails ? "FAILED" : "PASSED");
	return fails != 0;
}
