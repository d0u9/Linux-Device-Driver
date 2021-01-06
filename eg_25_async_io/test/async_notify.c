#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <aio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BATCH_SIZE	3
#define DEV_FILE	"/dev/async_io"
#define MAX_LOOP	5

#define BUF_SIZE	(BATCH_SIZE * sizeof(int))
#define READ_SIGNAL	SIGUSR1
#define WRITE_SIGNAL	SIGUSR2

static volatile sig_atomic_t got_quit_sig = 0;
static volatile sig_atomic_t got_read_sig = 0;
static volatile sig_atomic_t got_write_sig = 0;
static volatile int read_thread_exit = 0;

void quit_sig_handler(int sig)
{
	printf("Quit signal\n");
	got_quit_sig = 1;
}

void read_sig_handler(int sig, siginfo_t *si, void *ctx)
{
	printf("read sig\n");
	got_read_sig = 1;
}

void write_sig_handler(int sig, siginfo_t *si, void *ctx)
{
	printf("write sig\n");
	got_write_sig = 1;
}

void cancel_aio_ops(struct aiocb *aiocb, const char *prefix)
{
	int ret;
	ret = aio_cancel(aiocb->aio_fildes, aiocb);
	switch(ret) {
	case AIO_CANCELED:
		printf("%s: I/O canceled\n", prefix);
		break;
	case AIO_NOTCANCELED:
		printf("%s, I/O not canceled\n", prefix);
		ret = aio_error(aiocb);
		printf("error: %s\n", strerror(ret));
		break;
	case AIO_ALLDONE:
		printf("%s, I/O all done\n", prefix);
		break;
	default:
		printf("%s, error: aio_cancel", prefix);
	}
}

void *read_thread(void *arg)
{
	int i, ret, loop;
	int *buf;
	sigset_t sigset;
	struct aiocb *aiocb_read = (struct aiocb*)arg;

	sigemptyset(&sigset);
	sigaddset(&sigset, WRITE_SIGNAL);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	// Write goes first here
	sleep(1);

	buf = (int*)(aiocb_read->aio_buf);
	got_read_sig = 1;
	// for (loop = 0; loop < MAX_LOOP; loop++) {
	for (loop = 0; ; loop++) {
		if (got_quit_sig) {
			cancel_aio_ops(aiocb_read, "read");
			goto exit;
		}

		if (got_read_sig) {
			got_read_sig = 0;

			if (loop != 0) {
				ret = aio_error(aiocb_read);
				if (ret != 0 && ret != EINPROGRESS) {
					printf("last aio_read() failed, %x, %s\n", ret, strerror(ret));
					goto exit;
				}

				ret = aio_return(aiocb_read);
				if (ret == 0 && read_thread_exit) {
					printf("Read 0 byte, exit!\n");
					cancel_aio_ops(aiocb_read, "read");
					goto exit;
				} else {
					printf("Read %d bytes\n", ret);
				}
			}

			sleep(3);
			ret = aio_read(aiocb_read);
			if (ret < 0) {
				printf("aio_read() returns %d, %s\n", ret, strerror(errno));
				goto exit;
			}
		} else {
			usleep(1000 * 100);
		}

	}
exit:

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, fd, ret, cnt, loop;
	int *buf;
	void *read_buf, *write_buf;
	pthread_t read_tid, write_tid;
	sigset_t sigset;
	struct sigaction sa;
	struct aiocb aiocb_read, aiocb_write;

	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = quit_sig_handler;
	ret = sigaction(SIGQUIT, &sa, NULL);
	if (ret < 0) {
		printf("sigaction(SIGQUIT) failed: %s\n", strerror(errno));
		goto out;
	}

	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = quit_sig_handler;
	ret = sigaction(SIGINT, &sa, NULL);
	if (ret < 0) {
		printf("sigaction(SIGTERM) failed: %s\n", strerror(errno));
		goto out;
	}

	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = read_sig_handler;
	ret = sigaction(READ_SIGNAL, &sa, NULL);
	if (ret < 0) {
		printf("sigaction(READ_SIGNAL) failed: %s\n", strerror(errno));
		goto out;
	}

	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = write_sig_handler;
	ret = sigaction(WRITE_SIGNAL, &sa, NULL);
	if (ret < 0) {
		printf("sigaction(READ_SIGNAL) failed: %s\n", strerror(errno));
		goto out;
	}

	fd = open(DEV_FILE, O_RDWR);
	if (fd < 0) {
		printf("Open %s failed: %s\n", DEV_FILE, strerror(errno));
		goto out;
	}

	read_buf = malloc(BUF_SIZE);
	if (!read_buf) {
		printf("alloc buff for aio_read failed: %s\n", strerror(errno));
		goto out;
	}

	write_buf = malloc(BUF_SIZE);
	if (!write_buf) {
		printf("alloc buff for aio_read failed: %s\n", strerror(errno));
		free(read_buf);
		goto out;
	}

	aiocb_read.aio_fildes	= aiocb_write.aio_fildes =	fd;
	aiocb_read.aio_nbytes	= aiocb_write.aio_nbytes =	BUF_SIZE;
	aiocb_read.aio_reqprio	= aiocb_write.aio_reqprio = 0;
	aiocb_read.aio_offset	= aiocb_write.aio_offset = 0;
	aiocb_read.aio_buf	= read_buf;
	aiocb_write.aio_buf	= write_buf;
	aiocb_read.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	aiocb_write.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	aiocb_read.aio_sigevent.sigev_signo = READ_SIGNAL;
	aiocb_write.aio_sigevent.sigev_signo = WRITE_SIGNAL;

	ret = pthread_create(&read_tid, NULL, read_thread, (void*)(&aiocb_read));
	if (ret != 0) {
		fprintf(stderr, "Create write thread failed: %s\n",
			strerror(errno));
		goto out;
	}

	sigemptyset(&sigset);
	sigaddset(&sigset, READ_SIGNAL);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	buf = (int*)(aiocb_write.aio_buf);
	cnt = 0;
	got_write_sig = 1;
	for (loop = 0; loop < MAX_LOOP;) {
		if (got_quit_sig) {
			cancel_aio_ops(&aiocb_write, "write");
			exit(1);
		}

		if (got_write_sig) {
			// Check last error
			got_write_sig = 0;

			if (loop != 0) {
				ret = aio_error(&aiocb_write);
				if (ret != 0 && ret != EINPROGRESS) {
					printf("last aio_write() failed, %x, %s\n", ret, strerror(ret));
					goto exit;
				}
			}

			sleep(2);
			printf("Write start at %d, len %d\n", cnt, BATCH_SIZE);
			for (i = 0; i < BATCH_SIZE; i++) {
				*(buf + i) = cnt++;
			}
			ret = aio_write(&aiocb_write);
			if (ret < 0) {
				printf("aio_write() returns %d, %s\n", ret, strerror(errno));
				goto exit;
			}
			loop++;
		} else {
			usleep(1000 * 100);
		}
	}
exit:
	printf("All data has been writen, signal read thread to quit!\n");
	read_thread_exit = 1;

	pthread_join(read_tid, NULL);

	return 0;

out:
	return ret;
}

