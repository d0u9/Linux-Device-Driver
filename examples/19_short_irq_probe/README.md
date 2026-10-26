# short_irq_probe

The same interrupt handler as a plain short_irq module, but the driver does
not know the interrupt line in advance: it makes the device interrupt once
and lets the kernel tell which line fired. This example drives the emulated
`ldd-io` device, a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-io.md`](../../tools/qemu/ldd-io.md), at I/O port `0x200`.

| offset | register | used here as |
|--------|----------|--------------|
| 0      | DATA     | write: LEDs, read: received byte |
| 1      | STATUS   | bit 0 RX_READY, write 1 to acknowledge |
| 2      | CTRL     | bit 0 IRQ_EN, bit 1 LOOPBACK |

## why probe

ISA devices had their interrupt line set by jumpers on the card. The driver
could not ask the bus; it had to be told with a module parameter or find out
by itself. The kernel helps with `probe_irq_on()` and `probe_irq_off()`:

```c
mask = probe_irq_on();          /* enable all unused lines, return them */
/* make the device interrupt */
irq = probe_irq_off(mask);      /* which one fired? */
```

- `probe_irq_on()` enables every interrupt line that has no handler and may
  be probed, waits about 0.1 s and drops lines that fire on their own. It
  sleeps, so it is called from process context, here from `m_init()`.
- The driver then makes its device interrupt. ldd-io does that when a byte
  is received with IRQ_EN set; with LOOPBACK, writing a byte to DATA is
  enough.
- `probe_irq_off()` returns the line that fired, 0 if none did, or a
  negative number if several did. The module tries five times.

After probing, the device is quieted again (CTRL cleared, byte
acknowledged), and the module continues as usual: `request_irq()` on the
line found, then IRQ_EN and LOOPBACK in CTRL.

`dmesg` shows the result, e.g. `short_irq_probe: using IRQ 5`.

## probing today

Modern buses report the interrupt line themselves: PCI in the config space,
ACPI and the device tree in their tables. A driver bound to such a device
gets its interrupt from the bus code and never probes. `probe_irq_on()` is
still in the kernel for the few remaining legacy drivers, for example the
8250 serial port and some ISA network cards.

Probing only works if the driver can make the device interrupt on its own,
right now, without outside input. Real drivers use whatever the hardware
offers: the 8250 serial driver writes a byte to the transmitter and waits
for the "transmit holding register empty" interrupt, NE2000 network cards
are given a command that completes at once and interrupts. ldd-io raises
interrupts only on receive, so its loopback mode is the way to do it, just
as LDD3 wired an output pin of the parallel port to its interrupt pin.

LDD3 also showed probing by hand: `request_irq()` a probe handler on each
candidate line, make the device interrupt, see which handler ran, and
`free_irq()` them all. The kernel helpers do the same without installing
handlers.

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
`$LDD_ROOT/bin/boot.sh` with the ldd-io device in I/O port space and its
two chardevs added:

```bash
sh qemu.sh
```

To see the probe find another line, add `irq=N` to the device in
`qemu.sh`, with N a line no other device uses.

## Usage

Copy **load_module.sh** and **short_irq_probe.ko** to the target machine,
then run:

```bash
sh load_module.sh
```

For another port, load the module by hand with
`insmod short_irq_probe.ko base=0x300` and create the node as
`load_module.sh` does.

## test the module

```bash
dmesg | grep short_irq_probe       # using IRQ 5
printf abc > /dev/short_irq_probe
cat /dev/short_irq_probe           # irqs 3, last 0x63
grep short_irq_probe /proc/interrupts
```

---

### ¶ The end
