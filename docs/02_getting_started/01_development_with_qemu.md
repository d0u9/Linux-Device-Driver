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

These instructions target QEMU 11.1.2.

```
mkdir -p "$LDD_ROOT/src"
cd "$LDD_ROOT/src"
git clone --depth 1 --branch v11.1.2 https://gitlab.com/qemu-project/qemu.git qemu
cd qemu
git submodule update --init --recursive
```

The driver examples require custom emulated devices. Add them to the QEMU
source tree from the tutorial checkout before configuring QEMU:

**RUN ON HOST:**

```bash
./tools/qemu/link.sh "$LDD_ROOT/src/qemu"
```

See [the device sources](../../tools/qemu/README.md) for what the script changes.

> TODO: Write the devices in tools/qemu/hw/ldd/ and verify the corresponding
> driver examples in the guest.

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

QEMU installation is unnecessary. Install the workspace scripts as described
in the [workspace setup](../00_preface/00_dir_tree.md#install-the-workspace-scripts).

The installed `$LDD_ROOT/bin/qemu` shim runs
`$LDD_ROOT/src/qemu/build/qemu-system-x86_64` and forwards all arguments.
It derives the workspace path from its own location and does not fall back
to a system QEMU. The copy in the tutorial repository is the script source,
not the runtime entry point.

### Automatic environment activation

The installed `$LDD_ROOT/activate` file provides the shell hooks. Add this line
to `~/.bashrc` (Bash) or `~/.zshrc` (Zsh), using your workspace's absolute path:

```bash
source /path/to/your/ldd_root/activate
```

Open a new shell after this one-time setup. Entering the workspace or any of
its subdirectories sets `LDD_ROOT` and prepends `$LDD_ROOT/bin` to `PATH`.
Thus `qemu` runs the installed shim, including when working in the tutorial
checkout under `shared`. Leaving the workspace restores the saved `PATH`
and `LDD_ROOT`; re-entering activates it again. No per-visit `source` command
is needed. The tutorial checkout does not determine the activation boundary.

Zsh updates the environment immediately after `cd`. Bash updates it before
the next prompt, so use a separate command after `cd`, rather than
`cd /path/to/workspace && qemu`. Existing shell hooks are preserved. This hook
manages one workspace per shell and restores the saved PATH on exit, including
discarding PATH changes made while active. It does not stop running processes.

## Test

When the building process finishes, run the command below to test if it works
properly.

```
"$LDD_ROOT/bin/qemu" --version
```

If the custom devices were added with `link.sh`, they are listed among the
available devices, with names starting with `ldd-`:

**RUN ON HOST:**

```bash
"$LDD_ROOT/bin/qemu" -device help | grep ldd-
```

No output means the devices were not built into QEMU. Run `link.sh` as
described above and build QEMU again.

# ¶ The end

[VirtualBox]: http://www.qemu.org/
[QEMU]: http://www.qemu.org/
