# dma_sg

Scatter-gather DMA with the `ldd-pcie` device (`1234:cafe`), a QEMU device
kept in [`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-pcie.md`](../../tools/qemu/ldd-pcie.md). `/dev/dma_sg` is the
device's 1 MiB of on-board memory: `read()` and `write()` move the data
by DMA straight between it and the caller's buffer, with no copy through
a buffer of the driver's.

## why scatter-gather

A buffer in a process is contiguous in virtual memory only: its pages are
wherever the kernel found free memory. A device that can take only one
address and a length would need the data copied into one contiguous
buffer first. ldd-pcie takes a table of descriptors instead, each a bus
address and a length, and moves them all in one transfer:

```
user buffer: contiguous     its pages: scattered        descriptor table
in virtual memory           in physical memory
+------------+              +--------+
| page 0     | -----------> | page 0 | <-------------- [0] addr, 4096
| page 1     | ------.      +--------+
| page 2     | ---.  |      +--------+
+------------+    |  '----> | page 1 | <-------------- [1] addr, 4096
                  |         +--------+
                  |         +--------+
                  '-------> | page 2 | <-------------- [2] addr, 4096
                            +--------+
```

## how a transfer goes

1. `pin_user_pages_fast()` finds the pages behind the buffer and keeps
   them in memory while the device uses them. For `read()` the device
   writes to them, so they are pinned with `FOLL_WRITE`.
2. `sg_alloc_table_from_pages_segment()` builds a scatterlist from them,
   joining pages that happen to be next to each other, up to the largest
   segment the device takes. The first and last entries cover only the
   part of their page that is in the buffer.
3. `dma_map_sgtable()` maps the list for the device. With an IOMMU the
   mapped segments can be fewer than the entries: the IOMMU gives
   scattered pages contiguous bus addresses, and they are joined.
4. Each mapped segment, walked with `for_each_sgtable_dma_sg()`, becomes a
   descriptor: `sg_dma_address()` and `sg_dma_len()`, not the pages.
5. One transfer for all of them, waiting for the interrupt, then
   `dma_unmap_sgtable()`, `sg_free_table()` and unpinning. Pages the
   device wrote to are marked dirty, with
   `unpin_user_pages_dirty_lock()`, so that their new contents are not
   lost if they belong to a file.

A call moves at most what `MAX_DESC` pages hold, 1 MiB with the default 256,
and returns how much it moved.

## the device's limits

- `MAX_SEG`, the largest length of one descriptor, goes to
  `dma_set_max_seg_size()`: the scatterlist, and the IOMMU when it joins
  segments, keep within it. The scatterlist is built from whole pages, so
  the driver refuses a device whose segments are smaller than a page.
- `MAX_DESC`, the most descriptors in a table, limits the pages of one call.
- The descriptor table is coherent memory for `MAX_DESC` descriptors.

## seeing the segments

`/sys/bus/pci/devices/<addr>/dma_sg/last` shows the last transfer: pages
pinned, scatterlist entries, and descriptors after mapping. For a 300 KiB
buffer:

| QEMU | pages | entries | descriptors |
|------|-------|---------|-------------|
| defaults | 76 | 76 | 76 |
| with the IOMMU | 76 | 76 | 5 |
| with the IOMMU and `max-seg=4096` | 76 | 76 | 76 |
| `max-desc=4` | 4 per call | 4 | 4 |

Without an IOMMU, entries are joined only where `malloc()` happened to get
physically adjacent pages, which varies; with it, the IOMMU joins them up
to `MAX_SEG`, 64 KiB here.

## the rest

- The DMA mask comes from CAPS: 64 or 32 bits.
- One interrupt vector, MSI or INTx; not MSI-X, where DMA_ERROR has a
  vector of its own that a single vector would lose.
- A DMA error is logged with its code and the failing descriptor, and
  returns `-EIO`. After a timeout the device is reset before the pages are
  unmapped, since it may still be using them.
- With the module parameter `xor` set, the device XORs every byte it sends
  to RAM: proof that the data came through the device.
- The file may stay open after the device is removed: reads and writes
  then fail with `-ENODEV`, and the driver's structure goes with the last
  close.

## shorter ways of writing the same

- `pcim_enable_device()`, `pcim_iomap_region()`, `devm_request_irq()` and
  `dmam_alloc_coherent()` are managed versions, undone automatically.
- `pin_user_pages_fast()` with `FOLL_LONGTERM` is for buffers the device
  keeps for long, such as RDMA; this one only for a transfer.

## older ways of doing the same

- `get_user_pages()` and `put_page()`: `pin_user_pages()` and
  `unpin_user_page()` replaced them for pages whose contents are accessed
  by DMA.
- `dma_map_sg()` returns the number of mapped entries and 0 on failure;
  `dma_map_sgtable()` keeps it in `sgt->nents` and returns an error code.
- `sg_alloc_table_from_pages()` without a segment limit: the limit then
  had to be applied by hand.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

Build the test program, statically linked so that it runs in the guest:

```bash
make -C test
```

## run QEMU with the device

**RUN ON HOST:** build QEMU with the devices from `tools/qemu` (see its
README), then boot the guest with `qemu.sh` from this directory. It runs
`$LDD_ROOT/bin/boot.sh`, which already has the `q35` machine with a PCI
Express root port and `-device ldd-pcie` behind it. Its properties can be
changed with `-global`:

```bash
sh qemu.sh                                                   # defaults
sh qemu.sh -global ldd-pcie.max-desc=4                       # 4 pages a call
sh qemu.sh -global ldd-pcie.max-seg=512                      # refused
```

The IOMMU needs the machine options changed, so it runs QEMU without
`boot.sh`; see `tools/qemu/ldd-pcie.md` for the options to add.

## Usage

Copy **load_module.sh**, **dma_sg.ko** and **test/dma_sg.out** to the
target machine, then run:

```bash
sh load_module.sh
```

## test the module

The test takes the device's PCI address, to find the `last` file:

```bash
./dma_sg.out $(basename $(ls -d /sys/bus/pci/drivers/dma_sg/0000:*))
```

```
ok   write "hello dma_sg" at offset 0
ok   read it back
     a few bytes:             pages 1 segments 1 descriptors 1
ok   with xor=0x20 it comes back as "HELLO"
     300 KiB, unaligned:      pages 76 segments 76 descriptors 76
ok   write 300 KiB from an unaligned buffer: 1 call
ok   read the 300 KiB back intact
ok   write at the end stops at 1 MiB
ok   read past the end returns 0
ok   write past the end fails with ENOSPC
ok   write from a bad address fails with EFAULT
PASSED
```

---

### ¶ The end
