# Loading module

So far, we have eventually reached the final step of loading the module into
the kernel. Then, the tools used previously will be detailed in this section,
including `insmod`, `rmmod` and `modprobe`, and some other useful tools will
also be introduced in this section, e.g. `modinfo` and `lsmod`. In the end, we
will give a brief talk about the working mechanism of these tools for how
modules are loaded and unloaded to curious readers.

## Dependencies

Linux kernel module is a loadable file, parsed and checked before linking into
the kernel, like the dynamic libraries in user space. Mismatches in kernel version magic
or architecture will cause loading to fail and report an error in the
terminal.

The first check is the version dependency. Have you remembered the command used
before in building the kernel module? A path to the kernel source tree is given
as a `-C` option's parameter, i.e. `$LDD_ROOT/src/linux-stable`. This path
provides the desired kernel version that the module will be compiled against.
Keep in mind that the Linux kernel is an actively developing project in which
functions are added, removed, and modified frequently. So, a kernel module built
against a specific version may encounter errors of undefined symbols if it is
loaded on a different host with an incorrect version of the kernel. So to that,
version dependency is critical.

In this historical experiment, we built a "hello_world" module against Linux
5.10.4 and loaded it into a host running a different kernel. The tutorial now
targets Linux 7.2.y; the old versions below are retained to illustrate the error.

The Ubuntu host ran kernel 5.4.0. The error was:

```
# Module is compiled against kernel 5.10.4, but loaded on host with kernel 5.4.0
# insmod version is "kmod version 27"
insmod: ERROR: could not insert module hello_world.ko: Invalid module format
```

Those encountering this error for the first time may get a little confused
because the `Invalid module format` doesn't precisely describe what happened.
Use the `dmesg` command to get a more specific message about the error:

```
hello_world: version magic '5.10.4 SMP mod_unload ' should be '5.4.0-59-generic SMP mod_unload '
```

Obviously, the log gives the real reason that breaks the loading process, i.e.
version mismatch between the module and the host's kernel.

During module building, kbuild generates a `.mod.c` file and links its object
into the `.ko` file. Its `vermagic` metadata includes the target kernel release
and selected kernel configuration settings, not the compiler version. Then, when loading a module
into the kernel, this information is tested and compared for compatibility with
the running kernel.

Kernel's APIs and data structures are continuously evolving in time by adding
and abandoning functions that no longer satisfy the design. And the cruel fact
is that modules often need the ability to run on multiple kernels with different
versions. For example, Linux users can install graphic cards of the same type
on various Linux hosts on which different kernels run. Driver developers cannot
assume the target kernel version, which their customers install. It is the driver
developer's duty to keep the driver compatible with different kernel versions.
To solve this, module developers have to be familiar with using `#ifdef` macros
to build the code properly on different hosts. Linux kernel itself provides a
lot of macros and functions for managing and testing versions. The
`linux/version.h` header defines `LINUX_VERSION_CODE` and `KERNEL_VERSION()`
and should be included explicitly when using them. This book concentrates on one kernel
version and skips version tests in examples. But for a reference, a brief
introduction to these macros is listed below:


TODO: Version Dependency and Platform Dependency

## Loading

TODO: module parameters

`m_init()` function.

## Unloading

`m_exit()` function.

## How it works

TODO: How the kernel loads userspace module file into its memory space.

Via `finit_module()` or `init_module()` for loading, and `delete_module()`
for unloading.

# ¶ The end
