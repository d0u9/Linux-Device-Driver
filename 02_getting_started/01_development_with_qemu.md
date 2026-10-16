# Develop Linux Driver with QEMU

It is better to opt virtual machine instead to eliminate the peculiarities of
physical hosts on which readers test the examples. [VirtualBox] is a powerful
x86 and AMD64/Intel64 virtualization toolbox for enterprises and home users.
[VirtualBox] is powerful and comprehensive; however, it lacks the ability to
set up a minimal development environment for driver debugging and needs a
complete Linux distribution installed to boot up.

The drawback of lacking flexibility introduces low production efficiency. An
alternative to [VirtualBox] is [QEMU], a generic and open source machine
emulator and virtualizer widely used in the production environment. It is as
powerful as [VirtualBox] but is more lightweight and convenient since users can
develop their own virtual hardware easy and fast.

The most attractive feature QEMU provided is the command line option `-kernel`.
This option allows the user to feed a given Linux kernel directly from the
command line without installing it in the disk image. In other words, the
developer doesn't need to install a complete Linux distribution; instead, only
the Kernel image and an initial ramdisk are necessary to boot the whole system,
fast and convenient.

# Install QEMU

Install [QEMU] is simple. Its official website details all the procedures to
install a fresh [QEMU] on your host from scratch. On most Linux distributions,
the package manager, e.g., apt for Ubuntu or dnf for Fedora, is a good friend
for installing necessary software. However, installing a cutting-edge or custom
version must be done manually by compiling from the source.

Some special virtual hardware is a prerequisite for the samples presented in
this repo. For example, in the chapter on I/O operations and the chapter on
interrupts, virtual hardware is essential to respond to the driver's requests
and give feedback. Thus, building [QEMU] from the source with our patches
patched is crucial to function all pieces of samples in this repo.

## Download and patch QEMU

Install the build dependencies on Fedora:

```
sudo dnf install gcc gcc-c++ binutils make git patch gdb wget \
    python3 pkgconf-pkg-config ninja-build glib2-devel libslirp-devel \
    libcap-ng-devel libattr-devel
```

Download QEMU source from official git:

These instructions target QEMU 11.1.2. The custom device patch still needs
to be ported and the corresponding driver examples verified against this version.

```
mkdir -p "$LDD_ROOT/src"
cd "$LDD_ROOT/src"
git clone --depth 1 --branch v11.1.2 https://gitlab.com/qemu-project/qemu.git qemu
cd qemu
git submodule update --init --recursive
```

The driver examples require a patch that adds custom emulated devices.

> TODO: Port QEMU_LDD.patch to QEMU 11.1.2 and update the patch location
> and application instructions. Verify that isa-kmod-edu, pci-kmod-edu,
> and usb-kmod-edu are registered and that the corresponding driver examples work.

Until this is complete, an unpatched build can boot a guest but cannot run
the examples that require these custom devices or the current boot.sh command.

## Build QEMU

Set the build variables. The prefix is only used if you later run `make install`;
the steps below run QEMU directly from the build directory.

```
INSTALL_DIR="$LDD_ROOT/tools/qemu"
TARGET_LIST="i386-softmmu,x86_64-softmmu"
```

Then, configure and build

```
cd "$LDD_ROOT/src/qemu"
mkdir -p build
cd build
../configure \
    --prefix="$INSTALL_DIR" \
    --target-list="$TARGET_LIST" \
    --enable-virtfs \
    --enable-slirp \
    --disable-docs \
    --enable-debug \
    --extra-cflags="-g3" \
    --extra-ldflags="-g3" \
    --disable-strip
make -j"$(nproc)"
```

Installation is unnecessary; users can directly use the binary from the build
directory through a symbolic link in `$LDD_ROOT/bin`:

```
mkdir -p "$LDD_ROOT/bin"
cd "$LDD_ROOT/bin"
ln -s ../src/qemu/build/qemu-system-x86_64 qemu
export PATH="$LDD_ROOT/bin:$PATH"
```

## Test

When the building process finishes, run the command below to test if it works
properly.

```
qemu --version
```

# ¶ The end

[VirtualBox]: http://www.qemu.org/
[QEMU]: http://www.qemu.org/
