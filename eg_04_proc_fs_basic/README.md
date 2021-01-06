# proc_fs_basic

The proc file system acts as an interface to internal data structures in the
kernel. It can be used to obtain information about the system and to change
certain kernel parameters at runtime (sysctl).

In this example, our module creates a directory named `proc_demo` in the
`/proc`, which in turn contains two files named `proc_fs` and `proc_fs_mul`.

Any read to `proc_fs` will be returned **Hello World!**. Similarly, read to
`proc_fs_mul` will be returned **Hello World!**, but three times.

The `proc_fs_mul` file intends to demonstrate passing variables between
module load and file open.

The proc interface varied a lot from the content of LDD3, it introduces new
interfaces and methods to dump content from Kernel to userspace more easily.
Check '/home/doug/Linux_Kernel/linux/Documentation/filesystems/seq_file.txt'
for more.

This example uses `single_open()` interface. This interface is convenivent for
dump less contents to userspace. For large chunck data, the iterator of
seq_file is more powerful.

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
insmod proc_fs_basic.ko
```

## test the module

Execute `cat /proc/proc_demo/proc_fs` in the command line, you will get
**Hello World!**.

Execute `cat /proc/proc_demo/proc_fs_mul` in the command line, you will get
**Hello World!** three times.

## Note

`proc_create_data()` accepts a data pointer which can be passed to `open()`
method. In `open()` method, `PDE_DATA(inode)` helps retrieving pointer to data
from inode structure.

Also, `simple_open()` method's third argument acts as a data pointer as well.
Users can retrieve the data pointer from `struct seq_file`'s private field.

---

### ¶ The end
