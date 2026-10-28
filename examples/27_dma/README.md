# dma

DMA with the `ldd-pcie` device (`1234:cafe`), a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-pcie.md`](../../tools/qemu/ldd-pcie.md). ldd-pcie has 1 MiB
of memory of its own and a DMA engine that copies between it and system
memory. The driver turns that memory into `/dev/dma`: `write()` puts data
there by DMA, `read()` fetches it back by DMA, at the file position.

## how a transfer goes

The engine is told what to copy by a table of descriptors in system
memory, each a bus address and a length. This driver always uses one
descriptor; scatter-gather DMA uses many.

1. `write()` copies the user's data into the driver's buffer and maps it
   with `dma_map_single()`, which gives its bus address.
2. It fills the descriptor with that address and the length, and writes
   to the device's registers where the table is, how many descriptors,
   where in the on-board memory, which direction, and start.
3. The device reads the descriptor and the data, by DMA, and then
   interrupts with DMA_DONE, or DMA_ERROR.
4. The handler clears the interrupt and calls `complete()`; `write()` was
   waiting in `wait_for_completion_timeout()`, unmaps the buffer and
   returns.

`read()` is the same the other way round, then copies the buffer to the
user. A call moves at most one buffer, 64 KiB, and returns how much it
moved, as `read()` and `write()` may.

ldd-pcie's registers used here, all in BAR 0, 32 bits wide:

| offset | register | meaning |
|--------|----------|---------|
| `0x08` | CAPS | bit 0: the engine drives all 64 address bits |
| `0x10` | CTRL | bit 0: reset |
| `0x14` | STATUS | bits 15:8: why the last transfer failed |
| `0x18` | INT_STATUS | bit 0 DMA done, bit 1 DMA error; write 1 to clear |
| `0x1c` | INT_MASK | 1: the event interrupts |
| `0x24` | MAX_SEG | largest length of one descriptor |
| `0x40`, `0x44` | DESC_ADDR_LO, _HI | bus address of the descriptor table |
| `0x48` | DESC_COUNT | number of descriptors |
| `0x4c` | DEV_OFFSET | where in the on-board memory |
| `0x50` | DMA_CTRL | bit 0: device to RAM, else RAM to device; bit 1: start |
| `0x5c` | XOR | byte XORed into every byte sent to RAM |

## what each part shows

- **Bus addresses.** The device is given bus addresses (`dma_addr_t`), as
  returned by the DMA API, never kernel pointers or physical addresses
  worked out by hand. Without an IOMMU a bus address is the physical
  address; with one, it is an address the IOMMU translates, and the
  device can reach nothing else. The driver prints both for its buffer.
- **The DMA mask.** `dma_set_mask_and_coherent()` tells the DMA API which
  addresses the device can reach: all 64 bits, or the lower 32 if CAPS
  says so. The API then hands out only those, using the IOMMU or a bounce
  buffer when it must.
- **Bus mastering.** `pci_set_master()` lets the device start memory
  accesses: DMA, and MSI.
- **Coherent DMA, for the descriptor table.** `dma_alloc_coherent()`
  allocated once: the CPU writes the descriptor, the device reads it, and
  each sees the other's writes with no syncing. It suits small structures
  used again and again.
- **Streaming DMA, for the data.** The buffer is ordinary `kmalloc()`
  memory, mapped for each transfer with `dma_map_single()` in one
  direction and unmapped after with `dma_unmap_single()`. Mapping hands
  the buffer to the device and unmapping hands it back: in between the
  CPU must not touch it. On the way the DMA API flushes caches, sets up
  the IOMMU or copies through a bounce buffer, as the platform needs.
- **Byte order.** The descriptor is read by the device, which expects
  little endian: its fields are `__le64` and `__le32`, filled with
  `cpu_to_le64()` and `cpu_to_le32()`. Registers need no conversion,
  `iowrite32()` does it; and the 64-bit table address is written in two
  halves, as the registers take only 32-bit accesses.
- **Interrupts.** One vector, MSI or INTx. Not MSI-X: there DMA_ERROR has
  a vector of its own, and with one vector its interrupt would be lost.
- **Errors and timeouts.** A failed transfer reports why in STATUS; the
  driver logs it and returns `-EIO`. If no interrupt comes, the device may
  still be using the buffer, which must not be unmapped yet: the driver
  resets the device, which abandons the transfer, before going on.
- **Proof that the data went through the device.** With the module
  parameter `xor` set, the device XORs every byte it sends to RAM with
  it: what is read back is not what was written, so it was not copied by
  the CPU. It can be changed while loaded, in
  `/sys/module/dma/parameters/xor`.
- **Removal while open.** `/dev/dma` is a `struct device` with a devt,
  added together with its cdev by `cdev_device_add()`; devtmpfs creates the
  node. The cdev and every open file hold a reference to that device, and
  its `release()` frees the driver's structure. `remove()` deletes the node,
  marks the device gone so that open files get `-ENODEV`, and releases the
  hardware; the structure goes with the last close.

## shorter ways of writing the same

The comments in `main.c` name them:

- `pci_request_irq()` chooses `IRQF_SHARED` by itself.
- `pcim_enable_device()`, `pcim_iomap_region()`, `devm_request_irq()`,
  `devm_kmalloc()` and `dmam_alloc_coherent()` are managed versions,
  undone automatically.
- `lo_hi_writeq()` writes a 64-bit value as two 32-bit halves, low first.

## older ways of doing the same

- `pci_alloc_consistent()`, `pci_map_single()` and `pci_set_dma_mask()`
  are the old PCI names of `dma_alloc_coherent()`, `dma_map_single()`
  and `dma_set_mask()`.
- `iowrite32(cpu_to_le32(x))`: `iowrite32()` already converts to little
  endian, so the extra `cpu_to_le32()` swaps the value back on a
  big-endian CPU.

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
sh qemu.sh                                     # defaults
sh qemu.sh -global ldd-pcie.dma64=off          # 32-bit DMA only
sh qemu.sh -global ldd-pcie.latency-us=1500000 # every transfer times out
```

