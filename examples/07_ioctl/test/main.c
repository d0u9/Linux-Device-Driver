// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../ioctl_cmd.h"

#define DEV_FILE		"/dev/ioctl"
#define DEFAULT_NEW_MSG		"Hello, Linux!!\n"

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s reset\n"
		"       %s howmany <count>\n"
		"       %s message [text]\n",
		prog, prog, prog);
}

int main(int argc, char *argv[])
{
	struct ioctl_msg_arg msg_arg;
	int fd, ret;

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(DEV_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open " DEV_FILE);
		return EXIT_FAILURE;
	}

	if (!strcmp(argv[1], "reset")) {
		ret = ioctl(fd, IOCTL_RESET);
	} else if (!strcmp(argv[1], "howmany") && argc == 3) {
		ret = ioctl(fd, IOCTL_HOWMANY, strtoul(argv[2], NULL, 0));
	} else if (!strcmp(argv[1], "message")) {
		msg_arg.msg = argc == 3 ? argv[2] : DEFAULT_NEW_MSG;
		msg_arg.len = strlen(msg_arg.msg);
		ret = ioctl(fd, IOCTL_MESSAGE, &msg_arg);
	} else {
		usage(argv[0]);
		close(fd);
		return EXIT_FAILURE;
	}

	if (ret < 0)
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
	else
		printf("retval = %d\n", ret);

	close(fd);
	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
