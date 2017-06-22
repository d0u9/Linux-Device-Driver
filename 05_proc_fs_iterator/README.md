# proc_fs_iterator

Basic proc file system interface is a convenient way to export less kernel
information to userspace. However, if there are chuncks of variables and
structures which are needed to be exported. The basic proc interface is not
the best choice.

Linux kernel provides a set of APIs to simplify the process of exporting, i.e.
the seq_file interface. The seq_file interface assumes that the user steps over
a long sequence data array. Each element in this big array contains data which
is prepare to return to userspace. This implementation sometimes can be treated
as a iterator -- the cursor iterates over the datas and process them one by one.

A formal documentation can be found in the kernel source tree:
`Documentation/filesystems/seq_file.txt`.

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

Copy **proc_fs_basic.ko** file to the target machine, then run:

```bash
insmod proc_fs_iterator.ko
```

## test the module

Execute `cat /proc/proc_fs_iterator` in the command line, a brief description
of the Genesis will be returned.

```
ffffffffa00002e8: Day 1: God creates the heavens and the earth.
ffffffffa0000216: Day 2: God creates the sky.
ffffffffa0000318: Day 3: God creates dry land and all plant life both large and small.
ffffffffa0000360: Day 4: God creates all the stars and heavenly bodies.
ffffffffa0000398: Day 5: God creates all life that lives in the water.
ffffffffa00003d0: Day 6: God creates all the creatures that live on dry land.
ffffffffa0000232: Day 7: God rests.
```

The hex numbers indicate the memory address which hold the contents. It may
differ from machine to machine.

---

### ¶ The end

