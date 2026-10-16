#include <stdio.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEV_PATH		"/dev/poll"

#define BUF_LEN			1024

char buff[1024] = {0};

int main(void)
{
	
	int fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		printf("open %s error\n", DEV_PATH);
		return -1;
	}

	struct pollfd pollfd = { fd, POLLIN | POLLOUT, 0 };

	int c = 5;
	while (c--) {
		printf("polling ...\n");

		int err = poll(&pollfd, 1, -1);
		if (err < 0) {
			printf("poll error\n");
			break;
		}

		if (pollfd.revents & POLLIN) {
			read(pollfd.fd, buff, BUF_LEN);
			printf("[%d] read: %s\n", pollfd.fd, buff);
		}

		if (pollfd.revents & POLLOUT) {
			int len =sprintf(buff, "Hello world! -> %d", c);
			write(pollfd.fd, buff, len);
			printf("[%d] write: %s\n", pollfd.fd, buff);
		}

	}

	close(fd);

	return 0;
}
