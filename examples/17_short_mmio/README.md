# short_mmio

This example drives the emulated `ldd-io` device, a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-io.md`](../../tools/qemu/ldd-io.md), mapped in memory space
(`mmio=on`) at physical address `0xff000000`:

| offset | register | used here as |
|--------|----------|--------------|
| 0      | DATA     | write: eight LEDs, read: received byte |
| 1      | STATUS   | bit 0 RX_READY, write 1 to acknowledge |
| 2      | CTRL     | not used here |
| 8-15   | MATRIX   | 8x8 LED matrix, one byte per row |

| node                     | minor | read                         | write |
|--------------------------|-------|------------------------------|-------|
| `/dev/short_mmio_data`   | 0     | bytes received from the host | bytes to the LEDs |
| `/dev/short_mmio_matrix` | 1     | the 8 matrix rows            | matrix rows, seekable |

## mapping the registers

Memory-mapped registers live in the physical address space, next to RAM.
The kernel cannot use a physical address directly; it first maps it:

```c
request_mem_region(base, 16, "short_mmio");   /* claim, see /proc/iomem */
io = ioremap(base, 16);                       /* page tables, uncached */
...
iounmap(io);
release_mem_region(base, 16);
```

`ioremap()` creates page table entries for the range and marks them
uncached, so every access really reaches the device, in program order. The
result is a real kernel virtual address, unlike the cookie that
`ioport_map()` returns for ports. `ioread8()`/`iowrite8()` on it become
plain load and store instructions.

The pointer is still `void __iomem *` and must not be dereferenced: the
compiler would be free to merge, repeat, reorder or drop such accesses.
`readb()`/`writeb()`/`ioread8()`/... are volatile accesses with the right
ordering, and sparse warns about a direct dereference.

## access width: watching the bus on the panel

ldd-io redraws the panel once after **every write access** to MATRIX,
whatever its width, and MMIO allows 1, 2, 4 and 8 byte accesses (port I/O
stops at 4, there is no `outq`). A write of all eight rows from offset 0
uses the access width set by the `width` parameter, so the panel shows how
many accesses one picture took:

| `width`     | full picture written as | redraws |
|-------------|-------------------------|---------|
| `8` default | one `writeq()`          | 1       |
| `4`         | two `iowrite32()`       | 2       |
| `2`         | four `iowrite16()`      | 4       |
| `1`         | eight `iowrite8()`      | 8       |
| `0`         | `memcpy_toio()`         | whatever the architecture's copy does |

`width` can be changed while the module is loaded:

```sh
echo 1 > /sys/module/short_mmio/parameters/width
```

Its `module_param_cb()` setter rejects other values. Partial writes always
use `iowrite8()` per row; a full read uses one `readq()`.

`memcpy_toio()` copies a buffer to device memory with whatever widths the
architecture finds fastest. That is fine for a buffer that is just memory,
such as a frame buffer, and wrong for a register whose access width
matters.

### endianness

The `ioread*()`/`iowrite*()`/`readq()`/`writeq()` accessors are little
endian, like ldd-io: the lowest byte of the value goes to the lowest
address. `iowrite32(0x000000ff, io + 8)` therefore lights the **top** row
(offset 8). `iowrite32be()` would put that byte at offset 11 and light the
fourth row instead.

The rows come from user space as bytes. `get_unaligned_le64()` reads the
eight bytes as a little-endian value, so `rows[0]` is the lowest byte and
lands at offset 8 on any CPU. "Unaligned" because `rows` is a byte array
with no 8-byte alignment guarantee; the helper reads it safely where a cast
to `u64 *` would not.

## older ways of doing the same

- LDD3 followed every `ioread8()`/`iowrite8()` with `rmb()`/`wmb()`. The
  accessors are already ordered against each other and against memory, so
  the barriers are gone. The `*_relaxed()` variants, such as
  `readl_relaxed()`, drop the ordering against normal memory for speed and
  are the ones that may need explicit barriers.
- `register_chrdev()` registered a major with all 256 minors in one call;
  `alloc_chrdev_region()` and `cdev_add()` reserve only the minors used.
- Drivers bound to a `struct device` use the managed `devm_ioremap()` or
  `devm_platform_ioremap_resource()`, which unmap and release on their own.
  This example has no device, so it maps and unmaps by hand.
- `ioremap_wc()` maps with write-combining, letting the CPU merge
  neighbouring writes. That suits frame buffers, never registers.

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
README) and add the device in memory space with `mmio=on`, for example at
the end of `bin/boot.sh`:

```bash
    -chardev socket,id=panel0,host=127.0.0.1,port=7100,server=on,wait=off \
    -chardev socket,id=kbd0,host=127.0.0.1,port=7101,server=on,wait=off \
    -device ldd-io,panel=panel0,keyboard=kbd0,mmio=on
```

In two more host terminals, watch the panel and open the keyboard:

```bash
nc 127.0.0.1 7100      # panel
nc 127.0.0.1 7101      # keyboard
```

## Usage

Copy **load_module.sh** and **short_mmio.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

To use another address, e.g. with `-device ldd-io,...,mmiobase=0xfe000000`,
load the module by hand with `insmod short_mmio.ko base=0xfe000000` and
create the two nodes as `load_module.sh` does (minor 0 and 1).

## test the module

```bash
grep short_mmio /proc/iomem

# LEDs and receive, as with port I/O
printf '\x55' > /dev/short_mmio_data
cat /dev/short_mmio_data

# a whole picture in one writeq, then just the third row
printf '\x3c\x42\xa5\x81\xa5\x99\x42\x3c' > /dev/short_mmio_matrix
printf '\xff' | dd of=/dev/short_mmio_matrix bs=1 seek=2 conv=notrunc
hexdump -C /dev/short_mmio_matrix

# the same picture as eight byte writes: the panel redraws row by row
echo 1 > /sys/module/short_mmio/parameters/width
printf '\x3c\x42\xa5\x81\xa5\x99\x42\x3c' > /dev/short_mmio_matrix
```

---

### ¶ The end
