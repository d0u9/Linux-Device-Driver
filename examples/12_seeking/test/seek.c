#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEV_FILE	"/dev/seeking"

int main(void)
{
	int fd = -1;
	char buff[2] = {0};

	if ((fd = open(DEV_FILE, O_RDONLY)) < 0) {
		printf("error: %s\n", strerror(errno));
		return -1;
	}

	for (int i = 15; i >= 0; --i) {
		if (lseek(fd, i, SEEK_SET) < 0)
			goto on_error;

		if (read(fd, buff, 1) < 0)
			goto on_error;

		printf("%d(dec) = %s(hex)\n", i, buff);
	}

	close(fd);
	return 0;

on_error:
	printf("error: %s\n", strerror(errno));
	close(fd);
	return -1;
}

