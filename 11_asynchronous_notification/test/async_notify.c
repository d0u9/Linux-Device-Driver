#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define DEV_FILE		"/dev/async_notify"

static int gotdata = 0;
static char buffer[4096] = {0};

void sighandler(int signo)
{
	if (signo == SIGIO)
		gotdata++;
	return;
}

int main(int argc, char **argv)
{
	int count, fd;
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = sighandler;
	action.sa_flags = 0;

	sigaction(SIGIO, &action, NULL);

	fd = open(DEV_FILE, O_RDONLY);
	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | FASYNC);

	while(1) {
		sleep(1);

		if (!gotdata) {
			printf("no signal, continue!\n");
			continue;
		}
		printf("signal is catched\n");
		gotdata = 0;

		count = read(fd, buffer, 4096);
		printf("%s\n", buffer);
	}

	return 0;
}
