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

Install ninja-build with dnf or apt.

```
sudo dnf -y install ninja-build
```

Qemu will also require the following packages installed:

```
sudo dnf -y install libcap-ng-devel libattr-devel
```

Download QEMU source from official git:

Note: For now, QEMU 5.2.0 is used through out this repo.

```
git clone https://git.qemu.org/git/qemu.git
cd qemu
# The following line is an older version of QEMU. QEMU_LDD.patch has been updated for 7.0.50.
# git checkout v5.2.0
git submodule init
git submodule update --recursive
```

Apply QEMU patch for adding new hardwares:

{TODO}: Correct this url.
```
wget https://raw.githubusercontent.com/d0u9/Linux-Device-Driver/draft/02_getting_start_with_driver_development/QEMU_LDD.patch
git am QEMU_LDD.patch
```

## Build QEMU

Install necessary packages which are essential to building QEMU:

```
# For ubuntu 18.04
sudo apt-get install ninja-build libglib2.0-dev libcap-ng-dev libcairo2-dev python3-sphinx
```

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
    ;
make -j
# only do this line if you actually want to install QEMU otherwise note the instructions about setting up in $LDD_ROOT/bin
make install
```

Add `$INSTALL_DIR` in your `$PATH` environment variable.

```
export PATH="$PATH:$INSTALL_DIR"
```

Installation is unnecessary; users can directly use the binary from the build
directory:

```
cd $LDD_ROOT/bin
ln -s ../qemu/build/qemu-system-x86_64 qemu
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
