# ldd-pcie

`ldd-pcie` is a PCI Express endpoint with a register BAR, 1 MiB of on-board
memory and a scatter-gather DMA engine. The engine copies between guest
memory, described by a table of descriptors, and the on-board memory, and
raises an interrupt when it is done. Interrupts are MSI-X, MSI or INTx.

## Overview

```
        guest memory                     ldd-pcie
   +--------------------+       +-------------------------------+
   | descriptor table   | <-----| DMA engine                    |
   |  [0] addr,len  ----+--+    |   reads descriptors           |
   |  [1] addr,len  ----+--|-+  |   copies segment by segment   |
   |  ...               |  | |  |        ^            |         |
   +--------------------+  | |  |        |            v         |
   | buffer pages       |<-+ |  |   +-------------------------+ |
   |   (scattered)      |<---+--|-->| on-board memory, 1 MiB  | |
   +--------------------+       |   +-------------------------+ |
                                |        ^  BAR2 (CPU access)  |
   ioread32/iowrite32 ----------+--> BAR0 registers            |
                                |   INT_STATUS & INT_MASK -----+--> MSI-X / MSI / INTx
                                +-------------------------------+
```

## PCI identity

| Field              | Value |
|--------------------|-------|
| Vendor ID          | 0x1234 |
| Device ID          | 0xcafe |
| Revision           | 0x01 |
| Class code         | 0x080100, system peripheral: DMA controller |
| Subsystem          | QEMU default |
| Interrupt pin      | INTA# |
| Capabilities       | PCI Express endpoint, MSI (1 vector), MSI-X (3 vectors) |

