# Linux-Device-Driver

Examples of Linux Device Drivers, currently for kernel 4.9.

# Before start

The examples in this repo are compiled against Linux Kernel 4.9. Other versions
of kernel are not tested.

Set `KERNELDIR` environment variable to the Linux kernel source dir, and export
it in your local shell.

```bash
export KERNELDIR=/path/to/kernel/source/
```

This environment variable is mainly used in Makefile to determine which the
source tree the modules are built against.

---

# License

Linux-Device-Driver by d0u9 is licensed under a
[GNU General Public License, version 2][1].

[1]: https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
