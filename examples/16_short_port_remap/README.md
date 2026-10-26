# short_port_remap

This example drives the emulated `ldd-io` device, a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-io.md`](../../tools/qemu/ldd-io.md), at I/O port `0x200`:

| offset | register | used here as |
|--------|----------|--------------|
| 0      | DATA     | write: eight LEDs, read: received byte |
| 1      | STATUS   | bit 0 RX_READY, write 1 to acknowledge |
| 2      | CTRL     | not used here |
| 8-15   | MATRIX   | 8x8 LED matrix, one byte per row |

It still uses port I/O, but never calls `inb()`/`outb()` on a port number.
Instead it turns the ports into a `void __iomem *` with `ioport_map()` and
does every access with `ioread8()`/`iowrite8()` and friends, the accessors
used for memory-mapped registers too.

| node                           | minor | read                         | write |
|--------------------------------|-------|------------------------------|-------|
| `/dev/short_port_remap_data`   | 0     | bytes received from the host | bytes to the LEDs |
| `/dev/short_port_remap_matrix` | 1     | the 8 matrix rows            | matrix rows, seekable |

## ioport_map() is not a mapping

x86 has a separate 64K I/O port space, reached only with the `in`/`out`
instructions. Memory-mapped registers live in the physical address space and
must be mapped with `ioremap()` before use. `ioport_map()` lets a driver
treat both the same way:

- **On x86** (`lib/iomap.c`) it maps nothing. It returns the port number
  plus `0x10000` as a cookie. `ioread8()` and the others check the address:
  a cookie is turned back into a port and accessed with `inb()`/`outb()`; a
  real virtual address from `ioremap()` is accessed with a normal load or
  store.
- **On architectures without port instructions** (ARM, RISC-V, ...)
  (`asm-generic/io.h`) PCI port space is a memory window at `PCI_IOBASE`,
  and `ioport_map()` returns a real address inside it.

So the driver code is the same whether the registers are ports or memory;
only the line that obtains the pointer differs. `pci_iomap()` relies on
this: it calls `ioport_map()` or `ioremap()` depending on the BAR type. The
price is one address check per access, small next to the I/O itself.

| direct port I/O        | through `ioport_map()`             |
|------------------------|------------------------------------|
| `inb(base + off)`      | `ioread8(io + off)`                |
| `outb(v, base + off)`  | `iowrite8(v, io + off)`            |
| `outl(v, base + off)`  | `iowrite32(v, io + off)`           |
| `outsb(base, buf, n)`  | `iowrite8_rep(io, buf, n)`         |
| nothing to undo        | `ioport_unmap(io)`                 |

`ioport_map()` does not reserve anything: the ports are still claimed with
`request_region()`, and released after `ioport_unmap()`.

The rest is the same as with direct port I/O: one cdev whose `open()`
installs `data_fops` or `matrix_fops` by minor number, polling with
`readx_poll_timeout()` (here with `ioread8` as the read function), two
32-bit writes for a whole matrix picture, `fixed_size_llseek()` on the
matrix node, and `<linux/cleanup.h>` for the lock and the write buffer.

## older ways of doing the same

- LDD3 followed every `ioread8()`/`iowrite8()` with `rmb()`/`wmb()`. The
  accessors are already ordered against each other and against memory, so
  the barriers are gone.
- `register_chrdev()` registered a major with all 256 minors in one call;
  `alloc_chrdev_region()` and `cdev_add()` reserve only the minors used.
- `readsb()`/`writesb()` are the string accessors for memory addresses only;
  on a port cookie use `ioread8_rep()`/`iowrite8_rep()`.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## run QEMU with the device

**RUN ON HOST:** build QEMU with the devices from `tools/qemu` (see its
README) and add the device and its two chardevs to the QEMU command line,
for example at the end of `bin/boot.sh`:

```bash
    -chardev socket,id=panel0,host=127.0.0.1,port=7100,server=on,wait=off \
    -chardev socket,id=kbd0,host=127.0.0.1,port=7101,server=on,wait=off \
    -device ldd-io,panel=panel0,keyboard=kbd0
```

In two more host terminals, watch the panel and open the keyboard:

```bash
nc 127.0.0.1 7100      # panel
nc 127.0.0.1 7101      # keyboard
```

## Usage

Copy **load_module.sh** and **short_port_remap.ko** to the target machine,
then run:

```bash
sh load_module.sh
```

To use another port, e.g. with `-device ldd-io,...,iobase=0x300`, load the
module by hand with `insmod short_port_remap.ko base=0x300` and create the
two nodes as `load_module.sh` does (minor 0 and 1).

## test the module

```bash
grep short_port_remap /proc/ioports

# LEDs: the panel shows 0x55 as "○ ● ○ ● ○ ● ○ ●"
printf '\x55' > /dev/short_port_remap_data

# receive: type in the keyboard terminal, the bytes appear here
cat /dev/short_port_remap_data

# matrix: a whole picture (two iowrite32), then just the third row (iowrite8)
printf '\x3c\x42\xa5\x81\xa5\x99\x42\x3c' > /dev/short_port_remap_matrix
printf '\xff' | dd of=/dev/short_port_remap_matrix bs=1 seek=2 conv=notrunc
hexdump -C /dev/short_port_remap_matrix
```

---

### ¶ The end