With an emulated IOMMU every DMA address is translated and checked. This
needs the machine options changed, so it runs QEMU without `boot.sh`; see
`tools/qemu/ldd-pcie.md` for the options to add.

## Usage

Copy **load_module.sh**, **dma.ko** and **test/dma.out** to the target
machine, then run:

```bash
sh load_module.sh
sh load_module.sh load xor=0x20
```

## test the module

```bash
./dma.out
echo abc > /dev/dma
head -c 3 /dev/dma
```

```
ok   write "hello dma" at offset 0
ok   read it back
ok   with xor=0x20 it comes back as "HELLO\0DMA"
ok   write 200 KiB: 4 calls of at most 64 KiB
ok   read the 200 KiB back intact
ok   write at the end stops at 1 MiB
ok   read past the end returns 0
ok   write past the end fails with ENOSPC
PASSED
```

`dmesg` shows the buffer's addresses. Without an IOMMU they are the same:

```
dma 0000:01:00.0: using MSI, 64-bit DMA, descriptors at bus 0x0000000001e2c000
dma 0000:01:00.0: buffer at phys 0x0000000001e40000 is at bus 0x0000000001e40000
```

With the IOMMU they are not:

```
dma 0000:01:00.0: using MSI, 64-bit DMA, descriptors at bus 0x00000000fffff000
dma 0000:01:00.0: buffer at phys 0x0000000002030000 is at bus 0x00000000ffffe000
```

With `latency-us=1500000` a transfer times out after a second:

```
dma 0000:01:00.0: DMA timed out, resetting the device
sh: write error: Operation timed out
```

To see an open file outlive the device, unbind it while a file is open:

```bash
A=$(basename $(ls -d /sys/bus/pci/drivers/dma/0000:*))
exec 3<>/dev/dma
echo $A > /sys/bus/pci/drivers/dma/unbind     # /dev/dma disappears
cat <&3                                        # No such device
exec 3<&-                                      # the structure is freed now
echo $A > /sys/bus/pci/drivers/dma/bind
```

---

### ¶ The end
