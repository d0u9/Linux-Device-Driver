# pci_skel

The skeleton of a PCI driver, bound to QEMU's built-in `edu` device
(`1234:11e8`, described in `docs/specs/edu.rst` of the QEMU source). The
driver claims and maps the device's memory, reads a few registers and logs
what it finds. It has no device node.

## how a PCI driver gets its device

The module does not touch hardware when it is loaded. `m_init()` only registers a `struct pci_driver` with the PCI core:

```c
static const struct pci_device_id skel_ids[] = {
	{ PCI_DEVICE(0x1234, 0x11e8) },
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
2. `pci_request_regions()` claims the BARs, so no other driver can take
   them; they appear as `pci_skel` in `/proc/iomem`.
3. `pci_iomap()` maps BAR 0 and returns a `void __iomem *`. For a memory
   BAR this is `ioremap()`, for an I/O BAR `ioport_map()`; the driver uses
   `ioread32()`/`iowrite32()` either way.
4. `pci_set_drvdata()` keeps the mapping for `remove()`.

Then it logs:

- vendor, device and revision from the configuration space;
- the interrupt pin, the `PCI_INTERRUPT_LINE` register and `pdev->irq`.
  The register is only a note left by the firmware; `pdev->irq` is the
  interrupt the kernel assigned, and the one drivers use;
- whether the device has a PCI Express capability. `edu` is a
  conventional PCI device, and the default QEMU `pc` machine has no PCI
  Express at all, so this prints "conventional PCI device". A PCI Express
  driver uses the same `pci_*` calls;
- each BAR, as a `struct resource` printed with `%pR`;
- two edu registers in BAR 0: the identification register at 0x00
  (`0x010000ed`, version 1.0) and the liveness register at 0x04, which
  reads back the inverse of what was written.

## shorter ways of writing the same

The example spells things out. The comments in `main.c` name the shorter
forms:

- `module_pci_driver(skel_driver)` generates `m_init()` and `m_exit()`.
- `pcim_enable_device()` and `pcim_iomap_region()` are managed versions of
  enable, request and map: they are undone automatically when the driver
  is unbound, so the `goto` unwinding and most of `remove()` disappear.
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

`$LDD_ROOT/bin/boot.sh` already adds `-device edu`, so the guest started
with it has the device.

## Usage

Copy **load_module.sh** and **pci_skel.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

## test the module

```bash
dmesg | grep pci_skel
lspci -k                                  # 1234:11e8 ... pci_skel
ls -l /sys/bus/pci/drivers/pci_skel/      # the bound device, e.g. 0000:00:04.0
grep pci_skel /proc/iomem
sh load_module.sh unload                  # remove() runs
```

The device can also be unbound and bound again without unloading the
module:

```bash
echo 0000:00:04.0 > /sys/bus/pci/drivers/pci_skel/unbind
echo 0000:00:04.0 > /sys/bus/pci/drivers/pci_skel/bind
```

Use the address shown by `lspci`.

---

### ¶ The end
