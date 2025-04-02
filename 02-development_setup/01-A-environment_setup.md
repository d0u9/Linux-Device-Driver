# Environment Setup

To prevent playing around with this repo from messing up your system, we need to set up the enviroment properly to create isolated environment for running examples. (It is not completely isolated, for example, the rust is installed user-wide).

## Create shim file

```bash
cat << EOF > "$LDD_ROOT/bin/ldd-shim.sh"
export PATH="$LDD_ROOT/bin:\$PATH"
export LD_LIBRARY_PATH="$LDD_ROOT/lib:\$LD_LIBRARY_PATH"

# Make examples in this repo build against our kernel source tree
export LDD_KDIR="$LDD_ROOT/kernel/linux-current"

# The linux version
export KERNEL_VERSION=6.14

# The QEMU version
export QEMU_VERSION=9.2.3

# Disable this if you want to use GCC as the default compiler
export LDD_LLVM=1

EOF

source "$LDD_ROOT/bin/ldd-shim.sh"
```

The `$LDD_ROOT` is critical because it is the root hosting all our files and artifacts.

## Create necessary directories

```bash
mkdir -p "${LDD_ROOT}"/{bin,lib,source,tools,kernels,nfs_root}
```

## Clone this repo

```bash
cd $LDD_ROOT/nfs_root
git clone https://github.com/d0u9/Linux-Device-Driver.git
```

## Download Linux Source

Linux kernel sources of different versions are listed on its official web, [The Linux Kernel Archives].

At the time I write this documentation, the latest stable kernel version is
**${KERNEL_VERSION}**, and the examples in this book are implemented against this version.

There are many ways to obtain the kernel source. For example, download a tarball directly, make a git clone from the official's git repo, or even get a new version by patching an old one.

Here, we directly download the kernel source tarball from the official website and extract it locally in `$LDD_ROOT/kernel/` directory.

To download and extract the source ball:

```bash
cd $LDD_ROOT/kernels

wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VERSION}.tar.xz
tar -xf linux-${KERNEL_VERSION}.tar.xz

ln -s linux-${KERNEL_VERSION} linux-current
```

## Prepare Rust Toolchain

Linux has officially supports Rust in the latest kernel, and if you want to test rust functionality and build kernel modules using rust, you have to have rust toolset at hand.

### How to check if I missed some tool

You can run below command in the Linux source tree to test what tools are lost.


```bash
cd $LDD_ROOT/kernel/linux-current

make rustavailable
```

## Install Rust Toolchain for buiding rust code

### Rust compiler

> Ref: [Rust Offical](https://www.rust-lang.org/tools/install)

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### bindgen

```bash
cargo install --locked bindgen-cli
```

### Rust Core

```bash
rustup component add rust-src
```

## Install LLVM and Clang

Find the latest LLVM in [kernel.org] Page, and dowload one per your architecture.

```bash
cd $LDD_ROOT/tools
wget https://mirrors.edge.kernel.org/pub/tools/llvm/rust/files/llvm-19.1.7-rust-1.85.0-x86_64.tar.gz
tar -xf llvm-19.1.7-rust-1.85.0-x86_64.tar.gz
mv llvm-19.1.7-rust-1.85.0-x86_64 llvm
rm llvm-19.1.7-rust-1.85.0-x86_64.tar.gz

cd $LDD_ROOT/bin
ln -s ../tools/llvm/bin/* .

cd $LDD_ROOT/lib
ln -s ../tools/llvm/lib/* .
```

NOTE: Don't forget to run `source "$LDD_ROOT/bin/ldd-shim.sh"` once you started a new shell.

## For rust developers

Other than the tools mentioned above, developing rust in linux kernel needs extra tools.

### rustfmt

The latest `rustup` command will have `rustfmt` installed by default, but for users who using old rustup, running below command to have one:

```bash
rustup component add rustfmt
```

### clippy

This should be isntalled by default, but for others who don't have:

```bash
rustup component add clippy
```

### rust-analyzer

`rust-analyzer` should be installed properly already if you are using the latest `rustup`. To make it working more smooth with Linux kerne, generating a configuration file soely to the Kernel is good enough:

```bash
cd $LDD_ROOT/kernel/linux-current

make rust-analyzer
```

## LLVM vs GCC

For the best support of Rust in Linux, it is recommended to always use LLVM, because GCC for rust is very experimental at the moment.

If you want to use LLVM as the compiler, add this line below.

```bash
echo "export LDD_LLVM=1" >> "$LDD_ROOT/bin/ldd-shim.sh"

source "$LDD_ROOT/bin/ldd-shim.sh"
```

Or you can always feed make command `LLVM=1` at anytime you want to use LLVM instead of GCC.

```bash
make LLVM=1 ...
```

In this repo, we stick to using LLVM as the default compiler all the time to prevent strange errors.

Reference:

- [Linux Kernel Doc](https://docs.kernel.org/rust/quick-start.html)

## ¶ The end

[The Linux Kernel Archives]: https://www.kernel.org/
[kernel.org]: https://mirrors.edge.kernel.org/pub/tools/llvm/rust/
