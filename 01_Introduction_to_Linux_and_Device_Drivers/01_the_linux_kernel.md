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

Before kernel version 3.0, Linux opts a regular versioning scheme that the
version number are divied into four parts with dots as delimiters, i.e.
**w.x.y.z**. w is the major version, x is the minor version, y is the
revision and z is patch number. Under this versioning scheme, Linux kernels
distinguish between stable and development kernel with a simple manner. The
even or odd minor version points to stable or development kernel separately.
Sometimes, the fourth number is used as a mark for differentiating stable or
development version of kernel; only the stable kernel has the forth number.

For a long time, Linux uses this formula scheme until version 3.0 is in coming.
Linus, the creator and curator of Linux, utilize a new scheme base on timeline
that the odd and even number versioning scheme is abandoned. The new versioning
scheme, according to Linus' note, upgrades major number each time when running
out its fingers and toes to count.

The new scheme takes the first two numbers as a whole to indicates the major
version. Each circle a new merge-window is opened for accepting patches prepared
for next release. During the period of merge-window opening, only code is deemed
to be sufficiently stable is merge into the mainline kernel. This window opens
for approximately two weeks. At the end of the window, Linus Torvalds will
declare that the window is closed and release the first of the "rc" kernels.

"rc" is the abbreviation of release candidate. rc version is distinguish with
suffix of "-rcN", which indicates the "Nth" of the candidate. During the roll of
"rc" versions, only bug fix patches are accepted. New features or new drivers
is not welcome at this time. Linus is responsible for rolling this "rc" versions
until a point that the code is believed stable enough. Then, a new stable
kernel is spawned, a new circle is started.

If contributor luckily misses the merge-window, the best thing he may do is
waiting for next circle. However, this rule is not unbreakable, emergency fixes
or patched to fatal securities problem is permitted to merge into even the
merge-window is closed.

## Loadable Modules

Even Linux is "monolithic", it still supports dynamic insertion and removal of
code at runtime without re-compiling or rebooting the kernel. Loadable modules
is type of special binary file, which is usually named with suffix ".ko" (
abbreviation of Kernel object"). Internally, loadable modules uses ELF
relocatable binary format. It works almost the same as dynamic link library in
userspace, which can be dynamically loaded and unloaded into memory on demand.
However, due to the particularity of kernel, Linux implements its own mechanism
to insert and remove such loadable files into kernel space.

How to load ko files resident in userspace into kernelspace, and arrange its
memory layout in kernel is out of the scope of this book. The only thing the
developer has to keep in mind is that the code written for kernel modules is
finally loaded and run in kernel space. It means (1) the flaws in module can 
crash the whole system; (2) Have high permission to access almost any resource
in kernel, which maybe cause severe security issues; (3) No any usersapce
standard libraries can be referenced in module, e.g. glibc.

It is developer's duty to keep module consistent and insistent for that upgrade
of modules won't break the APIs. Bugs, too, is unbearable in kernel modules. As
mentioned before, modules run in kernelspace, any fatal bug can cause system
halt, such as deference an invalid pointer or deadlock.

Linux device driver is subset of loadable modules, usually. Actually, almost all
device drivers can be compiled as a loadable module, and loaded dynamically on
demand. However, aside from driving devices, the ability loadable modues can
have is far beyond. Filesystem support, network traffic control policy can
be implemented as a loadable module as well.

# ¶ The end

---

TODO:

1. monolithic kernel, directly function call instead of IPC. IPC is expensive
both in reousrce and time.

