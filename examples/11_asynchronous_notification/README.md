# asynchronous_notification

Asynchronous notification lets a driver notify a user space process of I/O
readiness using a signal, usually SIGIO. In contrast, `poll()` and `select()`
provide I/O multiplexing and can put the calling task to sleep while waiting
for readiness events. A sleeping task does not occupy the CPU; the scheduler
can run other tasks.

In this example, we write a simple device driver which implements the basic
asynchronous notification mechanism. When some processes write any data to
the device file, the process who set the file descriptor as async will receive
the SIGIO signal. The process could catch this signal and check the file
descriptor state for reading or writing.

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

To test this kernel module, first compile and copy the test program located
in **test** directory to the target machine. Then, after the module is
successfully loaded, run the test program **async_notify.out**.

The program **async_notify.out** registers the signal handler for SIGIO signal,
which will be invoked when the file descriptor is notified. In the signal
handler, we add 1 to the global variable each time the handler is invoked.
The global variable will be checked in the loop in `main()` function, and set
it to 0 if it is greater than 0. In this case, the `read()` function will be
called on the file descriptor for read data from the valid file descriptor.

Sample output is:

```
no signal, continue!;
no signal, continue!;
no signal, continue!;
no signal, continue!;
signal is catched
Hello World!
```

---

### ¶ The end
