# Directory Tree

To fulfill the purpose of practicing Kernel Driver development, a lot of repos
and tools are needed to cooperate together as a unity.

In this chapter, a list of directories is presented here in which all the
necessary software and tools are itemized.

During the following chapters later, the working directory including everything
is referenced as the `ldd_root` directory. Relative paths encountered later are
referenced against the `ldd_root` until an explicit note is made.

# Export LDD_ROOT environment variable

```
export LDD_ROOT=/path/to/your/ldd_root/
```

Use this variable for the setup commands below. Automatic activation is
configured after cloning the tutorial repository.

# List of directory

```
tree -L 2 "$LDD_ROOT"

../../
├── .clangd                 # Installed clangd configuration for the QEMU devices.
├── activate                # Installed shell hook for this workspace.
├── bin                     # Installed shims and helper scripts.
├── kernels                 # Created by the installer; unused by the current build.
├── initramfs               # Directory containing files to build initramfs image.
├── shared                  # Directory shared with the guest through 9p over virtio.
│   └── Linux-Device-Driver # This is our example repo.
└── src                     # Kernel and tool sources.
    ├── linux-stable         # Linux 7.2.y sources and build artifacts.
    └── qemu                # QEMU sources and the build/ directory.
```

# Make necessary directories

```
mkdir -p "$LDD_ROOT"/{bin,shared,src}
```

# Clone this repo

```
cd $LDD_ROOT/shared
git clone https://github.com/d0u9/Linux-Device-Driver.git
```

# Install the workspace scripts

The tutorial repository contains the script sources. The workspace is a separate
runtime directory; do not use the tutorial checkout as `LDD_ROOT`.
After cloning, install the scripts into the workspace:

```bash
cd "$LDD_ROOT/shared/Linux-Device-Driver"
./tools/install.sh "$LDD_ROOT"
```

See [workspace tools](../../tools/README.md) for the script inventory and checks.
The installer installs the qemu shim, activation hook, clangd configuration, boot.sh,
and rebuild_initramfs.sh;
it also creates bin, src, kernels, initramfs, and shared if missing, preserving
their contents. Source trees and build outputs are added in later chapters.
Repeat these commands after updating the scripts in the tutorial repository.
Add the following line to `~/.bashrc` or `~/.zshrc`, using the absolute path
to your workspace:

```bash
source /path/to/your/ldd_root/activate
```

Open a new shell. Entering the workspace activates its tools; leaving restores
the previous environment. A permanent PATH entry is unnecessary. The earlier
`export LDD_ROOT=...` remains useful for initial setup but need not be kept in
your shell configuration once automatic activation is installed.

# ¶ The end
