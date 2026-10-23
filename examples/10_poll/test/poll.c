// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#define DEV_PATH		"/dev/poll"
#define LOOPS			5

static char buff[1024];

int main(void)
{
	struct pollfd pollfd;
	ssize_t n;
	int fd, len;

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open " DEV_PATH);
		return 1;
	}

	pollfd.fd = fd;
	pollfd.events = POLLIN | POLLOUT;

	for (int c = LOOPS - 1; c >= 0; --c) {
		printf("polling ...\n");

		if (poll(&pollfd, 1, -1) < 0) {
			perror("poll");
			break;
		}

		if (pollfd.revents & POLLIN) {
			n = read(fd, buff, sizeof(buff) - 1);
			if (n < 0) {
				perror("read");
				break;
			}
			buff[n] = '\0';
			printf("[%d] read: %s\n", fd, buff);
		}

		if (pollfd.revents & POLLOUT) {
			len = snprintf(buff, sizeof(buff), "Hello world! -> %d", c);
			if (write(fd, buff, len) < 0) {
				perror("write");
				break;
			}
			printf("[%d] write: %s\n", fd, buff);
		}
	}

	close(fd);
	return 0;
}
