# Loading module

So far, we eventually reach the final step of loading kernel module into kernel.
The tools used previously will be detailed in this section, including `insmod`,
`rmmod` and `modprobe`. Besides, some other useful tools will also be introduced
in this section, e.g. `modinfo` and `lsmod`. And, as a bonus, a brief talk about
how the kernel is loaded and unloaded will given for readers who curious about
the working mechanism behind these tools.

## Dependencies

Linux kernel module is a loadable file, like the dynamic libraries in user space
, will it be parsed and checked before the actually processing of linking.
Mismatch of kernel magic number or platform will fail the loading process and
report a simple error in terminal.

The first check is version dependency check. Have you remember that the command
used before in building kernel module? A path to kernel source tree is given
as a command line parameter of `-C` option, i.e `~/LDD_ROOT/kernels/linux-5.10.4`.
This path gives the desired kernel version that kernel will be compiled against.
Keep in mind that Linux kernel is an actively developing project, in which
functions are added, removed, modified frequently. So, for kernel modules which
is built against a specific version, it may encounter errors of undefined
symbols if it is loaded on a different host with incorrect version of kernel.
To that, version dependency is very important.

As a experiment, we build a "hello_world" module against Linux 5.10.4, which is
the default kernel version in this book, and then load it on your host. If the
host's kernel version is not "5.10.4", error will be reported during loading.

For my Ubuntu host shipped with kernel 5.4.0, the errors is:

```
# Module is compiled against kernel 5.10.4, but loaded on host with kernel 5.4.0
# insmod version is "kmod version 27"
insmod: ERROR: could not insert module hello_world.ko: Invalid module format
```

For those who are the first time encounter this error may got a little confusing.
Because the `Invalid module format` doesn't precisely describes what happened.
The specific description of this problem can be viewed in kernel's log by
using `dmesg` command.

```
hello_world: version magic '5.10.4 SMP mod_unload ' should be '5.4.0-59-generic SMP mod_unload '
```

Obviously, this log gives the real reason that breaks the loading process, i.e.
version mismatch between module and kernel of host.

During the linking process of building a module, a specific file in kernel
source is linked against in our `.ko` file, in which sufficient information
about the kernel the module was built for is given. The linker links this file
in and embeds these information in our `.ko` file simultaneously. These
information includes target kernel version, compiler version, and the settings
of a number of important configuration variables. Then, when loading a module
into kernel, these information is compared and tested for compatibility with
the running kernel.

Kernel's APIs and data structures continuously evolving in time by adding and
abandoning functions that no longer satisfy the design. And the cruel fact is
that modules ofter needs the ability of running on multiple kernels with
different versions. For example, same type graphic card can be installed on
different hosts on which different distro and different kernel are running.
Driver developer cannot assume the target kernel version which its custom will
install. It is hardware driver developer's duty to keep its driver be compatible
with different versions of kernel. To solve this, kernel module developers have
to be familiar with using of macros and #ifdef constructs to make your code
build properly. Linux kernel itself provides a lot of macros and functions for
managing and testing versions. The `linux/version.h` defines these macros and
functions, which is in turn included by `linux/module.h`. This book concentrates
only on kernel 5.10.4, and skips version tests in examples. But for a reference,
a brief introduction to these macros are listed below:



// TODO: Version Dependency and Platform Dependency

## Loading

// TODO: module parameters

`m_init()` function.

## unloading

`m_exit()` function.

## How it work

// TODO: How kernel load userspace module file into its memory space.

via init_module() and delete_module() system calls

# ¶ The end
