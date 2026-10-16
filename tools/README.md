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

The installer creates the workspace layout and installs only `activate` and
the `qemu` shim:

```text
LDD_ROOT/
├── activate
├── bin/
│   └── qemu
├── src/
├── kernels/
├── initramfs/
└── shared/
```

Existing directory contents are preserved. Rerunning the installer updates the
two installed scripts. Source checkouts and build outputs are added later;
the installer does not create `src/qemu/build` before QEMU is cloned.
The boot and initramfs scripts remain available for manual installation.
It can be invoked from any working directory using its absolute path.

Add the `source` line printed by the installer to `~/.bashrc` or `~/.zshrc`
and open a new shell. Entering the workspace activates its tools; leaving
restores the previous environment. Repeat the installation commands after
updating these script sources. Installation does not build QEMU, the kernel,
or the initramfs, or modify the shell configuration.

See [QEMU setup](../docs/02_getting_started/01_development_with_qemu.md)
for build instructions and activation behavior.

## Patch status

[`patches/QEMU_LDD.patch`](patches/QEMU_LDD.patch) contains the custom QEMU
devices. Porting it to QEMU 11.1.2 is still pending; `boot.sh` requires those
devices and cannot run with an unpatched QEMU build.

## Check the scripts

Run from the tutorial checkout:

```bash
python3 tools/tests/test_install.py
python3 tools/tests/test_qemu_shim.py
python3 tools/tests/test_activate.py
bash -n tools/install.sh tools/activate tools/bin/qemu tools/bin/boot.sh tools/bin/rebuild_initramfs.sh
```

The tests use temporary workspaces and mock executables. Bash is required;
Zsh activation is also checked when Zsh is installed. The tests do not boot
a guest or build the QEMU patch.
