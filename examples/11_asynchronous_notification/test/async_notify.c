// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_FILE		"/dev/async_notify"

static volatile sig_atomic_t gotdata;
static char buffer[4096];

static void sighandler(int signo)
{
	if (signo == SIGIO)
		gotdata = 1;
}

int main(void)
{
	struct sigaction action;
	sigset_t block, orig;
	ssize_t count;
	int fd, flags;

	memset(&action, 0, sizeof(action));
	action.sa_handler = sighandler;
	sigaction(SIGIO, &action, NULL);

	fd = open(DEV_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open " DEV_FILE);
		return 1;
	}

	/* deliver SIGIO for this fd to us, then turn on O_ASYNC */
	fcntl(fd, F_SETOWN, getpid());
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_ASYNC);

	/*
	 * Keep SIGIO blocked except inside sigsuspend(). Otherwise a signal
	 * arriving between the gotdata test and the wait would be missed.
	 */
	sigemptyset(&block);
	sigaddset(&block, SIGIO);
	sigprocmask(SIG_BLOCK, &block, &orig);

	for (;;) {
		printf("waiting for SIGIO ...\n");
		while (!gotdata)
			sigsuspend(&orig);
		gotdata = 0;

		printf("signal is caught\n");

		count = read(fd, buffer, sizeof(buffer) - 1);
		if (count < 0) {
			perror("read");
			break;
		}
		buffer[count] = '\0';
		printf("%s\n", buffer);
	}

	close(fd);
	return 0;
}
