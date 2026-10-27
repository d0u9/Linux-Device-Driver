# msi_interrupt

Interrupts of a PCI device, by MSI or by the INTx line, on QEMU's `edu`
device (`1234:11e8`). The driver makes the device interrupt on request and
counts the interrupts it handles, through three sysfs files.

## INTx and MSI

- **INTx** is the original PCI interrupt: a wire per function (INTA# to
  INTD#), routed through bridges to an interrupt controller and shared by
  several devices. It is level-triggered: the device holds it asserted until
  the driver clears the cause, and a handler on a shared line must check
  whether its device is the one interrupting.
- **MSI** (Message Signalled Interrupt) has no wire: the device writes a
  value to a special address, and that memory write is the interrupt. Each
  vector belongs to one device and needs no check and no ack to stop. Since
  the device itself writes to memory, it must be allowed to master the bus
  (`pci_set_master()`). **MSI-X** is the same with a table of up to 2048
  vectors, so that queues can each have their own; edu has neither MSI-X
  nor more than one MSI vector.

## what each part shows

- **Allocating vectors.** `pci_alloc_irq_vectors(pdev, min, max, types)`
  enables MSI-X, MSI or INTx, whichever of `types` works first, and returns
  how many vectors it got. `pci_irq_vector(pdev, i)` gives the Linux IRQ
  number of vector `i`, which is what `request_irq()` takes. The module
  parameter `msi` chooses `PCI_IRQ_MSI` (default) or `PCI_IRQ_INTX`.
- **Requesting the IRQ.** With INTx, `IRQF_SHARED` is needed, since the line
  may be shared.
- **The handler.** It reads edu's status register and returns `IRQ_NONE` if
  no bit is set, as the interrupt may then belong to another device on the
  line. Otherwise it writes the bits back to the ack register, which lowers
  INTx once none is left, and counts the interrupt. Without the ack, INTx
  would stay asserted and the handler would run again and again until the
  kernel disabled the line.
- **sysfs.** `pci_driver.dev_groups` adds the files after `probe()` and
  removes them before `remove()`.

edu's registers used here:

| offset | access | meaning |
|--------|--------|---------|
| `0x24` | read | interrupt status |
| `0x60` | write | status \|= value, then interrupt |
| `0x64` | write | status &= ~value (ack) |

## shorter ways of writing the same

The comments in `main.c` name them:

- `pci_request_irq()` and `pci_free_irq()` choose `IRQF_SHARED` by
  themselves.
- `pcim_enable_device()`, `pcim_iomap_region()` and `devm_request_irq()`
  are managed: undone automatically when `probe()` fails or after
  `remove()`, so the error path and most of `remove()` go away.
- `module_pci_driver(irq_dev_driver)` generates `m_init()` and `m_exit()`.

## older ways of doing the same

- `pci_enable_msi()`, `pci_enable_msix_range()` and `pci_disable_msi()`;
  `pci_alloc_irq_vectors()` and `pci_free_irq_vectors()` replace them.

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

Copy **load_module.sh** and **msi_interrupt.ko** to the target machine,
then run:

```bash
sh load_module.sh               # MSI
sh load_module.sh load msi=0    # INTx
```

## test the module

```bash
D=$(ls -d /sys/bus/pci/drivers/msi_interrupt/0000:*)
echo 5 > $D/raise
echo 2 > $D/raise
cat $D/interrupts          # 2
cat $D/last_status         # 0x2
grep msi_interrupt /proc/interrupts
```

With MSI, `/proc/interrupts` shows a vector of its own:

```
 28:          2  PCI-MSI-0000:00:06.0     0-edge      msi_interrupt
```

With `msi=0`, the INTx line routed through the IO-APIC, which other devices
may share:

```
 10:          2  IO-APIC                 10-fasteoi   msi_interrupt
```

---

### ¶ The end
