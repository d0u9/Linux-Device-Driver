# Directory Tree

To fulfil the purpose of practicing Kernel Driver development, a lot of repos
and tools are needed which assemble together as a unity.

This chapter, list of directory is presented here in which all the necessary
softwares and tools are itemed.

During the whole chapters later, the directory includes everything is
referenced as the `ldd_root` directory. All the relative paths all referenced
against the `ldd_root` until an explicit note is made.

# Export LDD_ROOT environment variable

```
export LDD_ROOT=/path/to/your/ldd_root/
```

Add the line above in your `.bashrc` or `.zshrc` (if zsh is your woriking shell)
for a quick reference.

# List of directory

```
tree -L 1 -d LDD_ROOT

../../
├── bin                     # Symbolic links of all executable binary files.
├── kernels                 # The dir containing Linux kernel sources.
│   └── linux-5.10.4
├── initramfs               # Directory containing files to build initramfs image.
├── nfs_dir                 # Directory which is shared between host and QEMU via NFS protocol.
└── qemu                    # The QEMU source files, git cloned from official repo
```

# Some setups

Add bin directory in `PATH` environment for executable binary searching.

```
export PATH="$PATH:$LDD_ROOT/bin"
```

Append the line above in your your shell's `rc` file.

# ¶ The end
