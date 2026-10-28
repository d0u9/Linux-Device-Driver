# msi_interrupt

Interrupts of a PCI device by MSI-X, MSI or the INTx line, on the `ldd-pcie`
device (`1234:cafe`), a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-pcie.md`](../../tools/qemu/ldd-pcie.md). The driver makes
the device interrupt on request and counts the events it handles, through
sysfs files.

## INTx, MSI and MSI-X

- **INTx** is the original PCI interrupt: a wire per function (INTA# to
  INTD#), routed through bridges to an interrupt controller and shared by
  several devices. It is level-triggered: the device holds it asserted until
  the driver clears the cause, and a handler on a shared line must check
  whether its device is the one interrupting. PCI Express has no wires; it
  sends "assert" and "deassert" messages that the root port turns into the
  old behaviour.
- **MSI** (Message Signalled Interrupt) has no line: the device writes a
  value to a special address, and that memory write is the interrupt. A
  vector belongs to one device and is never shared. Since the device
  itself writes to memory, it must be allowed to master the bus
  (`pci_set_master()`).
- **MSI-X** is MSI with a table of up to 2048 vectors, each with its own
  address and data, kept in one of the device's BARs. Each queue or event
  gets a vector of its own, and so a handler of its own, which can run on
  a CPU of its own.

ldd-pcie has three events, DMA done, DMA error and a software one, each a
bit in the same place of three registers in BAR 0:

| offset | register | meaning |
|--------|----------|---------|
| `0x18` | INT_STATUS | events that happened; write 1 to clear |
| `0x1c` | INT_MASK | 1: the event interrupts; all 0 after reset |
| `0x20` | INT_RAISE | write 1: the event happens, for testing |

With MSI-X event *n* has vector *n*; with MSI or INTx all three share the
one vector.

## what each part shows

- **Choosing the type.** The module parameter `mode` is `msix`, `msi`,
  `intx` or `all`, the default. `pci_alloc_irq_vectors(pdev, min, max,
  types)` enables the first of MSI-X, MSI and INTx in `types` that can
  give at least `min` vectors, and returns how many it got. The driver
  asks for exactly 3 MSI-X vectors, else for 1 of the rest: with 1 or 2
  MSI-X vectors some event would have no vector to go to.
  `pci_irq_vector(pdev, i)` gives the Linux IRQ number of vector `i`,
  which is what `request_irq()` takes.
- **One handler per vector.** With MSI-X, `msi_vec_handler()` knows its
  event from the vector it was called for, and does not read the device.
  Each vector is requested with its own `dev_id`, a `struct irq_vec`, and
  its own name, which shows in `/proc/interrupts`.
- **One handler for all.** With MSI or INTx, `msi_handler()` reads
  INT_STATUS, handles every bit set, and returns `IRQ_NONE` if there is
  none: on a shared INTx line the interrupt may be another device's.
  INTx is requested with `IRQF_SHARED`.
- **Clearing, with any type.** Both handlers write the bits back to
  INT_STATUS. With INTx, a bit left set keeps the line asserted, and the
  handler is called again and again until the kernel disables the line
  ("nobody cared"). With MSI and MSI-X the device sends a message only
  when a bit goes from 0 to 1, so a bit left set loses every later event
  of its kind. That an MSI "needs no acknowledge" is true of the
  interrupt controller, not of the device.
- **Order.** Probe masks and clears the events, requests the IRQs and only
  then unmasks; remove masks first and then frees the IRQs, so the device
  never interrupts without a handler.
- **sysfs.** `pci_driver.dev_groups` adds the files after `probe()` and
  removes them before `remove()`.

## shorter ways of writing the same

The comments in `main.c` name them:

- `pci_request_irq()` and `pci_free_irq()` choose `IRQF_SHARED` by
  themselves, and format the name.
- `pcim_enable_device()`, `pcim_iomap_region()` and `devm_request_irq()`
  are managed: undone automatically when `probe()` fails or after
  `remove()`, so the error path and most of `remove()` go away.
- `module_pci_driver(msi_driver)` generates `m_init()` and `m_exit()`,
  without the check of `mode`.

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

**RUN ON HOST:** build QEMU with the devices from `tools/qemu` (see its
README), then boot the guest with `qemu.sh` from this directory. It runs
`$LDD_ROOT/bin/boot.sh`, which already has the `q35` machine with a PCI
Express root port and `-device ldd-pcie` behind it:

```bash
sh qemu.sh
```

## Usage

Copy **load_module.sh** and **msi_interrupt.ko** to the target machine,
then run:

```bash
sh load_module.sh                   # the first that works: MSI-X
sh load_module.sh load mode=msi
sh load_module.sh load mode=intx
```

## test the module

The files are in the device's directory:

| file | access | meaning |
|------|--------|---------|
| `raise` | write | events to raise: 1 done, 2 error, 4 soft, or a sum |
| `interrupts` | read | events handled, per event |
| `last_status` | read | events seen by the last interrupt |
| `mode` | read | interrupt type and number of vectors |

```bash
D=$(ls -d /sys/bus/pci/drivers/msi_interrupt/0000:*)
cat $D/mode                # msix 3
for v in 1 2 4 7 4; do echo $v > $D/raise; done
cat $D/interrupts          # done 2 error 2 soft 3
grep msi_interrupt /proc/interrupts
```

With MSI-X each event has a vector of its own, and raising 7 makes three
interrupts:

```
 28:          2  PCI-MSIX-0000:01:00.0    0-edge      msi_interrupt-done
 29:          2  PCI-MSIX-0000:01:00.0    1-edge      msi_interrupt-error
 30:          3  PCI-MSIX-0000:01:00.0    2-edge      msi_interrupt-soft
```

With `mode=msi` one vector takes all, and 7 is a single interrupt that
handles three events, so 5 interrupts for the same 7 events:

```
 28:          5  PCI-MSI-0000:01:00.0     0-edge      msi_interrupt
```

With `mode=intx`, the INTx line routed through the IO-APIC, which other
devices may share:

```
 20:          5  IO-APIC                 20-fasteoi   msi_interrupt
```

---

### ¶ The end
