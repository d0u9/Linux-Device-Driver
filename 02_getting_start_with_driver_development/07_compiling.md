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

What we have talked before is how to build an external module. For kernel
modules which are accepted by community (or just for some other purpose), they
may be added in source tree and can be compiled within kernel or a standalone
module. To add your own kernel module in source tree, things are pretty similar
to building it as an external module. In kernel there are many modules which
can be both compiled as a module or within kernel; taking these modules as a
reference is a good starting point to questions of how to add your module into
kernel tree.

We now have our module project and kernel source tree in hand, but don't start
press "make" button in hurry. The last check before flight is GCC version.
Insufficient GCC version is the potential root of various problems which are
mysterious. Linux Kernel heavily depends on GCC, which means mismatch of GCC
version and kernel version can lead to errors during compiling time. The minimal
required tools to compile Linux kernel and kernel modules are listed in kernel's
documentation. Read this list in Documentation/Changes in the kernel
documentation directory and check if the tools you are preparing to use is
sufficient are always best practices.

## Makefile for kernel module

We have talked so much about the prerequisites before we going any further, it
is time for talking about "real" things about building kernel modules. The first
thing need to be done is composing makefile. As mentioned before, the makefile
used to build our module project is actually a kbuild script, which uses make
as its basement tool for managing file dependency. kbuild has a lot of rules
, variables and definitions internally which are solely to Linux kernel. For
building a kernel module, not all rules is involved. Contrarily, a very small
subset of rules is sufficient to build major projects.

In makefile of our "hello world" example, only one line is contained in which
an `obj-m` variable is defined and assigned to a value of `hello_world.o`.
For C programmers who just turned to kernel space programming may be confused
by this makefile. Because there is no make target at all, which is very
essential to building programs in user space. This is due to that our make file
is managed by kbuild system, and is used by kbuild system internally. What a
module developer need to do is stating that we have a module here for building.
With passing a few parameters to make, the actual build process is under kbuild
system's control. `obj-m` here is a special make variable that represents to
build a kernel module. The value assigned to it is the object file that used
for generating final `.ko` module file. So, why it is `hello_world.o` but
`hello_world.c`? That is what called implicit rules of make. For make tool, it
has a lot of build-in implicit rules, in which a chunk of common ways of
creating a target file from certain source file is defined. For example, C
compilation typically takes a `.c` file and makes a `.o` file. So, `make` tool
applies the implicit rule for C compilation when it sees this combination of
file name endings. In short, make will automatically compile `hello_world.o`
from `hello_world.c` implicitly with some default compilation flags. For
interested readers, manual of make tools will do a lot of help. The output name
of `.ko` file is determined by make the valued assigned to `obj-m` variable.
For our example, `obj-m := hello_world.o`, the generated module name is
`hello_world.ko`.

In some projects, the kernel module file is compiled from a lot of source file.
For that situation, simply append these file names after `obj-m` variable is not
working. To compile multiple source file, another kbuild variable `XXXX-objs`
is used. Assign multiple file names to it and reference it later in `obj-m` is
sufficient. For example, if our `hello_world.ko` is generated from `source1.c`
and `source2.c`, the correct way to compose make file is:

```
obj-m := hello_world.o
hello_world-objs := source1.o source2.o
```

Then run make command as before:

```
make -C ~/LDD_ROOT/kernels/linux-5.10.4 M=$(pwd) modules
```

Some other details in this command worthing a note are the `-C` option, `M`
option and `modules` target. `-C` is a standard make option, which instruct
make doing a directory changing before doing any actual actions. In our example
above, make command will firstly change its working directory into
`~/LDD_ROOT/kernels/linux-5.10.4` and then start building `modules` target
which is defined in the kernel's top-level makefile. The `M` option informs
kbuild system that an extern module is being built. The value assigned to `M`
is the directory where the module's kbuild file(makefile or kbuild file)
residents. This path must be an absolute path. The `modules` target is not
compulsory for extern modules building. `M` option has told kbuild that an
external module is being built.

## Advanced makefile

