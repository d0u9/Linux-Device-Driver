// SPDX-License-Identifier: GPL-2.0
/*
 * Write to the device's on-board memory through /dev/dma and read it back.
 * Each read and write is one DMA transfer of at most 64 KiB.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEV_FILE	"/dev/dma"
#define XOR_FILE	"/sys/module/dma/parameters/xor"
#define MEM_SIZE	(1024 * 1024)
#define CHUNK		(64 * 1024)
#define BIG		(200 * 1024)
#define MSG		"hello dma"

static int fails;

static void check(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		fails++;
}

static int set_xor(int val)
{
	FILE *f = fopen(XOR_FILE, "w");

	if (!f)
		return -1;
	fprintf(f, "%d\n", val);
	return fclose(f);
}

/* read or write all of @len, one transfer at a time */
static int xfer_all(int fd, char *buf, size_t len, off_t off, int wr)
{
	size_t done = 0;
	ssize_t n;
	int calls = 0;

	while (done < len) {
		n = wr ? pwrite(fd, buf + done, len - done, off + done) :
			 pread(fd, buf + done, len - done, off + done);
		if (n <= 0)
			return -1;
		done += n;
		calls++;
	}
	return calls;
}

int main(void)
{
	char buf[64], line[80], *big, *back;
	int fd, calls, i;

	fd = open(DEV_FILE, O_RDWR);
	if (fd < 0) {
		perror(DEV_FILE);
		return 1;
	}

	check(pwrite(fd, MSG, strlen(MSG), 0) == strlen(MSG),
	      "write \"" MSG "\" at offset 0");
	memset(buf, 0, sizeof(buf));
	check(pread(fd, buf, strlen(MSG), 0) == strlen(MSG) &&
	      !strcmp(buf, MSG), "read it back");

	/*
	 * The device XORs every byte it sends with 0x20: letters change case
	 * and the space becomes a NUL. Proof the data came through the DMA.
	 */
	if (!set_xor(0x20)) {
		memset(buf, 0, sizeof(buf));
		check(pread(fd, buf, strlen(MSG), 0) == strlen(MSG) &&
		      !memcmp(buf, "HELLO\0DMA", strlen(MSG)),
		      "with xor=0x20 it comes back as \"HELLO\\0DMA\"");
		set_xor(0);
	} else {
		check(0, "set " XOR_FILE);
	}

	big = malloc(BIG);
	back = malloc(BIG);
	for (i = 0; i < BIG; i++)
		big[i] = i * 7;
	calls = xfer_all(fd, big, BIG, 4096, 1);
	snprintf(line, sizeof(line), "write 200 KiB: %d calls of at most 64 KiB",
		 calls);
	check(calls == (BIG + CHUNK - 1) / CHUNK, line);
	check(xfer_all(fd, back, BIG, 4096, 0) > 0 && !memcmp(big, back, BIG),
	      "read the 200 KiB back intact");
	free(big);
	free(back);

	check(pwrite(fd, MSG, strlen(MSG), MEM_SIZE - 4) == 4,
	      "write at the end stops at 1 MiB");
	check(pread(fd, buf, sizeof(buf), MEM_SIZE) == 0,
	      "read past the end returns 0");
	check(pwrite(fd, MSG, strlen(MSG), MEM_SIZE) < 0 && errno == ENOSPC,
	      "write past the end fails with ENOSPC");

	close(fd);
	printf("%s\n", fails ? "FAILED" : "PASSED");
	return fails != 0;
}
