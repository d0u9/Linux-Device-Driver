# Hello World Module

## Source code

Finally, we reached the point of doing some programming. The first program is 
a simple "Hello World" like any other programming book. However, the difference
is that our "hello world" program will run inside kernelspace rather than in
userspace.

The code is pretty simple:

```C
#include <linux/module.h>

static int __init m_init(void)
{
	printk(KERN_ALERT "Hello, world!\n");
	return 0;
}

static void __exit m_exit(void)
{
	printk(KERN_ALERT "Bye, world!\n");
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas Su");
MODULE_DESCRIPTION("Hello World program");
```

Put this source file in an empty directory, and name it `hello_world.c`.

This sample code is quite simple, with just a few lines of code there, and it
is self-explained in that only two functions are defined with a meaningful name
that points out the functions they do. A few special macros are used to tell
the kernel how to load and link this module and what the extra information this
module contains.

The module starts its journey in the function of `m_init()` and ends in
`m_exit()`. Two macros, `module_init()` and `module_exit()`, bridge the module's
function to the the kernel.

Like in userspace, where we have the `printf()` function for dumping messages
to the terminal, the kernel also has its print function named `printk()`. Yes,
the suffix 'k' stands for the kernel. Knowing the difference between `printf()`
in userspace and `printk()` in kernelspace has no contribution to our
understanding of how to load and unload a module. So, we skip the details here
and give more dives later.

Programming interfaces exported by the kernel are scattered in a set or a
single header file. According to the resources the device driver requests, the
sources code contains different header files, which provide the interface to
that resource.

The header file `linux/module.h` contains functions and macros that are very
basic for almost every kernel module.

## Build

Like any C project, including the Linux kernel itself, one or more Makefiles
exist, which tell how to piece each source file together, technically speaking,
the compiling and linking process.

```
obj-m := hello_world.o
```

Pretty simple, all right? It is sufficient for simple projects with one source
file like our "hello world" example. Copy and paste it to a text file named
`Makefile` in the same directory where the C file locates.

Then, build the first kernel module by running the command below in your
terminal. Run the `make` command in the same directory that contains the
Makefile and the C source file. 

```
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

A particular make variable `M` is passed in the command above, which points to
the directory that Makefile itself residents by assigning via `$(pwd)`. Besides,
the `-C` option, which stands for "change to directory", tells the make command
to read Makefiles in `/lib/modules/$(uname -r)/build` instead of the current
work directory. Finally, `modules` is the build target. Feel strange, all
right? The Makefile created before seems not to be used at all. That is due to
the peculiarity of how the kernel's build system works. We will talk about this
in detail later.

The loadable module file, suffixed with '.ko', is created if the `make` command
issues no error. Curious readers who wonder about the kernel module file's file
format can use the `file` command to inspect the detailed information of the
`.ko` file. The `.ko` file is indeed an `ELF` relocatable file that is widely
used in Linux.

## Load and Test

There are two common commands used for loading a module file into the kernel.
One is `modprobe`, which is mainly used, and another is `insmod`, which is
trivial and straightforward. For most scenarios, `modprobe` is the best choice
for it is more clever and can handle module dependencies. The `modprobe` tool
finds modules and relative files by looking up in the module directory,
`/lib/modules/$(uname -r)`. Add configurations in `/etc/modprobe.d` directory
to set additional search locations. However, in the "hello world" module,
`insmod` command is a better choice since it is simple enough that no extra
knowledge is needed. As its name implies: insert a module file into the kernel.

```bash
# Use insmod to insert hello-world module into the kernel.
insmod hello_world.ko
```

The sample code's `m_init()` function is instantly invoked and executed as long
as the module file is loaded successfully. This function is simple in that only
a string is dumped by the `printk()` function. Messages printed via `printk()`
function are written to an internal circular buffer maintained by the kernel.
Userspace tools, such as `dmesg`, can fetch the kernel's log messages and print
them in the control terminal.

```bash
dmesg
```

Your terminal screen maybe is flooded with various messages after executing the
`dmesg` command. But don't worry, the "Hello, world!" message printed by example
will finally appear at the end of the dump. The `dmesg` tool prints or controls
the ring buffer of the kernel. By default, without any command options
specified, `dmesg` displays all kernel messages. If the kernel's ring buffer
does not overrun, the log printed by the kernel and various kernel modules after
the system's boot is dumped.

Use 'dmesg -C` to clear the ring buffer.

## Unload module

The `rmmod` command accepts a parameter that is not the file name of the module
but the module name registered in the kernel. In the "hello world" example,
both file name and module name are identical, except that the file name is
suffixed with ".ko". 

Execution of `rmmod` command sometimes may fail due to the resource monopolized
by this module being still busy.

The module unloader of Linux invokes the `m_exit()` function during the module's
removal phase. The memory and resources allocated and requested during the
module's runtime must be freed and unregistered finally in the `m_exit()`
function. If not, such memory or resources will be leaked forever until the
system's next reboot. Memory leakage is a very severe problem that affects the
stability and durability of the whole system and may cause innocent processes to
be killed when OOM happens.

## Test in QEMU guest

As mentioned in previous sections, samples in this book can be played and tuned
in a QEMU virtual machine, in which we have set up a simple system based on the
`busybox` and `initramfs` filesystem. For some Linux users, it looks pretty
redundant to develop modules in a guest machine because all tests can be run and
tested directly on the host. However, developers can benefit much from using
virtual machines due to the distinct differences between the kernel module and
the userspace program.

The first bonus of developing modules in a virtual guest is that system crash
caused by module flaws won't affect the host machine. The kernel panic is
horrible. It may cause a deadly result to a system, such as file lost or system
broken, and further make the host system unusable. Second, using a virtual
machine makes it very convenient to test a kernel module against multiple kernel
versions. Third, for device driver developers, sometimes underlying hardware that
the module drives is not ready at the beginning of project initialization.
However, driver developers have no time to wait for hardware's readiness for the
sake of time. The virtual machine solves this problem by writing a simple
virtual hardware that implements the basic circuit interfaces as design.

Now, back to the QEMU virtual machine we have set up before, login into the
guest's terminal, and change the working directory to the `nfs` filesystem
mounted for sharing files between host and guest. Execute the commands below:

1. On the host, recompile the module against the stable kernel:

```
make -C ~/LDD_ROOT/kernels/linux-stable M=$(pwd) modules
```

2. load and test in guest:

```bash
# This is an auxiliary shell script created before.
qemu_run.sh

# Change to the NFS sharing direcotry. It is mounted at /mnt directory.
cd /mnt

# Change to our example directory
cd /mnt/Linux-Device-Driver/eg_01_hello_world

# Insert module
insmod hello_world.ko

# Verify via dmesg
dmesg
```

// TODO: explain why message is printed instantly.

To unload the module, use `rmmod` command:

```
rmmod hello_world
```

# ¶ The end


