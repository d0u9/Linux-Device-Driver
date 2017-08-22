# poll

The poll mechanism is a Linux kernel mechanism for I/O event notification. Its
function is to monitor multiplue file descriptors to see I/O is possible on
any of them. Different from the tranditional block I/O, poll mechanism promise
its users to monitor many file desciptors, normal file descriptrs or socket
file descriptors. If any interested event occurs on any file desciptor, for
example the file desciptor can be read or write, the poll will notify the user
process and tell it what happlend.

In this example, we implement a simple char device to test our poll ability.
After the module is successfully loaded, a timer will be used to control the
device if it is readable of writable. Each time the timer is out, the read flag
and write flag will be set to 1, and the value will be consumed when some
processes read or write this device file. Here, we set the read flag each
second elapsed, and set the write flag every two seconds elapsed.

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

Copy **load_module.sh** and **poll.ko** files to the target machine, then run:

```bash
sh load_module.sh
```

## test the module

After successfully load the driver, simply run the executable **poll.out** in
the **test** directory.

Each time the file descriptor is writable, we write the current loop counter
into the device file. Likewise, each time the file desciptor is readable, we
read the content from it and print to the screen.

As we described before, the file descriptor to the device file we opened will
be readable every 1 second, and will be writable every 2 seconds.

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
