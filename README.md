# Linux-Device-Driver

Examples of Linux Device Drivers, currently for kernel 5.10.

# Before start

The examples in this repo are compiled against Linux Kernel 5.10. Other versions
of the Kernel are not tested.

Set `KERNELDIR` environment variable to the Linux kernel source dir, and export 
it to your local shell.

```bash
export KERNELDIR=/path/to/kernel/source/
```

This environment variable is mainly used in Makefile to determine which kernel
source tree the drivers are built against.

# Introduction

This repo demonstrates basic examples of Linux Device Driver(LDD) for
the purpose of studying and sharing.

Some of the examples in this repo are excepted from the book of 
LDD3(Linux Device Drivers, Third Edition), and the other examples are written
by myself in necessary to illustrate some important data structures or details.

LDD3 is a great book, which is authored by Jonathan Corbet, Alessandro Rubini,
and Greg Kroah-Hartman, I give my sincere appreciation to all these authors
for their great work.

LDD3 is available online, and you may request a copy of the PDF from
[https://lwn.net/Kernel/LDD3/]. It is still an excellent book to reference for
the development of Linux device module. However, due to its old age, first
published in 2005, some of the APIs and data structures have drastically
changed after its first publication.

Compare to the Kernel 2.6.10, which is the base of the LDD3 book, I rewrite all 
the examples for a brand new Kernel version 5.10, released in December 2020.
Besides the kernel version, the hardware also has evolved a lot in recent years.
The parallel port, which is used as an example in I/O operations in LDD3, can be
barely found in any computer now. I moved these examples to a QEMU(A generic
and open source machine emulator and virtualizer) virtual machine, in which 
a virtual parallel port device is implemented for convenient debugging.

This is the opening of my whole project, I hope you will enjoy in the tour
of reading my book.

Thanks a lot.

Sincerely, Douglas.

---

# License

Linux-Device-Driver by d0u9 is licensed under a
[GNU General Public License, version 2][1].

---

### ¶ The end


[1]: https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
[https://lwn.net/Kernel/LDD3/]: https://lwn.net/Kernel/LDD3/
