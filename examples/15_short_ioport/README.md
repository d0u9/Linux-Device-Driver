# short_ioport

**short** ("Simple Hardware Operations and Raw Tests") is the series of
examples that drive the emulated `ldd-io` device, a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-io.md`](../../tools/qemu/ldd-io.md). By default it sits at I/O
port `0x200`:

| offset | register | used here as |
|--------|----------|--------------|
| 0      | DATA     | write: eight LEDs, read: received byte |
| 1      | STATUS   | bit 0 RX_READY, write 1 to acknowledge |
| 2      | CTRL     | not used here |
| 8-15   | MATRIX   | 8x8 LED matrix, one byte per row |

This example uses plain port I/O, `inb`/`outb` and friends, and exposes two
device nodes, told apart by the minor number:

| node                       | minor | read                         | write |
|----------------------------|-------|------------------------------|-------|
| `/dev/short_ioport_data`   | 0     | bytes received from the host | bytes to the LEDs |
| `/dev/short_ioport_matrix` | 1     | the 8 matrix rows            | matrix rows, seekable |

What each part shows:

- **Claiming the ports.** `request_region()` reserves the 16 ports, so they
  show up as `short_ioport` in `/proc/ioports` and no other driver can take
  them.
- **One cdev, one `file_operations` per minor.** The cdev covers both minors
  with a tiny `short_fops` whose `open()` looks at `iminor()` and installs
  `data_fops` or `matrix_fops` in the file. After that, `read()`, `write()`
  and `llseek()` go straight to the right functions, so the matrix node can
  seek and the data node has no `llseek` at all. LDD3 picks the behaviour by
  minor number too; `drivers/char/mem.c` uses the same trick for `/dev/null`,
  `/dev/zero` and friends.
- **Byte access and polling.** Receiving waits until STATUS.RX_READY is set,
  reads DATA with `inb()`, then acknowledges with `outb()` to STATUS. The
  wait uses `readx_poll_timeout()` from `<linux/iopoll.h>`, which reads
  STATUS at most 10 ms apart; it is restarted every 100 ms so a signal can
  stop the reader. With `O_NONBLOCK` the read returns `-EAGAIN`.
- **String instructions.** A write to the data node sends the whole buffer to
  the same port with one `outsb()`, a `rep outsb` on x86. The LEDs end up
  showing the last byte.
- **Wider accesses.** Writing all eight matrix rows at once uses two 32-bit
  `outl()` writes; partial writes use `outb()` per row. The device is little
  endian, so the first row is the lowest byte.
- **Seeking.** The matrix node uses `fixed_size_llseek()`, so `dd seek=N`
  can update a single row. The data node is a stream (`stream_open()`).
- **Scoped cleanup.** The mutex is taken with `scoped_cond_guard(mutex_intr,
  ...)` and the write buffer is declared `__free(kfree)`, both from
  `<linux/cleanup.h>`: the lock is dropped and the buffer freed on every way
  out of the scope, without explicit unlock or `kfree()` calls.

## older ways of doing the same

- LDD3 followed every `inb()` and `outb()` with `rmb()` or `wmb()`. The port
  I/O accessors are already ordered, so the barriers are gone.
- LDD3 also showed the "pausing" variants `inb_p()`/`outb_p()`, which add a
  delay for very slow ISA devices; nothing here needs them.
- `register_chrdev()` registered a major with all 256 minors in one call;
  `alloc_chrdev_region()` and `cdev_add()` reserve only the minors used.
- Polling used to be written by hand:

  ```c
  while (!(inb(base + STATUS) & RX_READY)) {
          if (msleep_interruptible(20))
                  return -ERESTARTSYS;
  }
  ```

  `read_poll_timeout()` and `readx_poll_timeout()` replace such loops and
  add a timeout. They sleep uninterruptibly, so a wait that may last forever
  is cut into slices with a signal check in between, as done here.
- Without `<linux/cleanup.h>`, every return path needs its own
  `mutex_unlock()` and `kfree()`, usually through `goto` labels.

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
`$LDD_ROOT/bin/boot.sh` with the ldd-io device in I/O port space and its two
chardevs added:

```bash
sh qemu.sh
```

In two more host terminals, watch the panel and open the keyboard:

```bash
nc 127.0.0.1 7100      # panel
nc 127.0.0.1 7101      # keyboard
```

## Usage

Copy **load_module.sh** and **short_ioport.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

To use another port, e.g. after adding `iobase=0x300` to the device in
`qemu.sh`, load the module by hand with `insmod short_ioport.ko base=0x300`
and create the two nodes as `load_module.sh` does (minor 0 and 1).

## test the module

```bash
grep short_ioport /proc/ioports

# LEDs: the panel shows 0x55 as "○ ● ○ ● ○ ● ○ ●"
printf '\x55' > /dev/short_ioport_data

# receive: type in the keyboard terminal, the bytes appear here
cat /dev/short_ioport_data

# matrix: a whole picture (two outl), then just the third row (outb)
printf '\x3c\x42\xa5\x81\xa5\x99\x42\x3c' > /dev/short_ioport_matrix
printf '\xff' | dd of=/dev/short_ioport_matrix bs=1 seek=2 conv=notrunc
hexdump -C /dev/short_ioport_matrix
```

---

### ¶ The end
