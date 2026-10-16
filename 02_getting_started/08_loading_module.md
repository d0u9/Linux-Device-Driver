# Loading module

So far, we have eventually reached the final step of loading the module into
the kernel. Then, the tools used previously will be detailed in this section,
including `insmod`, `rmmod` and `modprobe`, and some other useful tools will
also be introduced in this section, e.g. `modinfo` and `lsmod`. In the end, we
will give a brief talk about the working mechanism of these tools for how
modules are loaded and unloaded to curious readers.

## Dependencies

Linux kernel module is a loadable file, parsed and checked before linking into
the kernel, like the dynamic libraries in user space. Mismatches of kernel magic
number or platform will fail the loading process and report an error in the
terminal.

The first check is the version dependency. Have you remembered the command used
before in building the kernel module? A path to the kernel source tree is given
as a `-C` option's parameter, i.e. `~/LDD_ROOT/kernels/linux-5.10.4`. This path
provides the desired kernel version that the module will be compiled against.
Keep in mind that the Linux kernel is an actively developing project in which
functions are added, removed, and modified frequently. So, a kernel module built
against a specific version may encounter errors of undefined symbols if it is
loaded on a different host with an incorrect version of the kernel. So to that,
version dependency is critical.

As an experiment, we build a "hello_world" module against Linux 5.10.4, which
is the default kernel version in this book, and then load it into the host. If
the host's kernel version is not "5.10.4", the loader will report an error
during load time.

My Ubuntu host shipped with kernel 5.4.0, the error is:

```
# Module is compiled against kernel 5.10.4, but loaded on host with kernel 5.4.0
# insmod version is "kmod version 27"
insmod: ERROR: could not insert module hello_world.ko: Invalid module format
```

Those who are the first time encounter this error may get a little confusing
because the `Invalid module format` doesn't precisely describe what happened.
Using `dmesg` command to get a more specific message about the error:

```
hello_world: version magic '5.10.4 SMP mod_unload ' should be '5.4.0-59-generic SMP mod_unload '
```

Obviously, the log gives the real reason that breaks the loading process, i.e.
version mismatch between the module and the host's kernel.

During the linking phase of module building, a particular file in the kernel
source is linked. This file provides sufficient information about the kernel
against which the module is building. The linker links this file and
simultaneously embeds this information in the generated `.ko` file. This
information includes the target kernel version, compiler version, and the
settings of many essential configuration variables. Then, when loading a module
into the kernel, this information is tested and compared for compatibility with
the running kernel.

Kernel's APIs and data structures are continuously evolving in time by adding
and abandoning functions that no longer satisfy the design. And the cruel fact
is that modules ofter need the ability to run on multiple kernels with different
versions. For example, Linux users can install graphic cards of the same type
on various Linux hosts on which different kernels run. Driver developers cannot
assume the target kernel version, which their customs install. It is the driver
developer's duty to keep the driver compatible with different kernel versions.
To solve this, module developers have to be familiar with using `#ifdef` macros
to build the code properly on different hosts. Linux kernel itself provides a
lot of macros and functions for managing and testing versions. The
`linux/version.h` header file defines these macros and functions and is then
included by `linux/module.h`. This book concentrates only on a particular kernel
version and skips version tests in examples. But for a reference, a brief
introduction to these macros is listed below:


// TODO: Version Dependency and Platform Dependency

## Loading

// TODO: module parameters

`m_init()` function.

## unloading

`m_exit()` function.

## How it work

// TODO: How the kernel loads userspace module file into its memory space.

via init_module() and delete_module() system calls

# ¶ The end
