# ioctl

The ioctl() function manipulates the underlying device parameters of special
files. Most devices can perform operations beyond simple data transfers; user
space must often be able to request, for example, that the device lock its door,
eject its media, report error information, change a baud rate, or self destruct.
These operations are usually supported via the ioctl method.

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

Copy **ioctl.ko** and **load_module.sh** file to the target machine, then run:

```bash
./load_module.sh
```

## test the module

In this example we implement three different requests. They are defined in
`ioctl_cmd.h`, which both the module and the test program include:

1. `IOCTL_RESET`, restore the default message and count. No argument.

2. `IOCTL_HOWMANY`, set how many times the message is printed. The count is
passed by value as the ioctl argument, so the command is defined with `_IO`.
The previous count is returned. This request requires `CAP_SYS_ADMIN`.

3. `IOCTL_MESSAGE`, set the message. The argument points to a
`struct ioctl_msg_arg` holding the length and a pointer to the text, so the
driver copies twice: the struct first, then the text it points to.

By default, reading the device file via `cat /dev/ioctl` you will get:

```
Hello ioctl()!
Hello ioctl()!
Hello ioctl()!
```

Build the test program in the **test** directory with `make`, then try:

```bash
./ioctl_test.out message          # message becomes "Hello, Linux!!"
./ioctl_test.out howmany 5        # print it 5 times, returns the old count 3
./ioctl_test.out reset            # back to the defaults
cat /dev/ioctl
```

## locking

`read()` and `ioctl()` may run at the same time from different processes, so
both take the device mutex. They use `guard(mutex)(&dev->lock)` from
`<linux/cleanup.h>` (Linux 6.5+): the mutex is locked at that line and
unlocked automatically when the function returns, on every return path, so no
`mutex_unlock()` or `goto` is needed.

## why there is no compat_ioctl

A 32-bit program on a 64-bit kernel calls `.compat_ioctl`, not
`.unlocked_ioctl`. The generic `compat_ptr_ioctl` helper only forwards the
argument as a pointer, which is correct only if every command takes no
argument or a pointer to a structure with the same layout on 32-bit and
64-bit. Neither holds here: `IOCTL_HOWMANY` passes a plain integer, and
`struct ioctl_msg_arg` contains a pointer, so it is 8 bytes on 32-bit and 16
bytes on 64-bit. This example therefore supports 64-bit callers only. New
interfaces usually avoid the problem by passing pointers as `__u64` fields and
values through pointers.

---

### ¶ The end


