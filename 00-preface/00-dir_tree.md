# Directory Tree

To fulfill the purpose of practicing Kernel Driver development, a lot of repos
and tools are needed to cooperate together as a unity.

In this chapter, a list of directories is presented here in which all the
necessary software and tools are itemized.

During the following chapters later, the working directory including everything
is referenced as the `$LDD_ROOT` directory. Relative paths encountered later are
referenced against the `$LDD_ROOT` until an explicit note is made.

## Export `$LDD_ROOT` environment variable

```bash
export LDD_ROOT=/path/to/your/ldd_root/
```

Add the line above in your `.bashrc` or `.zshrc` (if zsh is your working shell)
for a quick reference.

## List of directory

```bash
tree -L 1 -d $LDD_ROOT

../../
├── bin                     # Symbolic links of all executable binary files.
├── kernels                 # The dir containing Linux kernel sources.
│   └── linux-5.10.4
├── initramfs               # Directory containing files to build initramfs image.
├── nfs_dir                 # Directory which is shared between host and QEMU via NFS protocol.
│   └── Linux-Device-Driver # This is our example repo.
└── qemu                    # The QEMU source files, git cloned from official repo
```

## Some setups

Add bin directory in `PATH` environment for executable binary searching.

```bash
export PATH="$PATH:$LDD_ROOT/bin"
```

Append the line above in your shell's `rc` file.

## Make necessary directories

```bash
mkdir -p "$LDD_ROOT"/{bin,source,tools,kernel,nfs_host}
```

## Clone this repo

```bash
cd $LDD_ROOT/nfs_host
git clone https://github.com/d0u9/Linux-Device-Driver.git
```

## ¶ The end
