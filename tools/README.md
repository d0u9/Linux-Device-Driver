# Workspace tools

This directory holds the sources for scripts installed into a separate
`LDD_ROOT` workspace. It is not itself a runtime workspace. See the
[workspace layout](../docs/00_preface/00_dir_tree.md) for initial setup.

| Source | Installed path | Purpose |
| --- | --- | --- |
| `activate` | `$LDD_ROOT/activate` | Bash/Zsh hooks for automatic activation when entering the workspace. |
| `bin/qemu` | `$LDD_ROOT/bin/qemu` | Run the workspace's x86_64 QEMU build. |
| `bin/boot.sh` | `$LDD_ROOT/bin/boot.sh` | Boot the tutorial guest with the custom emulated devices. |
| `bin/rebuild_initramfs.sh` | `$LDD_ROOT/bin/rebuild_initramfs.sh` | Rebuild the workspace's initramfs archive. |

## Install

Run from the tutorial checkout, specifying your runtime workspace:

```bash
./tools/install.sh /home/doug/Kernel
```

The installer creates the workspace layout and installs the activation hook,
QEMU shim, guest launcher, and initramfs build script:

```text
LDD_ROOT/
├── activate
├── bin/
│   ├── qemu
│   ├── boot.sh
│   └── rebuild_initramfs.sh
├── src/
├── kernels/
├── initramfs/
└── shared/
```

Existing directory contents are preserved. Rerunning the installer updates the
installed scripts. Source checkouts and build outputs are added later;
the installer does not create `src/qemu/build` before QEMU is cloned.
It can be invoked from any working directory using its absolute path.

Add the `source` line printed by the installer to `~/.bashrc` or `~/.zshrc`
and open a new shell. Entering the workspace activates its tools; leaving
restores the previous environment. Repeat the installation commands after
updating these script sources. Installation does not build QEMU, the kernel,
or the initramfs, or modify the shell configuration.

See [QEMU setup](../docs/02_getting_started/01_development_with_qemu.md)
for build instructions and activation behavior.

After entering the workspace with activation configured, run on the host:

```bash
rebuild_initramfs.sh
boot.sh
```

The initramfs source tree must already be populated, and the kernel and QEMU
must be built. The launcher uses the workspace's kernel, initramfs, and shared
directory. See the patch status below before using the custom devices.

## Patch status

[`patches/QEMU_LDD.patch`](patches/QEMU_LDD.patch) contains the custom QEMU
devices. Porting it to QEMU 11.1.2 is still pending; `boot.sh` requires those
devices and cannot run with an unpatched QEMU build.

## Build all examples

Run from the tutorial checkout on a Linux host to build every example module
and userspace test against one configured, prepared kernel tree:

```bash
./tools/check_builds.sh /path/to/linux    # default: $LDD_ROOT/src/linux-stable
./tools/check_builds.sh /path/to/linux 07_ioctl 22_lddbus
```

Examples are built in a temporary copy, so the checkout stays clean. The script
prints `PASS`/`FAIL` per directory, the build log for each failure, and exits
non-zero if any build fails. Unfinished `*_NOT_COMPLETE` examples are
skipped unless named explicitly. The kernel tree path must not contain spaces.

## Check the scripts

Run from the tutorial checkout:

```bash
python3 tools/tests/test_install.py
python3 tools/tests/test_boot.py
python3 tools/tests/test_qemu_shim.py
python3 tools/tests/test_activate.py
python3 tools/tests/test_check_builds.py
bash -n tools/install.sh tools/activate tools/bin/qemu tools/bin/boot.sh tools/bin/rebuild_initramfs.sh tools/check_builds.sh
```

The tests use temporary workspaces and mock executables. Bash is required;
Zsh activation is also checked when Zsh is installed. The tests do not boot
a guest or build the QEMU patch.
