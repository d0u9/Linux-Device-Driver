# short_irq

The simplest hardware interrupt handler. This example drives the emulated
`ldd-io` device, a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-io.md`](../../tools/qemu/ldd-io.md), at I/O port `0x200`,
IRQ 5:

| offset | register | used here as |
|--------|----------|--------------|
| 0      | DATA     | write: LEDs, read: received byte |
| 1      | STATUS   | bit 0 RX_READY, write 1 to acknowledge |
| 2      | CTRL     | bit 0 IRQ_EN, bit 1 LOOPBACK, both set on load |

With IRQ_EN set, ldd-io raises an interrupt when a byte has been received
(RX_READY goes from 0 to 1). With LOOPBACK set, every byte written to DATA is
received as well. So writing to `/dev/short_irq` makes the device interrupt
its own driver, without any input from the host. This replaces LDD3's wire
between pin 9 and pin 10 of the parallel port.

## what happens

1. `printf abc > /dev/short_irq`: `short_write()` writes each byte to DATA
   with `outb()`.
2. The device receives the byte, sets RX_READY and raises IRQ 5.
3. The kernel calls `short_interrupt()`, the handler registered with
   `request_irq()`. It reads STATUS, reads the byte from DATA, writes
   RX_READY to STATUS to acknowledge it, counts the interrupt and returns
   `IRQ_HANDLED`.
4. `cat /dev/short_irq` prints the count and the last byte.

The handler returns `IRQ_NONE` when RX_READY is clear: the interrupt did
not come from this device. The kernel uses that to detect interrupts nobody
handles.

## rules for the handler

- It runs in **hard interrupt context**: it interrupted whatever the CPU was
  doing, interrupts are disabled on this CPU, and it must not sleep. No
  `copy_to_user()`, no mutex, no `kmalloc(GFP_KERNEL)`.
- It must be short. Anything slow belongs to later, deferred work.
- It must tell the device the interrupt was handled, here by clearing
  RX_READY, or the device cannot report the next byte.

The handler and `short_read()` share only an `atomic_t` and a byte read with
`READ_ONCE()`, so no lock is needed.

## setting up and tearing down

The handler can run as soon as `request_irq()` returns, and as long as the
device is allowed to interrupt. So:

- `m_init()` claims the ports, registers the handler, adds the cdev, and
  only then sets IRQ_EN in CTRL.
- `m_exit()` first clears CTRL so the device stops interrupting, then calls
  `free_irq()`, which also waits for a handler still running on another
  CPU, and only then removes the rest.

`free_irq()` must be passed the same `dev_id` as `request_irq()`; the
kernel uses it to find the handler to remove.

## older ways of doing the same

- LDD3 took a spinlock with `spin_lock_irqsave()` inside the handler.
  Handlers have run with interrupts disabled since Linux 2.6.35, and this
  one needs no lock at all.
- `register_chrdev()` registered a major with all 256 minors in one call;
  `alloc_chrdev_region()` and `cdev_add()` reserve only the minor used.
- The `rmb()`/`wmb()` after port accesses are gone; port I/O is ordered.

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

Watch the panel from another host terminal; the IRQ line counts the pulses
sent by the device:

```bash
nc 127.0.0.1 7100      # panel
```

While the module is loaded LOOPBACK is set, and input from the keyboard
chardev is held back.

## Usage

Copy **load_module.sh** and **short_irq.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

For another port or interrupt line, load the module by hand, e.g.
`insmod short_irq.ko base=0x300 irq=10`, and create the node as
`load_module.sh` does.

## test the module

```bash
printf abc > /dev/short_irq
cat /dev/short_irq                 # irqs 3, last 0x63
grep short_irq /proc/interrupts    # the kernel's count for IRQ 5
```

The count can be lower than the number of bytes written. A byte written
while the previous one has not been acknowledged yet is dropped by the
device, and one handler call can stand for several pulses.

---

### ¶ The end
