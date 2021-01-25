# Compiling

We have briefly talked a simple "hello world" example in previous chapter, and
have compiled it into a module file which can by be loaded into kernel by using
`insmod` or `modprobe` command in user space. The compiling process looks very
easy with a two lines Makefile; just like any other C program in user space.
In fact, however, the way of using this Makefile is not simple; A sequence of
parameters and options are passed in. A quick glace over previous make command
may confusing its user that our own Makefile seems not be invoked at all. That
is the difference of compiling kernel module and normal C program in user space,
and we will talk more about it in this chapter.

Building kernel and kernel modules depends on few libraries and tools. The most
import tools that is essential to compiling kernel source is `make` and `gcc`.
Both tools are are GNU software, which means that they are free and opensource.

Linux kernel itself is a large project contains millions lines of code. Besides,
Linux is a monolithic kernel that each components must be finally linked into
one single binary. To make things worse, Linux can be tailored and can run on
different hardwares. All these reasons make Linux's build system being a huge
monster that it can be configured by enabling or disabling some parts and
only compile components that are touched during configuration.

Linux takes it own building system, named kbuild system, to handle all these
problems. It is a large and complex build system depends on a few of scripts and
tools. Most Makefiles within the kernel are kbuild Makefiles that use the
kbuild infrastructure. Actually, the Makefile we used in the "hello world"
example is a kbuild file which takes on rules of Makefile due to that kbuild
is based on make. For newer kernel version, the preferred name for the kbuild
files are 'Makefile' but 'Kbuild' can be used and if both a 'Makefile' and a
'Kbuild' file exists, then the 'Kbuild' file will be used.

In modern days, there are many tools for building a project such as "CMake" or
"bazel". However, Linux kernel consistent on using make tool as a basement of
its building system. Shifting from a building system to another is expensive
and unnecessary, since make is simple and powerful enough.

kbuild system has a set of definitions and rules that hugely expanded the
ability of makefile while reducing the complexity of configuration. There are
lots of docs in Kernel's documentation folder, which details these rules and
some details. However, for kernel module developers, a very small subset of
knowledge is needed; just as simple as a two lines makefile is enough as you
have tried before.

## Before we start

Linux kernel modules are ELF loadable files that can be loaded into kernel. So,
it depends on symbols exported by kernel. Like any other C program, to use
functions and variables that are exported by another lib, it must include header
files in which functions and variables are declared. To that, before building
external modules, you must have as a prebuilt kernel vailable that contains the
configurations and header files used in the build. In our example, a complete
source tree is used to be compiled against. For interested readers who want to
run modules in their favourite linux distribution, consult manual for how to
install header files. Almost every linux distribution provides these kernel
headers as a package in their own package management system, such as yum or apt.

What we talk before is how to build a external module. For kernel modules
which are accepted by community (or just for some other purpose), they may be
added in source tree and can be compiled within kernel or a standalone module.
To add your own kernel module in source tree, things are pretty similar to
building it as an external module. In kernel there are many modules which can be
both compiled as a module or within kernel; taking these modules as a reference 
is a good starting point to questions of how to add your module into kernel
tree.

## Makefile for kernel module
// Need symbols exported by kernel, a source tree is essential.


# ¶ The end
