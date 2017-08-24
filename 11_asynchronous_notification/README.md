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

---

### ¶ The end
