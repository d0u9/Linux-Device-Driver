# Role of Device Driver

As mentioned before, device management is a huge subsystem in kernel, and almost
every operation is finally mapped to devices. However, the interface provided
by devices are inconsistent, they vary from device to device and change
frequently when new hardware protocol is released.

How could the kernel know exactly what operations it can take on a specific
device? The solution is device driver. Device drivers are responsible for
bridging the hardware to kernel. On the kernel side, it provides a set of
well-defined interface to which any operation to a devices can be nicely mapped.
On the hardware side, no matter what protocol it takes on, the control commands
are finally instanced as electronic signals which are passed by the buses from
CPU pins to hardware pins. Device drivers translate this abstract software code,
and control how the CPU writes data to its pins, and vice versa.

The topology of hardwares attached to each other is tree-like. For x86 computers
that has the traditional southbridge and northbridge chipset, the northbridge
chip connects to CPU via front-side bus, and the southbridge chip connects to
northbridge chip via internal bus. Memory and other high-speed devices, such as
graphic card, is attached to northbridge directly. However for some low-speed
devices, e.g. PCI bus, ISA BUS or serial port, they are attached to southbridge.

The modern x86 computer has no longer take on these two chipsets. Instead, for
CPUs later that 2011, the functions of northbridge chip is integrated into CPU,
and the southbridge chip is renamed as Platform Controller HUB for intel or
Fusion controller HUB for AMD.

Even the integration of northbridge, the topology does still exist. Devices are
connected to cpu via different buses, and these buses are hierarchically
inter-connected. For example, USB slots provided by USB-PCIe card doesn't
directly communicates with CPU. Instead, the data is firstly converted from
USB protocol to PCIe protocol, and then delivered to CPU or main memory based on
if DMA is opted.

Linux internally maintained a complex data structure to describe this device
tree. All devices, along with their properties, are exported as a memory-based
virtual filesystem, i.e. sysfs. A deep dive into sysfs will be found in later
chapter.

In a word, device driver is a pieces of code, which can be compiled inside or
outside the kernel and can be loaded during boot time or at runtime, running in
kernel space and bridging the gap between the kernel and specific devices.

------ TODO: more introduction to Rust in Kernel

Historically, Linux modules are written in C as the other part of Linux Kernel.
However, due to the deficiencies of C language, especially in memory safety,
some hackers start to using Rust, a programming language originally developed
by Mozilla, as the second programming language in Linux Kernel. At the time
I am writing this book, Rust in Linux is still an infant, and is not fully
accepted by upstream, even some prestigious maintainers, include Linus Torvalds
who said "I don't hate it" on the topic of Rust in Linux in the LKML, performs
optimistic attitude.

Until now, Jun 25, 2021, there is still a lot of work have to do in integrating
Rust in the Kernel. Currently, the most challenge work is refatoring memory
alloctor.

There is another repo of mine, [Linux-Device-Driver-Rust], details how Rust
is integrated in the Kernel.

[Linux-Device-Driver-Rust]: https://github.com/d0u9/Linux-Device-Driver-Rust]

# ¶ The end
