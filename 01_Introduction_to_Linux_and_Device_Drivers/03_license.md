# License

Linux kernel is licensed under GPL 2.0. GPL shorts for the "GNU General Public
License", and the latest version is 3.0. Although the 3.0 version has been
released for years, the Linux developers still stick to using GPL 2.0 as
Linux's License.

For a device driver developer, it is free to choose any license for your driver.
The `MODULE_LICENSE()` tag, which is compulsory for Linux kernel modules,
contains sufficient information about the modules' license. This tag is used by
the module loader and userspace tools to determine whether the module is free
software or proprietary.

If the kernel module is closed-source and distributed only in binary code, it
is considered a proprietary kernel module. A specific term, "binary blob", is
used to refer to the kernel modules loaded into the kernel of an open-source
operating system. The Linux community firmly rejects any binary-only kernel
modules. The rejection I mentioned here does not mean that the module loader
cannot load closed-source modules, but the modules are not given any support
from the Linux community.

Linus Torvalds hates binary-only modules, and he has made a strong statement on
this issue. "It is THEIR duty to maintain closed-source modules, and the
community has little responsibility for answering any question about a
binary-only module", he said.

However, there still are many binary-only kernel modules and drivers. The most
notorious one is Nvidia's GPU driver, which has been blamed for years for its
closed-source. Also, many hardware firmware are closed-source, including some
wireless networking devices, BIOS, and some RAID controllers.

Any kernel module which tags itself as "Proprietary" is constrained to use
limited interfaces exported by the Kernel. The module loader refuses to link
proprietary modules against any function or variable exported with
`EXPORT_SYMBOL_GPL()` macro. And again, the kernel community won't provide any
support for binary-blob and won't answer any questions about the usage or
interface of such a module.

Note: Nvidia has provided an open-source GPU driver on Github on May 11, 2022.

# ¶ The end
