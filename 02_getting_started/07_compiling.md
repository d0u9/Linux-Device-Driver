# Compiling

We briefly discussed a simple "hello world" example in the previous section and
compiled it into a module file that can be loaded into kernel by using `insmod`
or `modprobe` command in user space. The module's compilation looks trivial
with a two lines Makefile, just like any other C program in user space. However,
the actual process taken by this "simple" Makefile is not as simple as it may
be seen; it accepts a trail of parameters and options that are not usual to a
typical makefile. A quick glance over the previous `make` command may confuse
its user that our  Makefile seems not to be invoked at all. The kernel's build
system provides a standard and typical build process to its module developers
that a two-step compilation makefile is used. A deep talk about this two-step
compilation will give in a later chapter.

Building the kernel and kernel modules depends on a few libraries and tools.
The most critical tools essential to compiling kernel sources are `make` and
`gcc`. Both tools are GNU software, which means they are free and open source.

Linux kernel itself is a large project containing millions of lines of code.
Besides, Linux is a monolithic kernel in that each component must be linked
into one single binary. What makes things worse is that Linux can be tailored
and run on different hardware platforms with various CPU chips. These make
Linux's build system a monster that can be configured by enabling or disabling
some parts and only compiles components that are touched during configuration.

Linux takes its particular building system, named the `kbuild` system, to
handle all these problems. It is a large and complex build system that depends
on a few scripts and tools. Most Makefiles within the kernel are kbuild
Makefiles that use the kbuild infrastructure. Actually, the Makefile used in
the "hello world" example is a kbuild makefile which takes on rules of Makefile
because it is based on a regular makefile. For the newer kernel versions, the
preferred file name for the kbuild files is 'Makefile', but 'Kbuild' can be used
flawlessly. If both 'Makefile' and 'Kbuild' files exist, the kbuild system will
use the 'Kbuild' file.

There are many tools for building a project, such as "CMake" or "bazel", in
modern days. However, the Linux kernel sticks to using the make tool as a
basement of its building system. That is because shifting from one building
system to another is expensive, and the shifting is unnecessary since make is
simple and powerful enough.

The kbuild system has a set of definitions and rules that hugely expand the
ability of a regular makefile while reducing the complexity of configuration.
There are many docs in Kernel's documentation folder detailing these rules and
details. However, for kernel module developers, a tiny subset of knowledge is
needed; just as simple as a two lines makefile is enough as you have tried
before.

## Before we start

Linux kernel modules are ELF loadable files that can be loaded into the kernel
at runtime. Therefore, it depends on symbols exported by the kernel. Like any
other C program, to use functions and variables exported by other libraries, it
must include header files that declare functions and variables. And our C
kernel modules are not exceptions; it has to include the necessary headers in
the first place. The kernel's source tree, of course, contains these header
files. However, the package manager, e.g., `yum` or `apt-get`, can install
these files as standalone packages as well for most Linux distributions. In the
"hello world" example, a complete source tree is used,  against which the
module compiles.

Until now, What we have talked about is how to build an external module. Kernel
modules accepted by the community (or just for some other purpose) may be added
to the upstream source tree and can be compiled and linked either within the
kernel or as a standalone module. Adding a kernel module in the source tree and
compiling it into the binary of the kernel image is pretty similar to building
it as an external module. In the kernel's source, many modules can be compiled
as separate modules or linked within the kernel; taking these modules as a
reference is a good starting point to questions on adding your module into the
kernel tree.

We now have our kernel and module source in hand, but don't start hitting the
"make" button in a hurry. The last check before the flight is the GCC version.
Insufficient GCC version is the potential root of various problems which are
mysterious. Linux Kernel heavily depends on GCC, which means the mismatch of GCC
version and kernel version can lead to errors during compiling time. Kernel's
documentation lists the minimal required tools to compile Linux kernel and
kernel modules. Reading this list in `Documentation/Changes` in the kernel's
documentation directory and checking if the tools you are preparing to use are
eligible are always the best practices.

## Makefile for kernel module

By introducing enough background knowledge, it is time to talk about "real"
things about building kernel modules. The first thing to do is compose the
Makefile. As mentioned before, the Makefile used to build our module project is
actually a "kbuild" script, which uses make as its basement tool to manage file
dependencies. The "kbuild" internally has a lot of rules, variables and
definitions, which are sole to the Linux kernel. For building a kernel module,
not all rules are involved. Contrarily, a tiny subset of rules is sufficient.

The Makefile used in the "hello world" example contains only one line rule in
which an `obj-m` variable is defined and is assigned to a value of
`hello_world.o`. This makefile maybe confuses the programmers who are familiar
with programming in the userspace since no "target" is defined. Actually, the
kbuild system defines the "modules" target in its makefile, and that target is
used to build kernel modules out of the source tree by invoking the command:

```
make -C ~/LDD_ROOT/kernels/linux-5.10.4 M=$(pwd) modules
```

Note the "modules" target in the above make command.

