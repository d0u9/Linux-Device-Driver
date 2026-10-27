// SPDX-License-Identifier: GPL-2.0
/*
 * Read and write /dev/async_io with the kernel's AIO system calls, so that
 * the driver gets asynchronous kiocbs. glibc's aio_read() and aio_write()
 * would not do: they run pread() and pwrite() in a thread of their own.
 * glibc has no wrappers for these calls, so they go through syscall().
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/aio_abi.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define DEV_FILE	"/dev/async_io"

static int fails;

static void check(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		fails++;
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int io_setup(unsigned int nr, aio_context_t *ctx)
{
	return syscall(SYS_io_setup, nr, ctx);
}

static int io_destroy(aio_context_t ctx)
{
	return syscall(SYS_io_destroy, ctx);
}

static int io_submit(aio_context_t ctx, long nr, struct iocb **iocbs)
{
	return syscall(SYS_io_submit, ctx, nr, iocbs);
}

static int io_getevents(aio_context_t ctx, long min, long max,
			struct io_event *events, struct timespec *timeout)
{
	return syscall(SYS_io_getevents, ctx, min, max, events, timeout);
}

/* Describe one request; aio_data comes back unchanged in its io_event. */
static void prep(struct iocb *cb, int fd, int opcode, void *buf, size_t len,
		 uint64_t data)
{
	memset(cb, 0, sizeof(*cb));
	cb->aio_fildes = fd;
	cb->aio_lio_opcode = opcode;
	cb->aio_buf = (uintptr_t)buf;
	cb->aio_nbytes = len;
	cb->aio_data = data;
}

/* Submit one request, wait for it, and report how long each step took. */
static long submit_and_wait(aio_context_t ctx, struct iocb *cb,
			    const char *what)
{
	struct iocb *cbs[1] = { cb };
	struct io_event ev;
	long t0, t1, t2;
	char line[80];

	t0 = now_ms();
	if (io_submit(ctx, 1, cbs) != 1) {
		perror("io_submit");
		return -1;
	}
	t1 = now_ms();
	if (io_getevents(ctx, 1, 1, &ev, NULL) != 1) {
		perror("io_getevents");
		return -1;
	}
	t2 = now_ms();

	snprintf(line, sizeof(line), "%s: io_submit returned after %ld ms",
		 what, t1 - t0);
	check(t1 - t0 < 50, line);
	snprintf(line, sizeof(line), "%s: completed after %ld ms, result %lld",
		 what, t2 - t0, (long long)ev.res);
	check(t2 - t0 >= 90 && ev.data == cb->aio_data, line);
	return ev.res;
}

int main(void)
{
	char buf[64] = "";
	aio_context_t ctx = 0;
	struct pollfd pfd;
	struct io_event ev;
	struct timespec zero = { 0, 0 };
	struct iocb cb, *cbs[1] = { &cb };
	uint64_t count;
	int fd, efd;

	fd = open(DEV_FILE, O_RDWR);
	if (fd < 0) {
		perror(DEV_FILE);
		return 1;
	}

	/* 1. read(2) and write(2): synchronous kiocbs, done at once */
	check(write(fd, "sync", 4) == 4, "sync write");
	check(read(fd, buf, sizeof(buf)) == 4 && !memcmp(buf, "sync", 4),
	      "sync read gets \"sync\" back");

	/* 2. an AIO context: up to 8 requests in flight */
	if (io_setup(8, &ctx) < 0) {
		perror("io_setup");
		return 1;
	}

	/* 3. asynchronous write, then read */
	prep(&cb, fd, IOCB_CMD_PWRITE, "hello aio", 9, 1);
	check(submit_and_wait(ctx, &cb, "aio write") == 9, "aio write: 9 bytes");

	memset(buf, 0, sizeof(buf));
	prep(&cb, fd, IOCB_CMD_PREAD, buf, sizeof(buf), 2);
	check(submit_and_wait(ctx, &cb, "aio read") == 9 &&
	      !strcmp(buf, "hello aio"), "aio read gets \"hello aio\" back");

	/* 4. completion through an eventfd, which poll() can wait for */
	efd = eventfd(0, 0);
	write(fd, "eventfd", 7);
	memset(buf, 0, sizeof(buf));
	prep(&cb, fd, IOCB_CMD_PREAD, buf, sizeof(buf), 3);
	cb.aio_flags = IOCB_FLAG_RESFD;
	cb.aio_resfd = efd;
	check(io_submit(ctx, 1, cbs) == 1, "aio read with an eventfd submitted");

	pfd.fd = efd;
	pfd.events = POLLIN;
	check(poll(&pfd, 1, 1000) == 1, "eventfd became readable");
	check(read(efd, &count, sizeof(count)) == 8 && count == 1,
	      "eventfd counts 1 completion");
	check(io_getevents(ctx, 1, 1, &ev, &zero) == 1 && ev.res == 7 &&
	      !strcmp(buf, "eventfd"), "event collected without waiting");

	close(efd);
	io_destroy(ctx);
	close(fd);
	printf("%s\n", fails ? "FAILED" : "PASSED");
	return fails != 0;
}
