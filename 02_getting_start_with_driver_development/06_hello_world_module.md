# Hello World Module

## Source code

Finially, we reach the point that some programming will be made. The first
program, like in any other programming books, is a simple "Hello World" program.
However, the difference is that our hello world code is running insdie
kernelspace rather than userspace.

The code is fairly simple

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

Put this source file an in empty directory, and name it the `hello_world.c`.

It is a quite simple example with just a few lines of code there. This code is
self-explained for that only two functions are defined with meaningful name that
points out what the function does. A few of special macros are used to tell the
kernel how to run this module and what extra information this module contains.

Briefly, this module starts its journal in the function of `m_init()`, and ends
in the function of `m_exit()`. Two macros, `module_init()` and `module_exit()`,
bridge our module's function to the kernel.

Like in userspace we have a `printf()` function for dumping messages to a
terminal, the kernel has its own print function named `printk()`. Yes, the
suffix 'k' stands for kernel. Knowing the difference of `printf()` in userspace
and `printk()` in kernelspace has no contribution to our understanding of how 
to load and unload a kernel. So, we skip talking about it here, and give more
details later.

Any programming interface exported by kernel are attributed to a set or
a single header file. For different device driver, it will request different
resource from kernel, and different header files are included in source code.
Header file `linux/module.h` contains functions and macros that are very basic
for almost every kernel module.

## Build

Like any C projects, include the Linux kernel itself, there is one or more
Makefiles exist, which tell the fact that how to piece each source file
together. For a technically speaking, the compiling and linking process.

```
obj-m := hello_world.o
```

Pretty simple, alright? For simple projects with one source file like our
"hello world" example, it is sufficient. Copy and paste it in the same directory
where the C file locates.


Then, build the first kernel module by running command below in your shell. Be
cautious, the working directory in the moment of running `make` command must be
the folder that contains the Makefile and source file. 

```
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

In the command above, a special make variable `M` is passed, which points to
the directory that Makefile itself residents by assigning via `$(pwd)`. Besides,
`-C` option instructs make to read Makefiles in `/lib/modules/$(uname -r)/build`
instead of own Makefile. Finally, `modules` is our build target here. Feel
strange alright? The Makefile we created before seems not be used at all. This
is due to difference of compiling kernel module. We will talk this in later
chapters.

Loadable module file, suffixed with '.ko', will be created if no error was
reported by `make` command. For curious readers who wondering the format of
module file, the result returned by using `file` command to inspect our `.ko`
file shows that module file is an ELF relocatable file indeed.

## Load and Test

There are two common commands used for loading a module file into kernel. One
is `modprobe` which is used mostly. Another is `insmod` which is trivial and
simple. For most scenarios, `modprobe` is the best choice for that it is more
clever and can handle module dependencies. `modprobe` looks in the module
directory `/lib/modules/$(uname -r)` for all the modules and other files, except
for the optional configuration files in the `/etc/modprobe.d` directory. In our
"hello world" module, however, `insmod` is a good choice, since it is simple and
straightforward as its name implies: insert a module file into kernel.

```bash
# Use insmod to insert hello-world module into kernel.
insmod hello_world.ko
```

The `m_init()` function in our sample code will be immediately invoked and
executed as along as module file is loaded successfully. In that function,
a string message is printed via `printk()` function. Messages dumped via
`printk()` function are written to a internal circular buffer in the kernel.
Usespace tools, such as `dmesg`, can fetch there kernel messages and print
them in the control terminal.

```bash
dmesg
```

Maybe, in this time after execution of `dmesg` command, your screen are flooded
by various messages, and our "Hello, world!" string is displayed in the end.
Messages other than "Hello, world!" string are that which are printed by various
part of kernel and modules since boot up. If your message buffer is not overrun,
initialization information during the very begin time of booting kernel may be
found at the beginning of dumped message.

## Unload module

`modprobe` command can also be used in unloading a kernel module. But, for
simplicity, `rmmod` is used here. `rmmod` is the twin of `insmod` which do a
task of removing kernel module from kernel, opposite to its counterpart `insmod`
command.

```
rmmod hello_world
```

`rmmod` accepts an parameter that is not the file name of module, but the module
name registered in kernel. For our sample, the module name is same as the file
name of module except the `.ko` suffix.

Execution of `rmmod` command sometimes may failed. It is often due to that
resource monopolized by this module is still in busy.

Removal of kernel module includes invoking `m_exit()` function defined by
kernel writer. In this function, reousrces hold by kernel module must be freed
properly, such as memory allocated during the running time, or it will lost
the only chance to give it back and cause memory leakage.


## Test in QEMU guest

As mentioned in previous sections, samples in this book can be played and tuned
in a QEMU virtual machine, in which we set up a simple system based on busybox
and initramfs. For some Linux users, it seems very redundant of developing
modules in a guest machine, because all tests can be run and tested directly on
the host. But, due to the distinct difference between the kernel module and the
userspace program, developers can gain a lot from using virtual machine. The
first bonus of developing in a virtual guest is that system crash caused by
module flaws won't affect the state of host machine. Kernel panic is horrible.
It may cause deadly result to a system, such as file lost or system broken down.
Second, using virtual machine makes it very convenient to test a kernel module
in multiple kernels with different version. Third, for device driver developer,
sometime, there is a ready hardware on which a driver will be developed. For
this situation, write a simple virtual hardware for driver test is fairly
convenient.

In previous, we have set up a QEMU virtual machine, and mounted nfs in it for
file sharing between the host and guest. To test "hello world" module, follow
steps below:

1. On host machine, recompile module against kernel 5.10, which is the version
used in guest.

```
make -C ~/LDD_ROOT/kernels/linux-5.10.4 M=$(pwd) modules
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

To unload module, use `rmmod` command

```
rmmod hello_world
```

# ¶ The end


