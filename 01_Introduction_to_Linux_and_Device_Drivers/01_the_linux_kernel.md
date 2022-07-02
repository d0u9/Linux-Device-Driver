# The Linux Kernel

According to the official page of the Linux kernel:

> Linux is a clone of the operating system Unix, written from scratch by
> Linus Torvalds with assistance from a loosely-knit team of hackers across
> the Net. It aims towards POSIX and Single UNIX Specification compliance.

Linux is great. It was created by Linus Torvalds in 1991 and then was richen by
people from all around the world. It is free (free speech, not free beer) and
open source, which means anyone can download and play it under the GPL V2
license. Linux is a UNIX-like operating system; it provides a standard 
programming interface to its users.

Linux is fast and lightweight. Due to its sophisticated design and the 
monolithic kernel, Linux is super fast and small, making it the first 
candidate for platforms with poor hardware resources. It can be tailored for
tasks' essential needs and still keep slim in size. Benefiting from these,
Linux nowadays is running on various devices; it spans from mobile phones to
supercomputers.

Linux is still evolving by continuously supporting new features and new
hardware. It is still in the actively developing phase. More and more companies
are involved, including Intel, Google, Microsoft, Huawei, etc.

Generally speaking, the word "Linux" stands for various distributions of the
Linux operating system, e.g., Ubuntu, Debian, and Redhat. But more specifically,
Linux only means the kernel. A Linux distribution contains a collection of
software built upon the Linux kernel and is usually shipped with a package 
management system for managing software installation and updating. Users can
download and install their favorite distribution to experience different
flavors of Linux. But the cores of these distributions are all the same. In
this book, we don't use any specific Linux distribution. Instead, an initramfs
is built and is run directly in QEMU as a guest.








## Architecture

Linux is a monolithic kernel which means the entire kernel is working in the
same address space. The whole system is built as a single binary file, and then
can be loaded and run on the host. Different parts of the kernel communicate with
each other directly via memory; making Linux excelling in performance.

The monolithic kernel can be split into several main parts. But different from 
the micro kernel, these parts share a common address space and exchange data
directly via main memory.

Linux can be split roughly into two parts, the userspace and kernelspace.
The software, including database, web server, render engine or games, are
running in userspace. The main and basic unit in userspace is "process". Ideally,
processes in a system run concurrently; they request resources from the system
directly and be scheduled by the kernel. Each process has its system resource,
e.g. network connectivity, memory, CPU time. These resources, from a process's 
view, are monopolistic. However, the feeling of monopoly is just an illusion,
since the actual underlaying resources are a few. The Linux kernel is
responsible for splitting and scheduling different real hardware resources to a
set of processes.

In the kernelspace, on the other side, the logic is hugely different. The
whole Linux kernel is a unity, even the term "thread" can be seen in the
kernel's source code. The code runs in kernelspace are divided into two
distinctive paths, the process context and the interrupt context. It is very
important to notice the context in which the logic is running, since the wrong
code may cause the whole system to hang.

However, from a software architecture's perspective, the Linux kernel can still
be split into several pieces, which are relatively and logically independent.

**Process management**

Process is an essential concept for operating systems. The process management
unit is in charge of managing processes both in kernelspace and userspace. 
It is responsible for creating and destroying processes and responding to the
requests of various resources made by processes. The userspace processes are
the outer-most layer of the whole operating system; any communication to the
outside world or other processes, i.e., IPC, the inter-process communication,
is handled by the kernel. Most important the role the process management unit
take is scheduling. Scheduling is a combination of policy and mechanism to
decide the next process utilize the CPU, and run it on the CPU. Different
schedulers have different strategies to determine how processes run after each
other.

**Memory management**

Other than the CPU time, one of the most precious resources in a computer is
memory. Memory stands at the critical cross of a program's execution and makes
it one of the most complex parts of the Linux kernel. It provides a set of
function APIs for other parts to interact with it; request and free single or
chuck of memory ranges for exclusive or shared use. Due to its essential role
in the kernel, any policy dealing with it is crucial to the system's
performance. Not only the software but also the hardware is evolving in 
boosting memory performance. Today, the most widely used technology in memory
management is virtual addressing which gives a virtual but large memory address
space to a process.

**Filesystems**

One of the fascinating design features in Unix and Unix-like systems is
"everything is a file". Linux borrows this philosophy with no doubt. It builds
an abstraction layer called VFS between the userspace and kernelspace. This VFS
layer, of course, supports the regular filesystem based on hard disks, e.g.,
ext4 filesystem or XFS filesystem. However, this layer is so generic that
almost any resource in a machine can be described as a file. Network
connectivity, keyboard, or even a graphics card can fit in this abstraction
layer so that operations to these devices are taken directly on the file
descriptors instead of some particular device abstractions.

**Device control**

