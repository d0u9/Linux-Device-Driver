# ioctl

The ioctl() function manipulates the underlying device parameters of special
files. Most devices can perform operations beyond simple data transfers; user
space must often be able to request, for example, that the device lock its door,
eject its media, report error information, change a baud rate, or self destruct.
These operations are usually supported via the ioctl method.

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

Copy **ioctl.ko** and **load_module.sh** file to the target machine, then run:

```bash
./load_module.sh
```

## test the module

In this example we implement three different requests:

1. `IOCTL_RESET`, reset the device, clear the internal buffer. No parameters is
needed.

2. `IOCTL_HOWMANY`, set the number of how many times the message will be
printed. The parameter is an integer.

3. `IOCTL_MESSAGE`, set the content of the message which will be printed out.
The parameter type is string.

I have written a simple userspace C program to test the ioctl() interface of
our device. You can find the source files in the **test** directory. Simply
execute `make` to build the program. Note that, if you want to test different
ioctl() requests, simply uncomment the statements in the source file
accordingly, and rebuild the program.

Defaultly, read the device file via `cat /dev/ioctl` you will get:

```
Hello ioctl()!
Hello ioctl()!
Hello ioctl()!
```

It prints `hello ioctl()!` three times.

After executing the test program in the **test** directory, the message changes
to:

```
Hello, Linux!!
Hello, Linux!!
Hello, Linux!!
```

Uncomment the statements in the test program to test different requests.

---

### ¶ The end


