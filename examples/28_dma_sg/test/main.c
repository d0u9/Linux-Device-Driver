// SPDX-License-Identifier: GPL-2.0
/*
 * Write to the device's on-board memory through /dev/dma_sg and read it
 * back. Each call is one DMA transfer straight to or from this program's
 * buffer, which is scattered over pages wherever malloc() put them.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEV_FILE	"/dev/dma_sg"
#define LAST_FILE	"/sys/bus/pci/drivers/dma_sg/%s/dma_sg/last"
#define XOR_FILE	"/sys/module/dma_sg/parameters/xor"
#define MEM_SIZE	(1024 * 1024)
#define BIG		(300 * 1024)
#define MSG		"hello dma_sg"

static int fails;
static char last_path[256];
static volatile unsigned long bad_addr = 8;	/* nothing is mapped there */

static void check(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		fails++;
}

/* "pages P segments S descriptors D" of the last transfer */
static void show_last(const char *what)
{
	char line[128] = "";
	FILE *f = fopen(last_path, "r");

	if (f) {
		if (!fgets(line, sizeof(line), f))
			line[0] = '\0';
		fclose(f);
	}
	line[strcspn(line, "\n")] = '\0';
	printf("     %-24s %s\n", what, line);
}

static int set_xor(int val)
{
	FILE *f = fopen(XOR_FILE, "w");

	if (!f)
		return -1;
	fprintf(f, "%d\n", val);
	return fclose(f);
}

/* read or write all of @len; returns the number of calls, or -1 */
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

int main(int argc, char *argv[])
{
	char buf[64], line[96], *big, *back;
	int fd, calls, i;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <pci address, e.g. 0000:01:00.0>\n",
			argv[0]);
		return 2;
	}
	snprintf(last_path, sizeof(last_path), LAST_FILE, argv[1]);

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
	show_last("a few bytes:");

	/* the device XORs every byte it sends with 0x20: case flips */
	if (!set_xor(0x20)) {
		memset(buf, 0, sizeof(buf));
		check(pread(fd, buf, 5, 0) == 5 && !memcmp(buf, "HELLO", 5),
		      "with xor=0x20 it comes back as \"HELLO\"");
		set_xor(0);
	} else {
		check(0, "set " XOR_FILE);
	}

	/* not page aligned: the first and last pages are partly used */
	big = malloc(BIG + 1);
	back = malloc(BIG + 1);
	for (i = 0; i < BIG; i++)
		big[1 + i] = i * 7;
	calls = xfer_all(fd, big + 1, BIG, 8192, 1);
	show_last("300 KiB, unaligned:");
	snprintf(line, sizeof(line), "write 300 KiB from an unaligned buffer: %d call%s",
		 calls, calls == 1 ? "" : "s");
	check(calls > 0, line);
	check(xfer_all(fd, back + 1, BIG, 8192, 0) > 0 &&
	      !memcmp(big + 1, back + 1, BIG), "read the 300 KiB back intact");
	free(big);
	free(back);

	check(pwrite(fd, MSG, strlen(MSG), MEM_SIZE - 4) == 4,
	      "write at the end stops at 1 MiB");
	check(pread(fd, buf, sizeof(buf), MEM_SIZE) == 0,
	      "read past the end returns 0");
	check(pwrite(fd, MSG, strlen(MSG), MEM_SIZE) < 0 && errno == ENOSPC,
	      "write past the end fails with ENOSPC");
	/* volatile: the compiler would see the bad address and warn */
	check(pwrite(fd, (void *)bad_addr, 16, 0) < 0 && errno == EFAULT,
	      "write from a bad address fails with EFAULT");

	close(fd);
	printf("%s\n", fails ? "FAILED" : "PASSED");
	return fails != 0;
}
