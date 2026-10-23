// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_FILE	"/dev/seeking"

/* Seek with off/whence, read one byte and return it, or -1 on error. */
static int read_at(int fd, off_t off, int whence)
{
	char c;

	if (lseek(fd, off, whence) < 0)
		return -1;
	if (read(fd, &c, 1) != 1)
		return -1;
	return c;
}

int main(void)
{
	int fd, c;

	fd = open(DEV_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open " DEV_FILE);
		return 1;
	}

	for (int i = 15; i >= 0; --i) {
		c = read_at(fd, i, SEEK_SET);
		if (c < 0)
			goto on_error;
		printf("%d(dec) = %c(hex)\n", i, c);
	}

	/* SEEK_END is relative to the end of the 16-byte table */
	c = read_at(fd, -1, SEEK_END);
	if (c < 0)
		goto on_error;
	printf("SEEK_END -1 = %c\n", c);

	/* positions past the end wrap around */
	c = read_at(fd, 16 + 10, SEEK_SET);
	if (c < 0)
		goto on_error;
	printf("SEEK_SET 26 = %c\n", c);

	/* a negative position is rejected */
	if (lseek(fd, -1, SEEK_SET) < 0)
		printf("SEEK_SET -1 fails: %s\n", strerror(errno));
	else
		printf("SEEK_SET -1 unexpectedly succeeded\n");

	close(fd);
	return 0;

on_error:
	perror("seek/read");
	close(fd);
	return 1;
}
