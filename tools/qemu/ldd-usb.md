# ldd-usb

`ldd-usb` is a full-speed USB device with one vendor-specific interface. It
ticks at a settable interval: an interrupt endpoint reports every tick, a
bulk endpoint returns the time of the last tick, and another bulk endpoint
sets the interval.

## Overview

```
      host (driver)                    ldd-usb
                         +----------------------------------+
  bulk OUT EP 2   -----> | interval (seconds) --> timer     |
                         |                          |       |
  interrupt IN EP 3 <--- | tick report <-- tick ----+       |
                         |                          |       |
  bulk IN EP 1    <----- | time of last tick <------+       |
                         +----------------------------------+
```

## Descriptors

| Field | Value |
|-------|-------|
| idVendor:idProduct | `1234:7863` |
| bcdUSB | 1.10, full speed |
| Interface class | 0xff (vendor specific), subclass 0xff, protocol 0xff |
| Endpoints | 3, below |

| Endpoint | Address | Type | Max packet | Direction |
|----------|---------|------|------------|-----------|
| 1 | 0x81 | bulk | 64 | IN |
| 2 | 0x02 | bulk | 64 | OUT |
| 3 | 0x83 | interrupt, bInterval 10 ms | 8 | IN |

Only the standard control requests are supported; anything else stalls.

## Endpoints

### EP 1, bulk IN: time

Returns the host's local time of the last tick as text,
`YYYY-MM-DD HH:MM:SS` followed by a NUL, 20 bytes. A shorter transfer
gets the first bytes. Before the first tick the time is that of the last
reset.

### EP 2, bulk OUT: interval

Takes exactly 4 bytes: the interval in seconds, little endian. Values are
clamped to 1 to 3600. Writing the interval restarts the tick timer and
drops a tick that has not been reported yet. A transfer of any other
length stalls.

### EP 3, interrupt IN: tick

| Offset | Size | Content |
|--------|------|---------|
| 0 | 4 | 1, little endian |
| 4 | 4 | current interval in seconds, little endian |

The device answers with this report once after every tick and NAKs in
between, so a pending interrupt transfer completes once per tick. Ticks are
not counted: if the host does not poll, several ticks collapse into one
report.

## Reset

On reset the interval returns to the `interval` property, the pending tick
is dropped, the time is set to now and the timer restarts.

## Properties

| Property | Default | Meaning |
|----------|---------|---------|
| `interval` | 1 | Interval in seconds after reset |

```bash
-device qemu-xhci -device ldd-usb
-device qemu-xhci -device ldd-usb,interval=5
```

The device needs a USB host controller; `qemu-xhci` is one. The tick
timer runs on the guest's virtual clock. The device cannot be migrated.
