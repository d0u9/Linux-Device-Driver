# usb_skel

The skeleton of a USB driver, after LDD3's usb-skel, bound to the emulated
`ldd-usb` device (`1234:7863`), a QEMU device kept in
[`tools/qemu/hw/ldd`](../../tools/qemu/hw/ldd) and described in
[`tools/qemu/ldd-usb.md`](../../tools/qemu/ldd-usb.md).

ldd-usb ticks at a settable interval and has three endpoints:

| endpoint | type | used for |
|----------|------|----------|
| 0x81 | bulk IN | the time of the last tick, as text |
| 0x02 | bulk OUT | the interval in seconds, 32-bit little endian |
| 0x83 | interrupt IN | one report per tick |

The driver turns that into `/dev/skel0`:

- `read()` waits for the next tick, then returns its time as one line, so
  `cat /dev/skel0` prints a line per tick;
- `write()` of a number, e.g. `echo 5 > /dev/skel0`, sets the interval;
- `poll()` reports the device readable when a tick has not been read yet.

## what each part shows

- **Matching and probing.** `skel_table` lists the vendor and product ID.
  The USB core calls `skel_probe()` once per matching *interface* (a USB
  driver binds to an interface, not to the whole device) and
  `skel_disconnect()` when the device is unplugged or the driver unloaded.
- **Endpoints.** `usb_find_common_endpoints()` picks the bulk IN, bulk OUT
  and interrupt IN endpoints from the interface descriptor.
- **A device node.** `usb_register_dev()` takes a minor from the USB major
  (180) and creates `/dev/skel0`; no `mknod` is needed.
- **Synchronous transfer.** `read()` calls `usb_bulk_msg()`, which submits
  a URB and sleeps until it completes or the timeout (in milliseconds)
  expires.
- **Asynchronous transfer.** `write()` fills a URB with
  `usb_fill_bulk_urb()` and submits it; `write()` returns at once and
  `skel_write_complete()` runs later, in interrupt context, and frees the
  buffer. URB buffers must be DMA-able, so they come from `kmalloc()`,
  never from the stack.
- **Interrupt endpoint.** The interrupt URB is submitted on the first
  `open()`. The device NAKs until the next tick, so the URB completes once
  per tick; `skel_tick_complete()` counts the tick, wakes up readers and
  submits the URB again. The last `close()` kills it. Its buffer is
  allocated once with `usb_alloc_coherent()`, because the URB is reused
  all the time.
- **Unplugging while open.** After `skel_disconnect()` returns, the
  interface is gone, but files may still be open. `struct usb_skel` is
  reference counted with a `kref`: the probe holds one reference and each
  open file another, and the last `kref_put()` frees it. `io_mutex` and
  the `disconnected` flag make `read()` and `write()` fail with `-ENODEV`
  once the device is gone, and wake up waiting readers.
- **Per-file state.** Each open file remembers how many ticks it has
  read, so every reader sees every tick, and `poll()` only looks at that
  count without changing it.

## shorter ways of writing the same

The comments in `main.c` name them:

- `module_usb_driver(skel_driver)` generates `m_init()` and `m_exit()`.
- The kernel's own `drivers/usb/usb-skeleton.c` limits the number of
  writes in flight with a semaphore and tracks them in a `usb_anchor`,
  so that they can all be waited for or killed at once.

## older ways of doing the same

- `usb_class_driver.name` used to be `"usb/skel%d"`, and a udev rule put the
  node in `/dev/usb/`. Now only the part after the last `/` names the
  device, and the node is `/dev/skel0` unless the driver adds a `.devnode`
  callback.
- LDD3 allocated every transfer buffer with `usb_buffer_alloc()`, now
  `usb_alloc_coherent()`. For one-off transfers `kmalloc()` is enough and
  the core does the DMA mapping.
- `__FUNCTION__` is `__func__` now.

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
`$LDD_ROOT/bin/boot.sh`, which already has the `qemu-xhci` controller,
with `-device ldd-usb` added:

```bash
sh qemu.sh
```

`lsusb` in the guest shows `1234:7863`.

## Usage

Copy **load_module.sh** and **usb_skel.ko** to the target machine, then
run:

```bash
sh load_module.sh
```

## test the module

```bash
head -n 3 /dev/skel0          # three ticks, one second apart
echo 3 > /dev/skel0           # tick every three seconds
cat /dev/skel0                # a line every three seconds, Ctrl-C to stop
```

The time is the host's local time, as ldd-usb reports it.

To unplug the device while `cat` is running, use the QEMU monitor
(Ctrl-A C on the serial console started by `boot.sh`):

```
(qemu) device_del usb0          # the id given in qemu.sh
(qemu) device_add ldd-usb,id=usb0
```

`cat` stops with "No such device", `/dev/skel0` disappears, and comes
back when the device is added again.

---

### ¶ The end
