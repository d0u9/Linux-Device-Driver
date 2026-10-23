# proc_fs_iterator

The basic proc file interface is a convenient way to export a small amount of
kernel information to userspace. However, when there are many variables and
structures to export, it is not the best choice.

The Linux kernel provides the seq_file interface to simplify this. It treats
the data as a long sequence of records and lets the user step over them one by
one, like an iterator: a cursor moves over the records and each one is
formatted and returned to userspace in turn.

The formal documentation is in the kernel source tree:
`Documentation/filesystems/seq_file.rst`.

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

Copy **proc_fs_iterator.ko** file to the target machine, then run:

```bash
insmod proc_fs_iterator.ko
```

## test the module

Execute `cat /proc/proc_fs_iterator` in the command line, a brief description
of the Genesis will be returned.

```
00000000c3a1f0d2: Day 1: God creates the heavens and the earth.
00000000a8e4b7c1: Day 2: God creates the sky.
000000005f2d9e36: Day 3: God creates dry land and all plant life both large and small.
00000000e71c4a58: Day 4: God creates all the stars and heavenly bodies.
000000009b06d3f4: Day 5: God creates all life that lives in the water.
0000000041f8a2ce: Day 6: God creates all the creatures that live on dry land.
00000000d5937b10: Day 7: God rests.
```

The hex numbers are the addresses of the strings printed with `%p`. Since
Linux 4.15, `%p` prints a hashed value instead of the real address so kernel
addresses are not leaked to userspace; the values differ from boot to boot.

---

### ¶ The end