The lifetime of a process always involves device operations. Since the 
process is an instance of a program running on CPU, reading program binary from 
hard disk or other persistent storage is usually the device involved. With the
exception of the processor, memory, and a very few other entities, the actions
a process has taken almost all eventually terminate with physical operations,
e.g., writing results to a hard disk, sending data via a network, fetching the
coordinates to which the mouse is currently pointing. Hardware devices are
designed and manufactured by hardware companies; the operations hardware can
act are specific to that device. Even though a lot of hardware standards exist,
such as the PCIe, the low-level hardware interfaces vary a lot from device to
device. It is the driver's role to map these device differences to a set of
pre-defined interfaces the kernel wants, which is what this book aims to.

**Networking**

Networking is a way that a computer communicates with the outside. For ingress
direction, it reads data from NIC (network interface card) and then distributes
them to different processes. Egress data from various processes are packeted
and then sent via NIC. Due to its multiplexer role, networking code works
asynchronously to processes and is in charge of delivering data packets across
programs and NICs. The rules of packing and unpacking data from and to processes
are called protocols. Networking utilizes a lot of different protocols in one
or more protocol layers. The most outstanding one is TCP/IP, which is vastly
used today. Traditionally, protocols are implemented inside the kernel as a
part of kernel code. But the ability to implement a protocol in userspace is
valid for new kernel versions.

## Versioning

Before kernel version 3.0, Linux opts for a regular versioning scheme in which
the version number is divided into four parts with dots as delimiters, i.e.,
**w.x.y.z**. The 'w' is the major version, 'x' is the minor version, 'y' is the
revision, and 'z' is the patch number. Under this versioning scheme, Linux
kernels distinguish between stable and development in a simple manner. The even
or odd minor version points to stable or development kernel separately.
Sometimes, the fourth number is used as a mark for differentiating the stable
or the development version of a kernel; only the stable kernel has the fourth
number.

Linux has used this formula scheme for a long time until version 3.0 is
incoming. Linus, the creator and curator of Linux, shifted to a new scheme
based on timeline that abandoned the odd and even number versioning scheme.
The new versioning scheme, according to Linus' note, upgrades major number each
time when running out its fingers and toes to count.

The new scheme takes the first two numbers as a whole to indicate the major
version. In each circle, a new merge window is opened for accepting patches
prepared for the next release. During the period of the merge window opening,
only code deemed to be sufficiently stable is merged into the mainline kernel.
This window opens for approximately two weeks. At the end of the window, Linus
Torvalds will declare that the window is closed and release the first of the
"rc" kernels.

"rc" is the abbreviation of "release candidate". rc version is distinguished 
with a suffix of "-rcN", which indicates the "Nth" of the candidate. During the
rolling of "rc" versions, only bugfix patches are accepted. New features or new
drivers are not welcome at this time. Linus is responsible for rolling these
"rc" versions until the point that the code is believed to be stable enough.
Then, a new stable kernel is spawned, and a new circle is started.

If the contributor luckily misses the merge window, the best thing he may do is
wait for the next circle. However, this rule is not unbreakable; emergency
fixes or patches to fatal security problems are permitted to merge into even
the merge window is closed.

## Loadable Modules

Even though Linux is "monolithic", it still supports dynamic insertion and
removal of code at runtime without recompiling or rebooting the kernel. A
loadable module is a special type of binary file, which is usually named with
the suffix ".ko" (abbreviation of Kernel object"). Internally, the loadable
module uses ELF relocatable binary format. It works almost the same as a dynamic
link library in userspace, which can be dynamically loaded and unloaded into
memory on demand. However, due to the particularity of the kernel, Linux
implements its own mechanism to insert and remove such loadable files into
kernel space.

How to load ko files resident in userspace into kernelspace and arrange its
memory layout in the kernel is out of the scope of this book. The only thing
the developer has to keep in mind is that the code written for kernel modules
is finally loaded and run in kernel space. It means:

1. The flaws in a module can crash the whole system.
2. Have high permission to access almost any resource in the kernel, 
    which may cause severe security issues.
3. Any userspace standard libraries cannot be referenced in the module,
    e.g., the glibc.

It is the developer's duty to keep the module consistent and insistent that the
modules' upgrade won't break the APIs. Bugs, too, are unbearable in kernel
modules. As mentioned before, modules run in kernelspace, any fatal bug can
cause a system halt, such as deference an invalid pointer or deadlock.

Linux device driver is a subset of loadable modules, usually. Almost all device
drivers can be compiled as a loadable module and loaded dynamically on demand.
However, aside from driving devices, the loadable modules' ability is far
beyond. For example, filesystem support and network traffic control policy can
be implemented as a loadable module.

# ¶ The end

---

TODO:

1. monolithic kernel, directly function call instead of IPC. IPC is expensive
both in reousrce and time.

