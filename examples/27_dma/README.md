# dma

DMA with QEMU's `edu` device (`1234:11e8`). edu has a 4 KB buffer of its
own and a DMA engine that copies between it and system memory. The driver
turns that buffer into `/dev/dma`: `write()` puts data there by DMA,
`read()` fetches it back by DMA, at the file position.

## how a transfer goes

1. `write()` copies the user's data into a DMA buffer the driver allocated
   with `dma_alloc_coherent()`.
2. It writes the source (the buffer's bus address), the destination
   (`0x40000` plus the file position, in edu's own address space), the
   length, and finally the command to edu's registers.
3. edu runs the transfer about 100 ms later, reading system memory itself,
   and then interrupts with `0x100` in its interrupt status.
4. The handler acks the interrupt and calls `complete()`; `write()` was
   waiting in `wait_for_completion_timeout()` and returns.

`read()` is the same the other way round, then copies the buffer to the
user.

edu's registers used here:

| offset | meaning |
|--------|---------|
| `0x24` | interrupt status |
| `0x64` | interrupt ack |
| `0x80` | DMA source address, 64 bits |
| `0x88` | DMA destination address, 64 bits |
| `0x90` | DMA length |
| `0x98` | DMA command: `0x01` start, `0x02` edu to RAM, `0x04` interrupt when done |

## what each part shows

- **Bus addresses.** `dma_alloc_coherent()` returns two addresses for the
  same memory: a kernel pointer for the CPU and a `dma_addr_t` for the
  device. With an IOMMU they differ; the device must only ever be given the
  `dma_addr_t`.
- **The DMA mask.** edu only drives 28 address bits.
  `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(28))` tells the DMA API, which
  then hands out only addresses below 256 MB, using a bounce buffer or the
  IOMMU when it must.
- **Bus mastering.** `pci_set_master()` lets the device start memory
  accesses: DMA, and MSI.
- **Coherent and streaming DMA.** Coherent memory needs no syncing: the CPU
  and the device see each other's writes. It suits buffers used again and
  again, like this one. Data that lives in a buffer of its own is mapped
  per transfer instead, with `dma_map_single()` and `dma_unmap_single()`
  (streaming DMA).
- **Byte order.** `writeq()` stores the address in little endian, as PCI
  registers are, so the driver passes it as is. The data in the buffer are
  plain bytes that edu copies unchanged. Structures in DMA memory that the
  device interprets, like descriptors, would need `__le32` fields and
  `cpu_to_le32()`, since the CPU writes them with ordinary stores.
- **Waiting for the interrupt.** A `completion` is how a thread waits for
  one event signalled from an interrupt handler.
- **Removal while open.** `/dev/dma` is a `struct device` with a devt,
  added together with its cdev by `cdev_device_add()`; devtmpfs creates the
  node. The cdev and every open file hold a reference to that device, and
  its `release()` frees the driver's structure. `remove()` deletes the node,
  marks the device gone so that open files get `-ENODEV`, and releases the
  hardware; the structure goes with the last close.

## shorter ways of writing the same

The comments in `main.c` name them:

- `pci_request_irq()` chooses `IRQF_SHARED` by itself.
- `pcim_enable_device()`, `pcim_iomap_region()`, `devm_request_irq()` and
  `dmam_alloc_coherent()` are managed versions, undone automatically.

## older ways of doing the same

- Two `iowrite32()`s for a 64-bit register, or `iowrite32(cpu_to_le32(x))`:
  `iowrite32()` already converts to little endian, so the extra
  `cpu_to_le32()` swaps the value back on a big-endian CPU. `writeq()`
  writes the whole register at once.
- `pci_alloc_consistent()` and `pci_set_dma_mask()` are the old PCI names
  of `dma_alloc_coherent()` and `dma_set_mask()`.

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

`$LDD_ROOT/bin/boot.sh` already adds `-device edu`, so the guest started
with it has the device.

## Usage

Copy **load_module.sh**, **dma.ko** and **test/dma.out** to the target
machine, then run:

```bash
sh load_module.sh               # MSI
sh load_module.sh load msi=0    # INTx
```

## test the module

```bash
./dma.out
echo abc > /dev/dma
head -c 3 /dev/dma
```

```
ok   write "hello dma" to edu at offset 0
ok   write waited for the DMA: 114 ms
ok   read it back from edu
ok   read waited for the DMA: 104 ms
ok   write at the end stops at 4096 bytes
ok   read past the end returns 0
ok   write past the end fails with ENOSPC
PASSED
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
