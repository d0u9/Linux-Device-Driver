# poll

The `poll()` system call provides I/O multiplexing: a user space process can
monitor multiple file descriptors for I/O readiness. Unlike a blocking
`read()` or `write()` on a single descriptor, `poll()` can wait for events on
a set of descriptors and report which ones are ready for the requested I/O.

In this example, we implement a simple char device to test poll. After the
module is loaded, a timer decides when the device is readable or writable:
every second it sets a read flag and wakes up the readers, and every other
second it also sets a write flag and wakes up the writers. A `read()` or
`write()` consumes the matching flag; without it, the call sleeps until the
timer sets it again, or fails with `-EAGAIN` if the file was opened with
`O_NONBLOCK`. A read returns the whole stored message and a write replaces it.

The driver's `.poll` method does two things only:

1. Call `poll_wait()` on every wait queue that may change the device state.
   This does not sleep; it registers the queue so that a later `wake_up()`
   makes the kernel ask the driver again.
2. Return the current state as a mask of `EPOLLIN`/`EPOLLRDNORM` and
   `EPOLLOUT`/`EPOLLWRNORM`.

The sleeping itself is done by the kernel's poll core, which may call
`.poll` several times during one `poll()` call. So `.poll` must only look at
the state and never change it; the flags are consumed by `read()` and
`write()`.

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

Copy **load_module.sh** and **poll.ko** files to the target machine, then run:

```bash
sh load_module.sh
```

## test the module

After loading the driver, build the program in the **test** directory with
`make` and run `./poll.out`.

Each time the file descriptor is writable, the program writes the current loop
counter into the device file. Likewise, each time the file descriptor is
readable, it reads the message and prints it to the screen. The descriptor
becomes readable every second and writable every two seconds.

Sample output as below:

```
polling ...
[3] read: Hello World!
[3] write: Hello world! -> 4
polling ...
[3] read: Hello world! -> 4
polling ...
[3] read: Hello world! -> 4
[3] write: Hello world! -> 2
polling ...
[3] read: Hello world! -> 2
polling ...
[3] read: Hello world! -> 2
[3] write: Hello world! -> 0
```

---

### ¶ The end
