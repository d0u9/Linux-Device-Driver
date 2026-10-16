# asynchronous_notification

Asynchronous notification is a mechanism to notify the user program that
interested I/O is ready to operate. Using poll or select is a sort to
synchonous of I/O notification, the process will be blocked on the call to
poll or select function until the underlaying I/O device is becoming ready
again. The CPU is occupied during the entire period, no other operations can
be taken. However, in asynchronous notification, the CPU can be handled to
other process for other tasks, when the I/O is ready, a signal(usually SIGIO)
is send to the process which interrupts the current process to let the I/O
event can be processed in time.

In this example, we write a simple device driver which implements the basic
asynchronous notification mechansim. When some processes write any data to
the device file, the process who set the file descriptor as async will receive
the SIGIO signal. The process could catch this signal and check the file
descriptor state for reading or writing.

## build the module

To build this module, execute:

```bash
make KERNELDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KERNELDIR` environment variable, simply
execute `make` is enough.

If neither `KERNELDIR` environment variable nor `KERNELDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh** and **async_notify.ko** files to the target machine,
then run:

```bash
sh load_module.sh
```

## test the module

To test this kernel module, first compile and copy the test program loacated
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
