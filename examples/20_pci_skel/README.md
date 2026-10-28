# pci_skel

The skeleton of a PCI driver, bound to the `ldd-pcie` device (`1234:cafe`),
a QEMU device kept in [`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and
described in [`tools/qemu/ldd-pcie.md`](../../tools/qemu/ldd-pcie.md). The
driver claims and maps the device's two BARs, reads a few registers, writes
to the device's memory and logs what it finds. It has no device node.

## how a PCI driver gets its device

The module does not touch hardware when it is loaded. `m_init()` only registers a `struct pci_driver` with the PCI core:

```c
static const struct pci_device_id skel_ids[] = {
	{ PCI_DEVICE(0x1234, 0xcafe) },
	{ }
};

static struct pci_driver skel_driver = {
	.name     = "pci_skel",
	.id_table = skel_ids,
	.probe    = skel_probe,
	.remove   = skel_remove,
};
```

The firmware and the PCI core have already found every device on the bus,
read its vendor and device IDs from the configuration space, and assigned
addresses to its BARs and an interrupt line. When a driver whose `id_table`
matches is registered, the core calls its `probe()` with that `struct
pci_dev`. When the driver is unloaded, or the device goes away, it calls
`remove()`.

`MODULE_DEVICE_TABLE(pci, skel_ids)` exports the table as module aliases.
`depmod` collects them in `modules.alias`, and udev or mdev uses that to
load the right module when such a device appears.

## what probe does

The usual order for a PCI driver, undone in reverse in `skel_remove()`:

1. `pci_enable_device()` wakes the device up and enables its memory and
   I/O decoding.
2. `pci_request_regions()` claims all the BARs, so no other driver can take
   them; they appear as `pci_skel` in `/proc/iomem`.
3. `pci_iomap()` maps a BAR and returns a `void __iomem *`. For a memory
   BAR this is `ioremap()`, for an I/O BAR `ioport_map()`; the driver uses
   `ioread32()`/`iowrite32()` either way. BAR 0 is mapped first, and the ID
   register checked: a device that answers to the same IDs but is not an
   ldd-pcie is refused with `-ENODEV`. Then BAR 2.
4. `pci_set_drvdata()` hangs the driver's `struct skel_dev`, with both
   mappings, on the device. `probe()`'s locals are gone when it returns
   and `remove()` gets only the `pci_dev`, which `pci_get_drvdata()` gets
   the structure back from. It is per device, not a global, because a
   driver may be bound to several devices at once.

Then it logs:

- vendor, device, revision and class code from the configuration space.
  `080100` is a system peripheral, DMA controller;
- the interrupt pin, the `PCI_INTERRUPT_LINE` register and `pdev->irq`.
  The register is only a note left by the firmware; `pdev->irq` is the
  interrupt the kernel assigned, and the one drivers use;
- whether the device has a PCI Express capability. ldd-pcie is a PCI
  Express endpoint behind a root port of the `q35` machine; a conventional
  PCI driver uses the same `pci_*` calls;
- each BAR, as a `struct resource` printed with `%pR`. Both BARs are
  64-bit, and a 64-bit BAR takes two of the six slots, the second holding
  the upper half of the address: so there is a BAR 0 and a BAR 2, and no
  BAR 1 or 3. BAR 2 is prefetchable (`pref`): reads have no side effects,
  so the CPU may combine and prefetch them;
- registers in BAR 0: the ID, `0x4c444450` or "LDDP", the version and
  whether the DMA engine reaches 64-bit addresses; then SCRATCH, which
  reads back what was written;
- a word written to the on-board memory in BAR 2 and read back.

```
pci_skel 0000:01:00.0: vendor 1234 device cafe revision 01 class 080100
pci_skel 0000:01:00.0: interrupt pin A, line 10, irq 20
pci_skel 0000:01:00.0: PCI Express device
pci_skel 0000:01:00.0: BAR0: [mem 0xfe800000-0xfe800fff 64bit]
pci_skel 0000:01:00.0: BAR2: [mem 0xfe000000-0xfe0fffff 64bit pref]
pci_skel 0000:01:00.0: id LDDP (0x4c444450), version 1.0, 64-bit DMA
pci_skel 0000:01:00.0: scratch: wrote 0x12345678, read 0x12345678
pci_skel 0000:01:00.0: on-board memory: wrote 0xcafef00d, read 0xcafef00d
```

## shorter ways of writing the same

The example spells things out. The comments in `main.c` name the shorter
forms:

- `module_pci_driver(skel_driver)` generates `m_init()` and `m_exit()`.
- `pcim_enable_device()` and `pcim_iomap_region()` are managed versions of
  enable, request and map, and `devm_kzalloc()` of the allocation: they
  are undone automatically when the driver is unbound, so the `goto`
  unwinding and all of `remove()` disappear.
- `pci_is_pcie()` replaces the `pci_find_capability(pdev,
  PCI_CAP_ID_EXP)` test.

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
README), then boot the guest with `qemu.sh` from this directory. It runs
`$LDD_ROOT/bin/boot.sh`, which already has the `q35` machine with a PCI
Express root port and `-device ldd-pcie` behind it:

```bash
sh qemu.sh
```

## Usage

Copy **load_module.sh** and **pci_skel.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

## test the module

```bash
dmesg | grep pci_skel
lspci -k                                  # 1234:cafe ... pci_skel
ls -l /sys/bus/pci/drivers/pci_skel/      # the bound device, e.g. 0000:01:00.0
grep pci_skel /proc/iomem
sh load_module.sh unload                  # remove() runs
```

The device can also be unbound and bound again without unloading the
module:

```bash
echo 0000:01:00.0 > /sys/bus/pci/drivers/pci_skel/unbind
echo 0000:01:00.0 > /sys/bus/pci/drivers/pci_skel/bind
```

Use the address shown by `lspci`.

---

### ¶ The end
