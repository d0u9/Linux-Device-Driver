# The Linux Kernel

According to the official page of Linux kernel:

> Linux is a clone of the operating system Unix, written from scratch by
> Linus Torvalds with assistance from a loosely-knit team of hackers across
> the Net. It aims towards POSIX and Single UNIX Specification compliance.

Linux is great. It was created by Linus Torvalds in 1991, and then be richen by
peopel from all around the world. It is free (free speach, not free beer) and
open source, which mean anyone can download and play it under the GPL V2
licnese. Linux is an UNIX-like operating system; it provides standard
programming interface to its users.

Linux is fast and lightweight. Due to its sophisticate design and the monolithic
kernel, Linux is super fast and small which makes it the first candidatae for
devices that has poor hardware resources. It can be tailored to cater the
essential need of task and to keep as much as small in size. Benefiting from
this, Linux nowdays is running on various devices; it spans from mobile phone to
supercomputer.

Nowdays, Linux is still envolving. New features are added in and new hardwares
are supported continuously. It is still in activately developing. More and more
companies are involved, including Intel, Google, Microsoft, Huawei, etc.

For the word "Linux", genericlly it includes various distributions of Linux,
e.g Ubuntu, Debian, Redhat. But more specificly, Linux only means the kernel.
Linux distributions are made from a software collection that is based upon the
Linux kernel, which are usually shipped with a package management system. Users
download and install their faviourite distribution to expericen different
flavours of Linux. But the cores of the system are the same. However, in this
book, we don't use any specific Linux distribution. Instead, a initramfs is
built and is run direclty in QEMU guest.

## Architecture

Linux is a monolithic kernel which means the entire kernel is working in the
same address space. The whole system is built as a single binary file, and then
can be loaded and run on host. Diffferent parts of the kernel conmunicate with
each other direlty via memroy; makes linux exceling in performance.

The monolithc kernel can be splited into serval main parts. But different from


## Versioning

## Loadable Modules

# ¶ The end
