# Role of Device Driver

As mentioned before, device management is a complex subsystem in the kernel,
and almost all operations are finally mapped to devices. However, the interfaces
provided by devices are inconsistent; they vary from device to device and change
frequently when a new hardware protocol is released.

How could the kernel exactly know what operations it can take on a specific
device? The solution is the device driver. Device drivers are responsible for
bridging the hardware to the kernel. On the kernel side, it provides a set of
well-defined interfaces to which any operation the device had can be nicely
mapped. On the hardware side, no matter what protocol it takes on, the control
commands are finally instanced as electronic signals passed by the buses from
CPU pins to hardware pins. The device driver translates these abstract
operations into software code and controls how the CPU writes data to its pins
and vice versa.

The topology of connections that hardware attached to each other is tree-like.
For the x86 platform with the traditional southbridge and northbridge chipset,
the northbridge chip connects to the CPU via the front-side bus, and the
southbridge chip connects to the northbridge chip via the internal bus. Memory
and other high-speed devices, such as graphic card, is attached to northbridge
directly. However, some low-speed devices, e.g., PCI bus, ISA BUS, or serial
port, are connected to southbridge.

The modern x86 computer no longer takes on these two chipsets. Instead, for
CPUs later than 2011, the functions of the northbridge chip are integrated into
the CPU, and the southbridge chip has been renamed "Platform Controller HUB"
for Intel or "Fusion controller HUB" for AMD.

Even with the integration of northbridge, the topology does still exist.
Devices are connected to the CPU via different buses, and these buses are
hierarchically interconnected. For example, USB slots provided by a USB-PCIe
card don't directly communicate with the CPU. Instead, the data is first
converted from USB protocol to PCIe protocol and then delivered to CPU or main
memory based on whether DMA has opted.

Linux internally maintained a complex data structure to describe this device
tree. All devices and their properties are exported as a memory-based virtual
filesystem, i.e., the `sysfs` filesystem. A later chapter will discuss the
`sysfs` filesystem in a deep dive.

In a word, the device driver is a piece of code that can be compiled inside or
outside the kernel source tree and be loaded during boot time or at runtime,
running in kernel space and bridging the gap between the kernel and a specific
device.

------ TODO: more introduction to Rust in Kernel

Historically, Linux modules are written in C as the other parts of the Linux
Kernel. However, due to the natural deficiencies of the C language, especially
in memory safety, some hackers are starting to use Rust, a programming language
initially developed by Mozilla, as the second programming language in Linux
Kernel. At the time I am writing this book, Rust support in Linux is still an
infant and is not accepted by upstream. However, some prestigious maintainers
perform an optimistic attitude toward Rust that Linus Torvalds said "I don't
hate it" in the LKML.

Until now, Jun 25, 2021, there is still much work have to do in integrating Rust
in the Kernel. Currently, the most challenging job is refactoring the memory
allocator.

Another repo of mine, [Linux-Device-Driver-Rust], details how Rust is integrated into the Kernel.

# ¶ The end

[Linux-Device-Driver-Rust]: https://github.com/d0u9/Linux-Device-Driver-Rust]
