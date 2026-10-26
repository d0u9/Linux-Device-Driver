# ldd-io

`ldd-io` is an ISA device with three 8-bit registers and an 8x8 LED matrix,
16 bytes in all. It drives eight LEDs and the matrix, receives bytes from the
host one at a time and can raise an interrupt when a byte is ready.

## Overview

```
     guest                       ldd-io                      host
                   +--------------------------+
  outb DATA   ---> | output latch ------------+--->  LEDs     (panel)
                   |      |                   |
                   |      v  if LOOPBACK      |
  inb  DATA   <--- | receive buffer <---------+----  bytes    (keyboard)
  inb  STATUS <--- | RX_READY                 |
  outb STATUS ---> | clear RX_READY           |
                   |                          |
  write MATRIX --> | 8x8 matrix --------------+--->  matrix   (panel)
                   |                          |
                   | RX_READY 0->1: pulse ----+--->  ISA IRQ
                   +--------------------------+
```

## Address map

The 16 bytes are mapped either in I/O port space or in memory space, never
both. The `mmio` property selects which (see [Properties](#properties)).
Offsets and behaviour are the same in both mappings.

| Mapping           | Base                    | Driver access                   |
|-------------------|-------------------------|---------------------------------|
| I/O port (default)| `iobase`, default 0x200 | `inb`/`outb`, `ioport_map`      |
| Memory            | `mmiobase`, default 0xff000000 | `ioremap` + `ioread8`/`iowrite8` |

| Offset | Access widths |
|--------|---------------|
| 0-7    | 1 byte |
| 8-15   | 1, 2, 4 or 8 bytes, naturally aligned, little endian |

I/O port space has no 8-byte access, so 8-byte accesses to MATRIX are possible
only with `mmio=on`. Accesses of any other width or alignment are undefined.

## Registers

| Offset | Name   | Access | Reset |
|--------|--------|--------|-------|
| 0      | DATA   | R/W    | 0x00  |
| 1      | STATUS | R/W1C  | 0x00  |
| 2      | CTRL   | R/W    | 0x00  |
| 3-7    | -      | -      | -     |
| 8-15   | MATRIX | R/W    | 0     |

Reserved bits and offsets 3-7 read as 0. Software must write them as 0.
W1C: writing 1 to a bit clears it; writing 0 leaves it unchanged.

### 0: DATA

| Direction | Meaning |
|-----------|---------|
| Write     | Latch the byte into the output latch and update the LEDs. Bit 7 is the leftmost LED, bit 0 the rightmost. With CTRL.LOOPBACK set, the byte also enters the receive path (see [Loopback](#loopback)). |
| Read      | Return the byte in the receive buffer. The value is valid only while STATUS.RX_READY is 1. Reading has no side effect. |

### 1: STATUS

| Bit | Name     | Meaning |
|-----|----------|---------|
| 0   | RX_READY | Read 1: the receive buffer holds a byte that has not been acknowledged. Read 0: no byte is waiting. Write 1: acknowledge the byte. |
| 7:1 | -        | Reserved |

Writing 1 to RX_READY acknowledges the byte in the receive buffer:

1. RX_READY is cleared.
2. The device accepts the next byte from the host, if one is waiting.

Writing 1 to RX_READY while it is 0 has no effect.

**Read DATA before clearing RX_READY.** After it is cleared the buffer may
already hold the next byte.

### 2: CTRL

| Bit | Name     | Meaning |
|-----|----------|---------|
| 0   | IRQ_EN   | 1: send an interrupt pulse when a byte becomes ready (see [Interrupt](#interrupt)). |
| 1   | LOOPBACK | 1: bytes written to DATA are received as input; host input is held back. |
| 7:2 | -        | Reserved |

### 8-15: MATRIX

An 8x8 LED matrix, one byte per row. Offset 8 is the top row and offset 15
the bottom row. In each row bit 7 is the leftmost LED and bit 0 the
rightmost; a 1 lights the LED.

```
offset  8  ○ ○ ● ● ● ● ○ ○   0x3c
offset  9  ○ ● ○ ○ ○ ○ ● ○   0x42
   ...
offset 15  ○ ○ ● ● ● ● ○ ○   0x3c
```

The matrix is redrawn once after every write access, whatever its width. An
8-byte write changes the whole matrix in one step; eight 1-byte writes change
it one row at a time. A read returns the current rows and has no side
effect.

## Receive path

```
            host sends a byte
                   |
                   v
   RX_READY == 0 ? ---- no ----> byte waits in the host side
                   |                    (taken after RX_READY is cleared)
                  yes
                   v
   receive buffer = byte
   RX_READY = 1
```

After RX_READY is cleared the next byte is not necessarily available at once.

## Interrupt

The interrupt is edge triggered. The line is normally low. The device sends
a pulse only when:

1. **A byte becomes ready**: RX_READY goes from 0 to 1 while IRQ_EN is 1.
2. **Interrupts are enabled with a byte waiting**: IRQ_EN goes from 0 to 1
   while RX_READY is 1.

One pulse may stand for several bytes. Rule 2 means that a byte arriving
while IRQ_EN is 0 is signalled when IRQ_EN is set again.

## Loopback

With CTRL.LOOPBACK set, a byte written to DATA:

1. is latched and shown on the LEDs, as usual;
2. becomes the received byte and sets RX_READY, if RX_READY is 0. If
   RX_READY is already 1, the byte is not received (overrun).

While LOOPBACK is set, input from the host is held back. It is delivered
after LOOPBACK is cleared.

## Reset

| State          | Value |
|----------------|-------|
| Output latch   | 0x00 (all LEDs off) |
| MATRIX         | all 0 (all LEDs off) |
| Receive buffer | empty, RX_READY = 0 |
| CTRL           | 0x00 (interrupts and loopback disabled) |
| Interrupt line | low |

## Properties

| Property   | Default    | Meaning |
|------------|------------|---------|
| `iobase`   | 0x200      | I/O port base, used when `mmio=off`; at most 0xfff0 |
| `mmio`     | off        | `on`: map the registers in memory space at `mmiobase` instead of I/O space |
| `mmiobase` | 0xff000000 | Physical address, used when `mmio=on`; 16-byte aligned, must not overlap another device |
| `irq`      | 5          | ISA interrupt number, 0 to 15. IRQ 5 is free on the PC machine; IRQ 7 belongs to the default parallel port |
| `panel`    | none       | Chardev for the [status panel](#status-panel), output only. Without it nothing is shown. |
| `keyboard` | none       | Chardev for the [receive path](#receive-path), input only. Without it no byte is ever received. |

```bash
-chardev socket,id=panel0,path=/tmp/ldd-io-panel.sock,server=on,wait=off \
-chardev socket,id=kbd0,path=/tmp/ldd-io-keyboard.sock,server=on,wait=off \
-device ldd-io,panel=panel0,keyboard=kbd0              # I/O port 0x200, IRQ 5
-device ldd-io,panel=panel0,keyboard=kbd0,mmio=on      # memory 0xff000000
-device ldd-io,panel=panel0,keyboard=kbd0,iobase=0x300,irq=10
```

## Host side

Use one terminal for each chardev. The panel only prints:

```bash
socat -u UNIX-CONNECT:/tmp/ldd-io-panel.sock STDOUT
```

Bytes typed in the keyboard terminal enter the receive path:

```bash
socat - UNIX-CONNECT:/tmp/ldd-io-keyboard.sock             # line by line
socat -,raw,echo=0 UNIX-CONNECT:/tmp/ldd-io-keyboard.sock  # every key at once
printf 'hello\n' | socat - UNIX-CONNECT:/tmp/ldd-io-keyboard.sock
```

### Status panel

The panel is a debugging view provided by the emulation, not part of the
register interface. Its layout may change. It shows:

| Line     | Content |
|----------|---------|
| header   | mapping (`port` or `mmio`) with its base, and the IRQ number |
| DATA out | output latch as eight LEDs |
| DATA in  | byte in the receive buffer, `-` when RX_READY is 0 |
| STATUS   | RX_READY |
| CTRL     | IRQ_EN and LOOPBACK |
| IRQ      | number of interrupt pulses sent since reset |
| MATRIX   | the eight rows |

```
 ldd-io  port 0x200  irq 5
 ────────────────────────────────
 DATA out   ○ ● ○ ● ○ ● ○ ●  0x55
 DATA in    'h'              0x68
 STATUS     RX_READY ●       0x01
 CTRL       IRQ_EN ● LOOP ○  0x01
 IRQ        pulses 3
 ────────────────────────────────
 MATRIX     ○ ○ ● ● ● ● ○ ○  0x3c
            ○ ● ○ ○ ○ ○ ● ○  0x42
            ● ○ ● ○ ○ ● ○ ●  0xa5
            ● ○ ○ ○ ○ ○ ○ ●  0x81
            ● ○ ● ○ ○ ● ○ ●  0xa5
            ● ○ ○ ● ● ○ ○ ●  0x99
            ○ ● ○ ○ ○ ○ ● ○  0x42
            ○ ○ ● ● ● ● ○ ○  0x3c
```

The whole panel is redrawn in place after every state change: every register
write, every write access to MATRIX, every received byte and every interrupt
pulse. A MATRIX write of any width causes exactly one redraw.

The LEDs `●` and `○` are East Asian ambiguous width characters. If the
columns do not line up, set the terminal to draw ambiguous width characters
single width.

## Programming notes

### Write a byte

```c
outb(byte, base + DATA);
```

### Read by polling

```c
while (!(inb(base + STATUS) & RX_READY))
        ;                               /* or sleep and retry */
byte = inb(base + DATA);
outb(RX_READY, base + STATUS);
```

### Read with interrupts

1. Set CTRL.IRQ_EN.
2. On an interrupt, clear CTRL.IRQ_EN.
3. Read bytes as in polling until STATUS.RX_READY is 0.
4. Set CTRL.IRQ_EN. A byte that arrived during step 3 sends a pulse (rule 2).

### Draw on the matrix

```c
writeq(0x3c4299a581a5423cULL, base + 8);   /* whole picture at once, mmio=on */

for (i = 0; i < 8; i++)                    /* one row at a time */
        iowrite8(rows[i], base + 8 + i);

iowrite32(0x000000ff, base + 8);           /* little endian: lights the top row */
```

### Trigger an interrupt without host input

```c
outb(IRQ_EN | LOOPBACK, base + CTRL);
outb(0x00, base + DATA);                /* RX_READY = 1, one pulse */
...
inb(base + DATA);
outb(RX_READY, base + STATUS);          /* RX_READY = 0 */
```
