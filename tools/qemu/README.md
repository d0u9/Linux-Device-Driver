# Custom QEMU devices

[`hw/ldd/`](hw/ldd) holds the emulated devices used by the driver examples.
Each device is a source file with its own `LDD_*` symbol in
[`Kconfig`](hw/ldd/Kconfig) and an entry in [`meson.build`](hw/ldd/meson.build).

> TODO: No devices are written yet. Add them here and list them in this file.

## Add the devices to QEMU

**RUN ON HOST**, after cloning QEMU and before configuring it:

```bash
./tools/qemu/link.sh "$LDD_ROOT/src/qemu"
```

The script links `hw/ldd` into the QEMU source tree and appends
`subdir('ldd')` to `hw/meson.build` and `source ldd/Kconfig` to
`hw/Kconfig`. Rerunning it changes nothing. Without an argument it uses
`$LDD_ROOT/src/qemu`. Then configure and build QEMU as usual.

QEMU must be built in `$LDD_ROOT/src/qemu/build`, the default directory
used by its `configure`. The `bin/qemu` shim and the installed
`$LDD_ROOT/.clangd`, which lets clangd find the compile commands for these
sources, both expect it there.

To restore the QEMU source tree, for example before checking out another
QEMU version:

```bash
./tools/qemu/link.sh --remove "$LDD_ROOT/src/qemu"
```
