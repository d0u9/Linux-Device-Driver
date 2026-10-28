# bitmap

The kernel's bitmaps, `<linux/bitmap.h>` and `<linux/bitops.h>`. On load
the module sets and clears bits, hands out free slots, finds a run of
free bits, fills the map, allocates a bigger one at run time, and checks
what it sees.

## an array of unsigned long

`DECLARE_BITMAP(slots, 40)` is `unsigned long slots[BITS_TO_LONGS(40)]`:
one long on a 64-bit kernel, two on a 32-bit kernel. The bits are rounded
up to whole longs. The `bitmap_*()` calls take the size in bits and look
no further. A bitmap is the cheapest way to say which of N things are in
use: channels, interrupt vectors, minor numbers, slots in a ring.

## atomic or not

- `set_bit()`, `clear_bit()`, `test_and_set_bit()` are atomic: safe when
  others change other bits of the same word at the same time.
- `__set_bit()`, `__clear_bit()` are a plain read, change and write:
  cheaper, for a bitmap nobody else touches yet, or under a lock.
- The `bitmap_*()` calls on ranges are not atomic either.

Finding a free bit and taking it are two steps: two callers can find the
same zero bit. `take_slot()` claims it with `test_and_set_bit()`, and
looks again if another caller won.

## what each part shows

- **Searching.** `find_first_zero_bit()`, `find_next_bit()`, and
  `for_each_set_bit()` to visit the set ones.
- **Ranges.** `bitmap_find_next_zero_area()` finds a run of free bits,
  `bitmap_set()` and `bitmap_clear()` change a run.
- **The whole map.** `bitmap_weight()` counts, `bitmap_fill()`,
  `bitmap_zero()`, `bitmap_full()`, `bitmap_empty()`.
- **Printing.** `%*pbl` prints ranges, `0-1,3-5`; `%*pb` hex. The width
  is the number of bits.
- **Run-time size.** `bitmap_zalloc()` and `bitmap_free()`.

## output

```
set 0 and 5:               0,5  (hex 00,00000021, 2 set)
4 slots taken:             0-5  (hex 00,0000003f, 6 set)
2 given back:              0-1,3-5  (hex 00,0000003b, 5 set)
8 free in a row from 6
range set:                 0-13  (hex 00,00003fff, 14 set)
full, take_slot: -28
1000 bits allocated: 100-149
```

## where the kernel uses it

- `cpumask_t` and `nodemask_t` are bitmaps of CPUs and NUMA nodes.
- Interrupt vector and MSI allocation; the IDA keeps its IDs as bits.
- Drivers' free-slot maps for channels, queues and descriptors.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## test the module

```bash
insmod bitmap.ko && dmesg | tail -n 20
rmmod bitmap
```

The log ends with `bitmap check passed`.

---

### ¶ The end
