# asynchronous_notification

Asynchronous notification lets a driver notify a user space process of I/O
readiness using a signal, usually SIGIO. In contrast, `poll()` and `select()`
provide I/O multiplexing and can put the calling task to sleep while waiting
for readiness events. A sleeping task does not occupy the CPU; the scheduler
can run other tasks.

In this example, we write a simple device driver which implements the basic
asynchronous notification mechanism. When a process writes data to the device
file, every process that enabled `O_ASYNC` on it receives SIGIO. The process
can catch this signal and then read the new data.

The steps are:

1. The process installs a SIGIO handler.
2. `fcntl(fd, F_SETOWN, getpid())` tells the kernel who gets the signal.
3. `fcntl(fd, F_SETFL, flags | O_ASYNC)` turns notification on. The kernel
   calls the driver's `.fasync`, which adds the file to the device's
   `async_queue` with `fasync_helper()`.
4. On every write, the driver calls
   `kill_fasync(&dev->async_queue, SIGIO, POLL_IN)` to signal all of them.

When the file is closed, `.release` removes it from the queue again. The VFS
would also do this on its own, since `__fput()` calls `.fasync` with
`fd == -1` for files with `FASYNC` set; the example keeps the explicit call so
the pairing is visible.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh** and **async_notify.ko** files to the target machine,
then run:

```bash
sh load_module.sh
```

## test the module

Build the program in the **test** directory with `make`, load the module, and
run `./async_notify.out`. It waits for SIGIO; in another console write to the
device:

```bash
echo "Hello World!" > /dev/async_notify
```

The signal handler only sets a flag. The main loop keeps SIGIO blocked and
waits in `sigsuspend()`, which unblocks it atomically while sleeping, so a
signal cannot slip in between testing the flag and going to sleep. When the
flag is set, the program reads the device and prints the message.

Sample output:

```
waiting for SIGIO ...
signal is caught
Hello World!

waiting for SIGIO ...
```

---

### ¶ The end
