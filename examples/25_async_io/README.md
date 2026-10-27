# async_io

Asynchronous I/O in a driver, after LDD3's scullp: a request is accepted
at once, `io_submit()` returns, and the driver reports the result later
with `ki_complete()`. The device is a one-page ring buffer; what is written
to `/dev/async_io` can be read back once. No hardware is needed.

## sync and async kiocbs

Every read and write reaches the driver as `.read_iter` or `.write_iter`,
with a `struct kiocb` for the request and an `iov_iter` for the user's
buffers.

- `read(2)` and `write(2)` bring a *synchronous* kiocb (`ki_complete` is
  NULL, `is_sync_kiocb()` is true). The driver returns the result, and the
  system call returns it.
- `io_submit(2)` (kernel AIO) and io_uring bring an *asynchronous* kiocb.
  The driver may return `-EIOCBQUEUED`, a promise to call
  `iocb->ki_complete(iocb, result)` later, and the system call returns
  without waiting.

Here an asynchronous request is done right away but completed 100 ms later
from a delayed work item, as if a device took that long. The data is copied
at submission, while the `iov_iter`, which lives on the submitter's stack,
is valid and the submitter's memory is at hand; the worker is a kernel
thread and has neither. A real driver starts the transfer at submission and
calls `ki_complete()` from its interrupt handler.

The module uses a workqueue of its own and destroys it on unload, which
waits for a completion still running. A pending request holds its file and
the file the module, so the module cannot go away while one is queued.

## how user space gets the result

`io_setup()` creates a context with a completion ring. `ki_complete()` ends
up in `aio_complete()`, which writes a `struct io_event` with the result
and the request's `aio_data` into the ring and wakes the context's waiters.
User space then:

- waits with `io_getevents()`, or
- sets `IOCB_FLAG_RESFD` on the request: the eventfd in `aio_resfd` is
  signalled on completion, so `poll()` or epoll can wait for it together
  with other file descriptors.

io_uring calls the same `read_iter` and `write_iter` with its own
`ki_complete`, and puts the result in its completion queue.

## POSIX AIO is not this

glibc's `aio_read()` and `aio_write()` from `<aio.h>` are a different
interface with a similar name. glibc runs them as plain `pread()` and
`pwrite()` in a thread of its own, so the driver only ever sees synchronous
kiocbs (see `man 7 aio`). The test therefore uses the kernel's AIO system
calls, through `syscall()` since glibc has no wrappers for them (see
`man 2 io_submit`).

## in practice

Few character drivers complete requests asynchronously. The main users are
block devices and `O_DIRECT` files, where databases and virtual machines
keep many requests in flight; FunctionFS endpoint files are a character
device example. When a driver does not support it, io_uring runs the
request in a worker thread of its own instead.

## older ways of doing the same

- LDD3's driver had `.aio_read` and `.aio_write` and completed with
  `aio_complete()`; these are `.read_iter`, `.write_iter` and
  `iocb->ki_complete()` now.
- `.read` and `.write` still exist, but when a driver has both, `read(2)`
  and `write(2)` use them; this driver has only the `_iter` methods.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

Build the test program, statically linked so that it runs in the guest:

```bash
make -C test
```

The guest kernel needs `CONFIG_AIO`, and `CONFIG_EVENTFD` for the eventfd
part.

## Usage

Copy **load_module.sh**, **async_io.ko** and **test/aio_test.out** to the
target machine, then run:

```bash
sh load_module.sh
```

## test the module

```bash
./aio_test.out
dmesg
```

```
ok   sync write
ok   sync read gets "sync" back
ok   aio write: io_submit returned after 0 ms
ok   aio write: completed after 108 ms, result 9
ok   aio write: 9 bytes
ok   aio read: io_submit returned after 0 ms
ok   aio read: completed after 105 ms, result 9
ok   aio read gets "hello aio" back
ok   aio read with an eventfd submitted
ok   eventfd became readable
ok   eventfd counts 1 completion
ok   event collected without waiting
PASSED
```

`dmesg` shows each request done at submission and completed 100 ms later:

```
wrote 9, 9 in buffer
complete: 9
read 9, 0 left
complete: 9
```

The device also works with plain `read(2)` and `write(2)`:

```bash
echo abc > /dev/async_io
cat /dev/async_io
```

---

### ¶ The end
