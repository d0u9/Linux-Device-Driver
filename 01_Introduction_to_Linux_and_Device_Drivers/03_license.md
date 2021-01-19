# License

Linux kernel is licensed under GPL 2.0. GPL is the short of GNU General Public
License, and the newest version is 3.0. Despite 3.0 version is released, the
Linux developers still stick to using GPL 2.0 as its License.

For a device driver developer, it is free to choose any license for your driver.
Linux kernel module (i.e. driver) requires a `MODULE_LICENSE()` tag which
provides sufficient information whether this module is free software or
proprietary for the kernel module loader and for user space tools.


However, if the kernel module is closed-source and distributes only in binary
cod, it is considered as a proprietary kernel module. A specific term, binary
bolb, is used to refer the kernel modules loaded into the kernel of an
open-source operating system. The linux community firmly rejects any binary-only
kernel modules. The rejection I mentioned here not means that closed-source
modules cannot be loaded into kernel, but the modules are not given any support
for Linux community.

Linus Torvalds hates binary-only modules, and he has made a strong statement on
this issue. It is THEIR duty to maintain closed-source modules, and the
community has little responsibility for answering any question about a
binary-only module.

However, there still are many binary-onle kernel modules and drivers. The most
notorious one is Nvidia's GPU driver, which has been blamed for years for its
closed-source. Also, a lot of hardware firmwares are closed-source, including
some wireless networking devices, BIOS and some RAID controllers.

Any kernel moduel which tags itself as "Proprietary" are constrainted to use
limited interfaces exported by kernel. The module loader refuses to link
proprietary modules against any function or variable that are exported with
EXPORT_SYMBOL_GPL(). And again, the kernel community won't provide any support
for binary-blob, and won't answer any question about the usage or interface
of such a module.


# ¶ The end
