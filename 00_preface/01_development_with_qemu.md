# Develop Linux Driver with QEMU

To eliminate to the differences between physical hosts on which the reader make
their development, it is better to use a virtual machine instead. [VirtualBox]
is a powerful x86 and AMD64/Intel64 virtualization toolbox for enterprise as
well as home use. [VirtualBox] does be powerful and comprehensive, but it lacks
the ability to setup a minimal development environment for driver debugging, and
needs a full Linux distribution to be installed in to boot the whole system.

The drawback of lacking flexibility introduces low production efficiency. An
alternative to [VirtualBox] is [QEMU], a generic and open source machine
emulator and virtualizer which is widely used in production environment. It is
powerful as [VritualBox] but more lightweight and convenient, since users are
granted the ability to develop their own virtual hardwares easy and fast.

The most attractive feature QEMU provided is the command line option `-kernel`.
This option permits it users to feed a given Linux kernel without installing it
in the disk image. In other words, developer need not to install a full Linux
distribution, only the Kerenl image and initial ram disk are necessary to boot
the whole system, fast and convenient.

# Install QEMU

Install [QEMU] is simple, its official website details all the procedures to
install a fresh [QEMU] on your host. On most Linux distributions, the package
manager, e.g. apt or rpm, is a good friend to install necessary software.
However, some cutting edge versions or a custom version of software must be
installed via manually compiling from source.

For the examples presented in this repo, some special virtual hardwares re
prerequisite. In the chapter of I/O operations and the chapter of interrupt,
virtual hardwares are essential to response the requests from driver and give
feedbacks. For the purpose of functioning all pieces of code in this repo,
compiling [QEMU] from source is necessary.

## Download and patch QEMU

Download QEMU source from official git:

Note: For new, QEMU 5.2.0 is used through this repo

```
git clone https://git.qemu.org/git/qemu.git
cd qemu
git checkout v5.2.0
git submodule init
git submodule update --recursive
```


Apply QEMU patch for adding new hardwares:

```
git am QEMU_LDD.patch
```

## Build QEMU

Setup necessary ENVs for building.

```
export INSTALL_DIR=/path/to/be/installed/to
export CONFIG_DIR=$INSTALL_DIR/etc
export TARGET_LIST="i386-softmmu,x86_64-softmmu"
```

Then, configure and build

```
mkdir build
cd build
../configure \
    --prefix=$INSTALL_DIR \
    --sysconfdir=$CONFIG_DIR \
    --target-list=$TARGET_LIST \
    --enable-virtfs \
    --enable-debug \
    --extra-cflags="-g3" \
    --extra-ldflags="-g3" \
    --disable-strip \
    --disable-docs
make -j
make install
```

Add `$INSTALL_DIR` in your `$PATH` environment variable.

```
export PATH="$PATH:$INSTALL_DIR"
```

Installation is unnecessary, users can directly use the binary from the build
directory:

```
cd $LDD_ROOT/bin
ln -s ../qemu/build/qemu-system-x86_64 qemu
```

## Test


When the building process finished, run the command below to test if it works
properly.

```
qemu --version
```

# ¶ The end

[VirtualBox]: http://www.qemu.org/
[QEMU]: http://www.qemu.org/