The `obj-m` variable is unique to the kbuild system because it tells the kbuild
system that we have a target kernel object named "hello_world.o" to compile. The
value name assigned to `obj-m` variable is the name of generated kernel module
with the replacement from `.o` to `.ko`. The kernel module file itself is a
typical object file with some "modifications". But why do we use `hello_world.o`
instead of `hello_world.c` here? It is due to make's implicit rules. The `make`
command has many built-in implicit rules which define the ways of creating a
target file from a specific source file. For example, one of the built-in
implicit rules generates the `.o` file from its counterpart `.c` file. Benefits
from these rules, users can write concise and clear makefile.

In some projects, the kernel module file is compiled and linked from many source
files. For that situation, append these file names right after `obj-m` variable
is not working. Use another kbuild built-in variable `XXX-objs` to generate a
single module binary from multiple source files. List and append the object file
names to the `XXX-objs` variable and then reference this `XXX` later in `obj-m`
variable is sufficient. For example, if the "hello_world.ko" is linked from two
object files, say `source1.o` and `source2.o`, the correct makefile is:

```
obj-m := hello_world.o
hello_world-objs := source1.o source2.o
```

The `hello_world-objs` variable tells the objects that are linked in the final
kernel objects, and it then generates an integrated object file named
`hello_world.o`. The `obj-m` variable takes the object file and turns it into
the final `.ko`.

Other details in this command worthing a note are the `-C` option, `M` option
and `modules` target. `-C` is a standard make option, which instructs make doing
a directory changing before doing any actual actions. In our example above, the
`make` command first changes its working directory to
`~/LDD_ROOT/kernels/linux-5.10.4` and starts building the `modules` target
defined in the kernel's top-level makefile. The `M` is a make variable that
informs the kbuild system that an extern module is going to be built. The value
assigned to `M` is the directory where the module's kbuild file(makefile or
kbuild file) residents. This path must be an absolute path. The `modules` target
is not compulsory for extern module building. The `M` has told kbuild system
that an external module is being built.

## Advanced makefile

It is cumbersome to type such a long command during intensive build-and-run
testing. The `make` command users are used to typing a single `make` in the
terminal to initialize the building process. Use some tricks to overcome these
constraints and make our makefile smart enough to automatically find the kernel
source tree for building external kernel modules.

Traditionally, a "double-entering" makefile technique is used. The
"double-entering" means that the make command reads the makefile twice during
the building process:

```
# Normally, kernel's build system defines the KERNELRELEASE variable.
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

This makefile seems a bit complex, but don't worry, it is still simple.  The 
whole makefile can be divided into two parts by an `if-else` statement. The
first part can only be seen in the kernel's kbuild context with the
`KERNELRELEASE` variable set; the second part can be seen in the normal make
context. Variable `KERNELRELEASE` acts like an indicator instructing command to
execute different building paths. The kernel's top-level makefile defines the
`KERNELRELEASE` variable, which can only be accessed in kbuild's context. For
our example above, after triggering the `make` command in a terminal, the `make`
reads this makefile and executes targets in the `else` statement since it has
not set `KERNELRELEASE` yet. The first path that the make executed is the first
entering. In this round, the make sets `KDIR` and `PWD` variables properly and
executes commands in the `modules` target. You may have noticed that the
commands of the `modules` target are what we have used in the previous building
process. The execution of these commands traps our make into the second
entering, in which the `KERNELRELEASE` variable is set. Due to `KERNELRELEASE`,
the building process shifts to execute commands in the `if` statement. The
second pass runs inside the kbuild context with `obj-m` variable is defined;
the kernel's kbuild system will take care of the actual building of the module.

If you want the module to be compiled against a specific kernel tree, supply
the `KDIR=` option on the command line with an absolute path that points to the
correct kernel source. That is pretty enough.

In newer versions of the kernel, the "Kbuild" file takes precedence over the
"Makefile". The kbuild will first look for a file named "Kbuild", and only if
that doesn't present will it then look for a makefile. This search sequence
gives flexibility that splits our two-entering makefile explicitly into two
files.

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

The `make` command can only see makefile; kbuild system stops after processing
`Kbuild` file. The split makes things clear and concise.

## Compiling flags

Compiling flags such as `-DDEBUG` can be specified by `ccflags-y` variable or
`CFLAGS_<filename>.o` variable. The difference between these two forms is that
the latter only applies to a single source file. For example, the kbuild system
uses ISO C98 by default, an ancient standard with many new features unsupported.
Declaring to use another modern standard, say the `gnu99`, by adding the
`ccflags-y := -std=gnu99` flag in your Makefile's kbuild context. Another way
to specify the `CFLAGS` only to a single object compilation is adding
`CFLAGS_source1.o := -std=gnu99`.

## Makefile targets

A few of make targets are available when building an external module.

- `modules`:

    This is the default target for building an external module. It means that
    make will opt `modules` as its target if no any target is specified.

- `modules_install`:

    Install the external module just built. By default, `modules_install` target
    uses `/lib/modules/($uname -r)/extra/` directory as its installation
    location. Users can point to their installation path by feeding a special
    variable, `INSTALL_MOD_PATH`, to the make command. For example, to install
    generated `.ko` file to `/kmods`:

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
before in the "hello_world" module building. A more formulatic description is
given below for a quick reference.

```
make -C $KDIR M=$PWD [target]
```

Replace target with any one listed above according to your need.

# ¶ The end