The device must sit on a PCI Express bus, behind a root port on the `q35`
machine (see [Properties](#properties)).

## BARs

| BAR | Size  | Type                          | Content |
|-----|-------|-------------------------------|---------|
| 0   | 4 KiB | memory, 64-bit, non-prefetchable | [registers](#registers), MSI-X table and PBA |
| 2   | 1 MiB | memory, 64-bit, prefetchable  | [on-board memory](#on-board-memory) |

A 64-bit BAR takes two slots, so BAR1 and BAR3 are the upper halves and
there are no BAR4 or BAR5.

### BAR0 layout

| Offset        | Content |
|---------------|---------|
| 0x000 - 0x0ff | registers |
| 0x100 - 0x7ff | reserved, read as 0 |
| 0x800 - 0x82f | MSI-X table, 3 entries |
| 0xc00 - 0xc07 | MSI-X PBA |

Registers are 32 bits wide. Only naturally aligned 4-byte accesses are
supported; other widths are undefined. All values are little endian.

## Registers

| Offset | Name         | Access | Reset |
|--------|--------------|--------|-------|
| 0x00   | ID           | RO     | 0x4c444450 ("LDDP") |
| 0x04   | VERSION      | RO     | 0x00010000 |
| 0x08   | CAPS         | RO     | see below |
| 0x0c   | SCRATCH      | R/W    | 0x00000000 |
| 0x10   | CTRL         | W      | 0x00000000 |
| 0x14   | STATUS       | RO     | 0x00000000 |
| 0x18   | INT_STATUS   | R/W1C  | 0x00000000 |
| 0x1c   | INT_MASK     | R/W    | 0x00000000 |
| 0x20   | INT_RAISE    | W      | -     |
| 0x24   | MAX_SEG      | RO     | `max-seg` |
| 0x28   | MAX_DESC     | RO     | `max-desc` |
| 0x40   | DESC_ADDR_LO | R/W    | 0     |
| 0x44   | DESC_ADDR_HI | R/W    | 0     |
| 0x48   | DESC_COUNT   | R/W    | 0     |
| 0x4c   | DEV_OFFSET   | R/W    | 0     |
| 0x50   | DMA_CTRL     | R/W    | 0     |
| 0x54   | XFER_BYTES   | RO     | 0     |
| 0x58   | ERR_DESC     | RO     | 0     |
| 0x5c   | XOR          | R/W    | 0x00  |

Reserved bits and offsets read as 0. Software must write them as 0.

| Access | Meaning |
|--------|---------|
| RO     | read only; writes are ignored |
| R/W    | reads back what was written |
| W      | write only; reads as 0 |
| W1C    | write 1 to clear: writing 1 to a bit clears it, writing 0 leaves it unchanged |
| SC     | self-clearing: writing 1 starts an action; the bit reads as 0 |

### 0x00: ID, 0x04: VERSION

ID is the constant 0x4c444450. A driver checks it in probe before touching
anything else. VERSION is major in bits 31:16, minor in bits 15:0.

### 0x08: CAPS

| Bit  | Name  | Meaning |
|------|-------|---------|
| 0    | DMA64 | 1: the engine reaches all 64 address bits. 0: only addresses below 4 GiB; anything above fails with DMA_FAULT. Set by the `dma64` property. |
| 31:1 | -     | Reserved |

### 0x0c: SCRATCH

Holds whatever is written to it. It has no effect on the device: use it to
test register access.

### 0x10: CTRL

| Bit  | Name  | Access | Meaning |
|------|-------|--------|---------|
| 0    | RESET | SC     | Write 1: [reset](#reset) the device. |
| 31:1 | -     | -      | Reserved |

CTRL always reads as 0.

### 0x14: STATUS

| Bit   | Name     | Meaning |
|-------|----------|---------|
| 0     | BUSY     | 1: a transfer is in progress. |
| 1     | DONE     | 1: the last transfer finished without error. |
| 2     | ERROR    | 1: the last transfer failed; ERR_CODE says why. |
| 7:3   | -        | Reserved |
| 15:8  | ERR_CODE | See [Errors](#errors). 0 when ERROR is 0. |
| 31:16 | -        | Reserved |

DONE and ERROR are cleared when the next transfer starts.

### 0x18: INT_STATUS, 0x1c: INT_MASK, 0x20: INT_RAISE

The three registers share one layout:

| Bit  | Name      | Set when | MSI-X vector |
|------|-----------|----------|--------------|
| 0    | DMA_DONE  | a transfer finishes without error | 0 |
| 1    | DMA_ERROR | a transfer fails | 1 |
| 2    | SOFT      | software writes 1 to INT_RAISE bit 2 | 2 |
| 31:3 | -         | Reserved | |

- INT_STATUS bits are set by the events above, whatever INT_MASK holds.
  Writing 1 clears them.
- INT_MASK: 1 enables the interrupt for that bit. Reset leaves all disabled.
- INT_RAISE: writing 1 to a bit sets the same bit in INT_STATUS, as if the
  event had happened. Any of the three bits can be raised; it is meant for
  testing interrupt handlers without a transfer.

See [Interrupts](#interrupts) for how bits become interrupts.

### 0x24: MAX_SEG, 0x28: MAX_DESC

MAX_SEG is the largest length one descriptor may have, in bytes. MAX_DESC is
the largest DESC_COUNT. A driver passes MAX_SEG to `dma_set_max_seg_size()`
and limits the entries of its scatterlist to MAX_DESC.

### 0x40 - 0x5c: DMA engine

| Register     | Meaning |
|--------------|---------|
| DESC_ADDR_LO/HI | Bus address of the descriptor table, 16-byte aligned. Bits 3:0 are ignored. |
| DESC_COUNT   | Number of descriptors, 1 to MAX_DESC. |
| DEV_OFFSET   | Where in the on-board memory the transfer starts. |
| DMA_CTRL     | See below. |
| XFER_BYTES   | Bytes copied by the last transfer. |
| ERR_DESC     | Index of the descriptor at which the last transfer failed. |
| XOR          | Bits 7:0: every byte copied from device to guest is XORed with this value. 0 copies unchanged. |

DMA_CTRL:

| Bit  | Name  | Access | Meaning |
|------|-------|--------|---------|
| 0    | DIR   | R/W    | 0: guest to device. 1: device to guest. |
| 1    | START | SC     | Write 1: start a [transfer](#transfer). |
| 31:2 | -     | -      | Reserved |

Writes to DESC_*, DEV_OFFSET and DMA_CTRL.DIR while BUSY is 1 are ignored.

## Descriptors

The descriptor table is an array of DESC_COUNT entries in guest memory, read
by the device with DMA. Each entry is 16 bytes, little endian:

```c
struct ldd_pcie_desc {
        __le64 addr;    /* bus address of the segment */
        __le32 len;     /* bytes, 1 to MAX_SEG */
        __le32 flags;   /* reserved, must be 0 */
};
```

Segments may start at any address and have any length up to MAX_SEG. They
are copied in table order; the on-board side is one contiguous range
starting at DEV_OFFSET:

```
   desc[0]  addr A, len 4096  <->  on-board DEV_OFFSET + 0
   desc[1]  addr B, len 4096  <->  on-board DEV_OFFSET + 4096
   desc[2]  addr C, len  512  <->  on-board DEV_OFFSET + 8192
```

The device reads the table once, when the transfer starts. Changing it
afterwards has no effect on that transfer.

## Transfer

1. Software fills the descriptor table and writes DESC_ADDR_LO/HI,
   DESC_COUNT, DEV_OFFSET and DMA_CTRL.DIR.
2. Software writes DMA_CTRL with START set. The device sets BUSY and clears
   DONE, ERROR, XFER_BYTES and ERR_DESC.
3. The device reads the table and checks every descriptor. If one is
   invalid, the transfer fails before any data is moved.
4. The device copies the segments in order.
5. After `latency-us` microseconds the transfer completes: BUSY is cleared,
   DONE or ERROR is set, and INT_STATUS.DMA_DONE or DMA_ERROR is set.

Writing START while BUSY is 1 is ignored. The transfer runs in the
background: software must wait for the interrupt, or poll STATUS.BUSY; it
must not touch the buffers until then.

DMA needs the Bus Master Enable bit in the PCI command register
(`pci_set_master()`). Without it, START fails with NO_MASTER.

## Errors

| ERR_CODE | Name         | Cause | Data moved |
|----------|--------------|-------|------------|
| 1        | NO_MASTER    | Bus Master Enable is 0 | none |
| 2        | BAD_COUNT    | DESC_COUNT is 0 or above MAX_DESC | none |
| 3        | BAD_DESC     | a descriptor has len 0, len above MAX_SEG, or flags not 0 | none |
| 4        | OUT_OF_RANGE | the segments add up past the end of the on-board memory | none |
| 5        | DMA_FAULT    | a DMA access failed: an IOMMU fault, an address above 4 GiB without CAPS.DMA64, or no memory at the address | up to ERR_DESC |

For BAD_DESC and DMA_FAULT, ERR_DESC is the index of the faulting
descriptor. After a DMA_FAULT, segments before ERR_DESC have been copied and
XFER_BYTES counts them. Faults reading the table itself report ERR_DESC 0.

## Interrupts

An interrupt is wanted while `INT_STATUS & INT_MASK` is not 0. How it is
delivered depends on what the driver enabled:

| Mode  | Vectors | Delivery |
|-------|---------|----------|
| MSI-X | 3       | Bit *n* of INT_STATUS uses vector *n*. A message is sent when the bit becomes set while unmasked, or when INT_MASK unmasks a bit that is already set. |
| MSI   | 1       | Same rule, all bits share the vector. |
| INTx  | 1       | Level triggered: INTA# is asserted while `INT_STATUS & INT_MASK` is not 0. The handler must clear INT_STATUS before returning. |

MSI-X is used if enabled, otherwise MSI, otherwise INTx. With MSI or MSI-X
one message may stand for several events: the handler reads INT_STATUS and
clears what it handled.

## On-board memory

1 MiB, the target of every transfer. BAR2 maps it for the CPU as well:

- Accesses of 1, 2, 4 or 8 bytes, naturally aligned, little endian.
- Prefetchable: it may be mapped write-combined (`ioremap_wc()`,
  `pci_mmap_resource_range()` from user space through sysfs).
- CPU access to the range of a transfer that is in progress is undefined.

Its contents are cleared at power on, not by reset.

## Reset

CTRL.RESET and a PCI reset (function level reset or bus reset) do the same:

| State          | Value |
|----------------|-------|
| transfer       | an in-progress transfer is abandoned; no interrupt |
| STATUS         | 0 |
| INT_STATUS     | 0 |
| INT_MASK       | 0, all interrupts disabled |
| DMA engine registers | 0 |
| SCRATCH, XOR   | 0 |
| on-board memory | unchanged |

## Properties

| Property     | Default | Meaning |
|--------------|---------|---------|
| `dma64`      | on      | off: the engine reaches only addresses below 4 GiB (CAPS.DMA64 = 0) |
| `max-seg`    | 65536   | MAX_SEG in bytes, 512 to 1048576, a power of 2 |
| `max-desc`   | 256     | MAX_DESC, 1 to 4096 |
| `latency-us` | 100     | time from START to completion |

```bash
-M q35 \
-device pcie-root-port,id=rp0,bus=pcie.0,chassis=1 \
-device ldd-pcie,bus=rp0                                 # defaults
-device ldd-pcie,bus=rp0,dma64=off,max-seg=4096          # tight limits
```

With an emulated IOMMU every DMA address is translated and checked:

```bash
-M q35,kernel-irqchip=split -device intel-iommu,intremap=on \
-append '... intel_iommu=on iommu.passthrough=0'
```

Debug output: `-d guest_errors` logs bad register accesses and failed
transfers.

## Programming notes

### Probe

```c
pcim_enable_device(pdev);
regs = pcim_iomap_region(pdev, 0, "ldd-pcie");
if (ioread32(regs + ID) != 0x4c444450)
        return -ENODEV;
pci_set_master(pdev);
dma_set_mask_and_coherent(&pdev->dev,
        ioread32(regs + CAPS) & DMA64 ? DMA_BIT_MASK(64) : DMA_BIT_MASK(32));
dma_set_max_seg_size(&pdev->dev, ioread32(regs + MAX_SEG));
```

### Interrupts

```c
nvec = pci_alloc_irq_vectors(pdev, 1, 3, PCI_IRQ_ALL_TYPES);
/* nvec == 3: one handler per vector; otherwise one handler reads INT_STATUS */
iowrite32(DMA_DONE | DMA_ERROR | SOFT, regs + INT_MASK);

iowrite32(SOFT, regs + INT_RAISE);         /* test the handler */
```

### A scatter-gather transfer

```c
desc = dma_alloc_coherent(dev, MAX_DESC * 16, &desc_dma, GFP_KERNEL);

n = dma_map_sgtable(dev, sgt, DMA_TO_DEVICE, 0);
for_each_sgtable_dma_sg(sgt, sg, i) {      /* DMA segments, not pages */
        desc[i].addr  = cpu_to_le64(sg_dma_address(sg));
        desc[i].len   = cpu_to_le32(sg_dma_len(sg));
        desc[i].flags = 0;
}

iowrite32(lower_32_bits(desc_dma), regs + DESC_ADDR_LO);
iowrite32(upper_32_bits(desc_dma), regs + DESC_ADDR_HI);
iowrite32(sgt->nents, regs + DESC_COUNT);
iowrite32(0, regs + DEV_OFFSET);
iowrite32(START, regs + DMA_CTRL);         /* DIR 0: guest to device */

wait_for_completion(&done);                /* completed by the handler */
dma_unmap_sgtable(dev, sgt, DMA_TO_DEVICE, 0);
```

With an IOMMU, `sgt->nents` is often smaller than the number of pages,
because contiguous bus addresses are merged.

### Check a transfer from the CPU

```c
mem = pcim_iomap_region(pdev, 2, "ldd-pcie-mem");
memcpy_fromio(buf, mem + offset, len);     /* compare with what was sent */
```

## Experiments

| Topic | What to use |
|-------|-------------|
| PCI probe, config space, BAR mapping | ID, SCRATCH, BAR0 |
| MMIO access, endianness | SCRATCH, BAR2 with 1/2/4/8-byte accesses |
| mmap of device memory to user space | BAR2 |
| INTx, MSI, MSI-X, shared and per-vector handlers | INT_RAISE, `pci_alloc_irq_vectors()` |
| coherent DMA | the descriptor table |
| streaming DMA of one buffer | one descriptor |
| scatter-gather DMA, user pages | many descriptors, `pin_user_pages()` |
| DMA mask and bounce buffers | `dma64=off` |
| segment limits | `max-seg`, `max-desc` |
| IOMMU translation and faults | `intel-iommu`, a wrong address gives DMA_FAULT |
| error paths and reset | [Errors](#errors), CTRL.RESET |
| proof that data went through the device | XOR |