It is cumbersome to type such a long command during an intensive build-and-run
testing. Make users are used to type a single `make` in terminal for
initializing building process. To fulfill this, some tricks are used in writing
makefile which makes makefile smart enough to find kernel source tree
automatically for building external kernel modules.

Traditionally, a double-entering makefile is used which means the makefile will
be read twice during the building process:

```
# Normally, kernel's build system defines KERNELRELEASE variable.
# Users can determine which context our make process is in by validing if
# KERNELRELEASE is defined.

ifneq ($(KERNELRELEASE),)

# In kbuild context
module-objs := hello_world.o
obj-m := hello_world.o

else

# In normal make context
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
```

This makefile seems a little bit complex, but don't worry, it is still simple.
The whole make file can be divided into two parts by an if-else statement.
The first part can be seen in kernel's kbuild context with `KERNELRELEASE`
variable is set; the second part can be seen in normal make context. Variable
`KERNELRELEASE` is  acts like a flags that instructs make to execute different
building paths. `KERNELRELEASE` variable is defined in kernel's top-level
makefile, and only can be accessed in kbuild's context. For our example above,
after triggering make command in terminal, make reads this makefile and execute
targets in the `else` statement, since it has not set `KERNELRELEASE` yet. This
is the first entering. In this round, make set `KDIR` and `PWD` variables
properly, and execute command in `modules` target. You may have notice that
the command of `modules` target is what we have used in previously building
process, which is long and tiresome. The execution of this command traps our
make command into the second entering, in which `KERNELRELEASE` variable is set.
Due to the existence of `KERNELRELEASE`, the building process shifts to
the execution of command in `if` statement. The second pass is running inside
kbuild context with `obj-m` is defined; kernel's kbuild system will take care of
actually building the module.

If you want the module be compiled against a specific kernel tree, supply a
`KDIR=` option on the command line with path that points to correct kernel
source is fairly enough.

As a note, it is worth to note that this makefile is also compatible with make
command we used in previous chapter. For that situation, we directly instruct
make to use kernel's top-level makefile, in which `KERNELRELEASE` variable is
defined, and skip the first round of building process.

In newer versions of the kernel, "Kbuild" file takes precedence over "Makefile"
from kbuild system's view in your project. kbuild will first look for a file
named "Kbuild", and only if that doesn't not present, will it then look for a
makefile. This search sequence gives a flexibility that split our two-entering
makefile explicitly into two files.

```
# file Kbuild
module-objs := hello_world.o
obj-m := hello_world.o
```

```
# file Makefile
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

`make` command can only see makefile; kbuild system stops after processing
`Kbuild` file. The split makes thing clear and simple.

## Compiling flags

Compiling flags such as `-DDEBUG` can be specified by `ccflags-y` or
`CFLAGS_<filename>.o` variables. For example, by default kernel takes ISO C98
standard, but for modules which want to use a more modern standard, say `gnu99`,
it can be specified by adding `ccflags-y := -std=gnu99` in your Makefile's
kbuild context domain, or adding `CFLAGS_source1.o := -std=gnu99` if want to
apply this flag only to compiling `source1.o`.

## Makefile targets

A few of make targets are available when building an external module.

- `modules`:

    This is the default target for building an external module. It means that
    make will opt `modules` as its target if no any target is specified.

- `modules_install`:

    Install the external module just be built. By default, `modules_install`
    target uses `/lib/modules/($uname -r)/extra/` directory as its installation
    location. Users can point to its own installation path by feeding a special
    variable, `INSTALL_MOD_PATH`, to make command. For example, to install
    generated `.ko` file in `/kmods`:

    ```
    make INSTALL_MOD_PATH=/kmods` modules_install
    ```

    The `.ko` file, say `hello_world.ko`, will appear in
    `/kmods/lib/modules/$(uname -r)/extra/`


- `clean`:

    Remove all genereated files in the module directory.


- `help`:

    Dump a help message.


Usage of these targets is simple; we have seen the example of `modules` target
before in the processing of building "hello_world" module. A more formulatic
description is given below for a quick reference.

```
make -C $KDIR M=$PWD [target]
```

Replace target with any one listed above according to your need.

# ¶ The end
