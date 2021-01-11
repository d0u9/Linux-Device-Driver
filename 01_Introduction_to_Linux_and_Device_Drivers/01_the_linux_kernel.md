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
can be loaded and run on host. Different parts of the kernel communicate with
each other directly via memory; makes Linux excelling in performance.

The monolithic kernel can be split into several main parts. But different from
micro kernel, these parts sharing a common address space and exchanging data
directly via main memory.

The Linux can be split roughly into two parts, the userspace and kernelspace.
The softwares, include database, web server, render engine or games, are
running in userspace. The main and basic unit in userspace is process. Ideally,
processes in a system run concurrently; they request resources from the system
directly and be scheduled by the kernel. Each process has its system resource,
e.g. network connectivity, memory, CPU time. These resources, from processes'
view, are monopolistic. However, the feeling of monopoly is just an illusion,
since the actual underlaying resources are a few. The Linux kernel is
responsible for splitting and scheduling different real hardware resource to a
set of process.

In the kernelspace, on the other side, the logical is hugely different. The
whole Linux kernel is an unity, even the term of thread can be seen in the
kernel's source code. The code runs in kernelspace are divided into two
distinctive paths, the process context and the interrupt context. It is very
important to noticing the context which the logic is running on, since the wrong
code may cause the system hang.

However, from a software architecture's perspective, Linux can still be split
into several pieces, which are relatively logical independent.

**Process management**

Process is an important concept to operating system. Process management unit is
in charge of manage processes both in kernelspace and userspace. Creating and
destroying processes and fulfil the requests of various resources made by
process. The userspace processes are the outer-most layer of the whole operating
system; any communication to the outside world or to other processes, i.e IPC,
inter-process communication, are handled by kernel. Most important the role the
process management unit take is scheduling. Scheduling is a combination of
policy and mechanism to decide which the next process to utilize the CPU and to
run it on CPU. Different scheduler has different strategies to determine the
sequence that how processes run after each other.

**Memory management**

Other than CPU, the most precious resource in a computer is memory. Memory
stands at the critical cross of program's execution, and makes it one of the
most complex parts in kernel. It provides a set of function APIs for other
parts to interact with it; request and free single or chuck of memory ranges
for exclusive or sharing use. Due to its important role in the kernel, any
policy used to deal with it is a critial one for system performance. Not only
the software, but also the hardware is evolving in boosting memory performance.
Today, the most widely used technology used in memory management is virtual
addressing which give a virtual but large memory address space to process.

**Filesystems**

One of the most fascinating design features in Unix and Unix-like system is
"everything is a file". Linux borrows this design with no doubt. Linux builds
and abstraction layer of VFS in between the userspace and kernelspace. This VFS
layer of course supports the normal filesystem that based on hard disks, e.g
ext4 filesystem or XFS filesystem. However, It is so generic that almost any
resource in a machine can be described as a file. Network connectivity,
keyboard or even a graphic card can fit in this abstraction layer, that
options to theses devices are impose directly on device file's descriptor.

**Device control**

During the life time a process, devices operations are always involved. Since
the process is a instance of program which is running on CPU, reading program
binary from hard disk or other persistent storage are usually device involving.
The actions a process taken almost eventually terminated with physical
operations, e.g write result to hard disk, send data via network, fetching the
coordinates the mouse currently pointing to, with the exception of the processor,
memory, and a very few other entities. Hardware devices are designed and
manufactured by hardware companies; the operations a hardware can act are
specifically to that device. Even a lot of hardware standards exist, such as
PCIe, the low level hardware interfaces vary a lot from device to device.
It is drvier's role to map these differences of device to a pre-defined
interfaces the kernel wanted, and that is what this book aims to.

**Networking**

Networking is a way that computer communicates with outside. For ingress
direction, it reads data from NIC (network interface card) and then distribute
them to different process. For egress data from various processes, it packets
them and then sends via NIC. Due to is multiplexer role, networking code works
asynchronously to process, and in charge of delivering data packets across
program and NIC. The rules of how to pack and unpack data from and to processes
are called protocols. A lot of different protocols are utilized in networking
in one or more protocol layers. In them, the most outstanding one is TCP/IP,
which is vastly used today. Traditionally, protocols are implemented inside
kernel code and a part of kernel code. But the ability of implementing a
protocol in userspace is true for new kernels.

## Versioning

## Loadable Modules

# ¶ The end

---

TODO:

1. monolithic kernel, directly function call instead of IPC. IPC is expensive
both in reousrce and time.

